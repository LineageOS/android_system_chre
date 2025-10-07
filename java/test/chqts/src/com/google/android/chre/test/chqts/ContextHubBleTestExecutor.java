/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.android.chre.test.chqts;

import static android.bluetooth.BluetoothDevice.TRANSPORT_LE;

import static com.google.common.truth.Truth.assertThat;

import static java.util.concurrent.TimeUnit.SECONDS;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertisingSet;
import android.bluetooth.le.AdvertisingSetCallback;
import android.bluetooth.le.AdvertisingSetParameters;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.hardware.location.NanoAppBinary;
import android.os.ParcelUuid;
import android.util.Log;

import androidx.test.core.app.ApplicationProvider;

import com.google.android.utils.chre.ChreApiTestUtil;
import com.google.common.collect.ImmutableList;
import com.google.protobuf.ByteString;

import org.junit.Assert;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HexFormat;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;

import dev.chre.rpc.proto.ChreApiTest;

/**
 * A class that contains common BLE functionality using the CHRE API Test nanoapp.
 */
public class ContextHubBleTestExecutor extends ContextHubChreApiTestExecutor {
    private static final String TAG = "ContextHubBleTestExecutor";

    /**
     * The Base UUID is used for calculating 128-bit UUIDs from "short UUIDs" (16- and 32-bit).
     *
     * @see {https://www.bluetooth.com/specifications/assigned-numbers/service-discovery}
     */
    public static final UUID BASE_UUID = UUID.fromString("00000000-0000-1000-8000-00805F9B34FB");

    /**
     * Used for UUID conversion. This is the bit index where the 16-bit UUID is inserted.
     */
    private static final int BIT_INDEX_OF_16_BIT_UUID = 32;

    /**
     * The ID for the Google Eddystone beacon.
     */
    public static final UUID EDDYSTONE_UUID = to128BitUuid((short) 0xFEAA);

    /**
     * The ID for the service data beacon for multi-device test.
     */
    public static final UUID CHRE_TEST_SERVICE_DATA_UUID = to128BitUuid((short) 0xABCD);

    /**
     * The delay to report results in milliseconds.
     */
    private static final int REPORT_DELAY_MS = 0;

    /**
     * The RSSI threshold for the BLE scan filter.
     */
    private static final int RSSI_THRESHOLD = -128;

    /**
     * The advertisement type for service data.
     */
    public static final int CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16 = 0x16;

    /**
     * The advertisement type for manufacturer data.
     */
    public static final int CHRE_BLE_AD_TYPE_MANUFACTURER_DATA = 0xFF;

    /**
     * The BLE advertisement event ID.
     */
    public static final int CHRE_EVENT_BLE_ADVERTISEMENT = 0x0350 + 1;

    /**
     * CHRE BLE test manufacturer ID.
     */
    public static final int CHRE_BLE_TEST_MANUFACTURER_ID = 0xEEEE;

    /**
     * For requesting RSSI read from CHRE
     */
    public static final int CHRE_MESSAGE_READ_RSSI = 0x16;

    /**
     * Call back timeout.
     */
    private static final int CALLBACK_TIMEOUT_SEC = 5;

    /**
     * RSSI value indicating no RSSI value available.
     */
    private static final int CHRE_BLE_RSSI_NONE = 127;

    private BluetoothAdapter mBluetoothAdapter = null;
    private BluetoothLeAdvertiser mBluetoothLeAdvertiser = null;
    private BluetoothLeScanner mBluetoothLeScanner = null;
    private BluetoothManager mBluetoothManager;
    private BluetoothGattServer mBluetoothGattServer;
    private BluetoothDevice mServer;
    private BluetoothGatt mBluetoothGatt;
    private Context mContextGatt;
    private CountDownLatch mConnectionBlocker = null;
    private Integer mWaitForConnectionState = null;

    /**
     * The signal for advertising start.
     */
    private CountDownLatch mAdvertisingStartLatch = new CountDownLatch(1);

    /**
     * The signal for advertising stop.
     */
    private CountDownLatch mAdvertisingStopLatch = new CountDownLatch(1);

    /**
     * Indicates whether the device is advertising or not.
     */
    private AtomicBoolean mIsAdvertising = new AtomicBoolean();

    /**
     * List to store BLE scan received by host.
     */
    public final List<ScanResult> mScanResults = Collections.synchronizedList(new ArrayList<>());

    /**
     * Callback for BLE scans.
     */
    private final ScanCallback mScanCallback = new ScanCallback() {
        @Override
        public void onBatchScanResults(List<ScanResult> results) {
            // do nothing
        }

        @Override
        public void onScanFailed(int errorCode) {
            Assert.fail("Failed to start a BLE scan on the host");
        }

        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            if (result == null || result.getDevice() == null) {
                Log.w(TAG, "ScanResult or device was null");
                return;
            }
            synchronized (mScanResults) {
                mScanResults.add(result);
            }
        }
    };

    /**
     * The BLE advertising callback. It updates the mIsAdvertising state
     * and notifies any waiting threads that the state of advertising
     * has changed.
     */
    private AdvertisingSetCallback mAdvertisingSetCallback = new AdvertisingSetCallback() {
        @Override
        public void onAdvertisingSetStarted(AdvertisingSet advertisingSet,
                int txPower, int status) {
            mIsAdvertising.set(true);
            mAdvertisingStartLatch.countDown();
        }

        @Override
        public void onAdvertisingDataSet(AdvertisingSet advertisingSet, int status) {
            // do nothing
        }

        @Override
        public void onScanResponseDataSet(AdvertisingSet advertisingSet, int status) {
            // do nothing
        }

        @Override
        public void onAdvertisingSetStopped(AdvertisingSet advertisingSet) {
            mIsAdvertising.set(false);
            mAdvertisingStopLatch.countDown();
        }
    };

    public ContextHubBleTestExecutor(NanoAppBinary nanoapp) {
        super(nanoapp);

        BluetoothManager bluetoothManager = mContext.getSystemService(BluetoothManager.class);
        assertThat(bluetoothManager).isNotNull();
        mBluetoothAdapter = bluetoothManager.getAdapter();
        mContextGatt = ApplicationProvider.getApplicationContext();
        if (mBluetoothAdapter != null) {
            mBluetoothLeAdvertiser = mBluetoothAdapter.getBluetoothLeAdvertiser();
            mBluetoothLeScanner = mBluetoothAdapter.getBluetoothLeScanner();
        }
    }

    /**
     * Creates a new GATT server and registers a primary service with the give UUID.
     */
    public BluetoothGattServer createGattServer(String expectedMacAddress) {
        if (mContextGatt == null) {
            Log.e(TAG, "Context is null.");
            return null;
        }
        mBluetoothManager =
                    mContextGatt.getSystemService(BluetoothManager.class);
        if (mBluetoothManager == null) {
            Log.e(TAG, "BluetoothManager is null");
            return null;
        }

        mBluetoothGattServer =  mBluetoothManager.openGattServer(
                        mContextGatt, new BluetoothGattServerCallback() {});

        return mBluetoothGattServer;
    }

    /**
     * Retrieves the list of currently connected GATT client devices.
     */
    public List<BluetoothDevice> getConnectedDevices() {
        return mBluetoothManager.getConnectedDevices(BluetoothProfile.GATT_SERVER);
    }

    /**
     * Callback for Gatt client connection events.
     */
    private final BluetoothGattCallback mGattCallback =
            new BluetoothGattCallback() {
                @Override
                public void onConnectionStateChange(BluetoothGatt device,
                        int status, int newState) {

                        if (newState == mWaitForConnectionState && mConnectionBlocker != null) {
                            Log.v(TAG, "Connected");
                            mConnectionBlocker.countDown();
                        }
                    }
            };

    /**
     * Connects to a BLE server with a know MAC address, initates a GATT connection,
     * and reads the remote RSSI value after connection is established.
     */
    public BluetoothDevice connect(String expectedMacAddress) {
        var serverFoundBlocker = new CountDownLatch(1);
        var scanner = mBluetoothAdapter.getBluetoothLeScanner();
        var callback = new ScanCallback() {
                    @Override
                    public void onScanResult(int callbackType, ScanResult result) {
                        var deviceAddress = result.getDevice().getAddress();
                        if (deviceAddress != null
                                && deviceAddress.equalsIgnoreCase(expectedMacAddress)) {
                            mServer = result.getDevice();
                            serverFoundBlocker.countDown();
                        }
                    }
                };
        scanner.startScan(null,
             new ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setLegacy(false)
                .build(), callback);
        boolean timeout = false;
        try {
            timeout = !serverFoundBlocker.await(CALLBACK_TIMEOUT_SEC, SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "", e);
            timeout = true;
        }
        scanner.stopScan(callback);
        if (timeout) {
            Log.e(TAG, "Did not discover server");
            return null;
        }
        mConnectionBlocker = new CountDownLatch(1);
        mWaitForConnectionState = BluetoothProfile.STATE_CONNECTED;
        mBluetoothGatt = mServer.connectGatt(mContextGatt, false, mGattCallback, TRANSPORT_LE);
        timeout = false;
        try {
            timeout = !mConnectionBlocker.await(CALLBACK_TIMEOUT_SEC, SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "", e);
            timeout = true;
        }
        if (timeout) {
            Log.e(TAG, "Did not connect to server");
            return null;
        }

        return mServer;
    }

    /**
     * Disconnects from the GATT server.
     */
    public boolean disconnect(String expectedMacAddress) {
        if (mServer == null || !mServer.getAddress().equalsIgnoreCase(expectedMacAddress)) {
            Log.e(TAG, "Connected server does not match expected MAC address: "
                        + expectedMacAddress);
            return false;
        }

        mConnectionBlocker = new CountDownLatch(1);
        mWaitForConnectionState = BluetoothProfile.STATE_DISCONNECTED;
        mBluetoothGatt.disconnect();

        boolean timeout = false;
        try {
            timeout = !mConnectionBlocker.await(CALLBACK_TIMEOUT_SEC, SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "", e);
            timeout = true;
        }
        if (timeout) {
            Log.e(TAG, "Did not disconnect from server");
            return false;
        }

        return true;
    }

    /**
     * Sends a read RSSI request to CHRE
     */
    public boolean sendReadRssiMessageToChre(int connectionHandle) throws Exception {
        if (connectionHandle <= 0) {
            Log.e(TAG, "Invalid connection handle");
        }

        ChreApiTest.ChreBleReadRssiRequest.Builder message =
                ChreApiTest.ChreBleReadRssiRequest.newBuilder()
                            .setConnectionHandle(connectionHandle);

        ChreApiTestUtil util = new ChreApiTestUtil();
        List<ChreApiTest.ChreBleReadRssiEvent> response =
                util.callServerStreamingRpcMethodSync(getRpcClient(),
                        "chre.rpc.ChreApiTestService.ChreBleReadRssiSync",
                        message.build());
        assertThat(response).isNotEmpty();
        for (ChreApiTest.ChreBleReadRssiEvent status: response) {
            assertThat(status.getStatus()).isTrue();
            assertThat(status.getRssi()).isNotEqualTo(CHRE_BLE_RSSI_NONE);
        }
        return true;
    }

    /**
     * Generates a BLE scan filter that filters only for the known Google beacons:
     * Google Eddystone and Nearby Fastpair.
     *
     * @param useEddystone          if true, filter for Google Eddystone.
     * @param useNearbyFastpair     if true, filter for Nearby Fastpair.
     * @param useChreTestServiceData if true, filter for Test Service Data.
     */
    public static ChreApiTest.ChreBleScanFilter getDefaultScanFilter(boolean useEddystone,
            boolean useNearbyFastpair, boolean useChreTestServiceData) {
        ChreApiTest.ChreBleScanFilter.Builder builder =
                ChreApiTest.ChreBleScanFilter.newBuilder()
                        .setRssiThreshold(RSSI_THRESHOLD);

        if (useEddystone) {
            ChreApiTest.ChreBleGenericFilter eddystoneFilter =
                    ChreApiTest.ChreBleGenericFilter.newBuilder()
                            .setType(CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16)
                            .setLength(2)
                            .setData(ByteString.copyFrom(HexFormat.of().parseHex("AAFE")))
                            .setMask(ByteString.copyFrom(HexFormat.of().parseHex("FFFF")))
                            .build();
            builder = builder.addScanFilters(eddystoneFilter);
        }

        if (useNearbyFastpair) {
            ChreApiTest.ChreBleGenericFilter nearbyFastpairFilter =
                    ChreApiTest.ChreBleGenericFilter.newBuilder()
                            .setType(CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16)
                            .setLength(2)
                            .setData(ByteString.copyFrom(HexFormat.of().parseHex("2CFE")))
                            .setMask(ByteString.copyFrom(HexFormat.of().parseHex("FFFF")))
                            .build();
            builder = builder.addScanFilters(nearbyFastpairFilter);
        }

        if (useChreTestServiceData) {
            ChreApiTest.ChreBleGenericFilter eddystoneFilter =
                    ChreApiTest.ChreBleGenericFilter.newBuilder()
                            .setType(CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16)
                            .setLength(2)
                            .setData(ByteString.copyFrom(HexFormat.of().parseHex("CDAB")))
                            .setMask(ByteString.copyFrom(HexFormat.of().parseHex("FFFF")))
                            .build();
            builder = builder.addScanFilters(eddystoneFilter);
        }

        return builder.build();
    }

    /**
     * Generates a BLE scan filter that filters only for the CHRE test manufacturer ID.
     */
    public static ChreApiTest.ChreBleScanFilter getManufacturerDataScanFilterChre() {
        ChreApiTest.ChreBleScanFilter.Builder builder =
                ChreApiTest.ChreBleScanFilter.newBuilder()
                        .setRssiThreshold(RSSI_THRESHOLD);
        ChreApiTest.ChreBleGenericFilter manufacturerFilter =
                ChreApiTest.ChreBleGenericFilter.newBuilder()
                        .setType(CHRE_BLE_AD_TYPE_MANUFACTURER_DATA)
                        .setLength(2)
                        .setData(ByteString.copyFrom(HexFormat.of().parseHex("EEEE")))
                        .setMask(ByteString.copyFrom(HexFormat.of().parseHex("FFFF")))
                        .build();
        builder = builder.addScanFilters(manufacturerFilter);

        return builder.build();
    }

    /**
     * Generates a Filter that only known Broadcaster Address
     */
    public static ChreApiTest.ChreBleScanFilter getBroadcasterAddressFilter(String macAddress) {
        byte[] macBytes = parseMacStringToBytes(macAddress);

        ChreApiTest.ChreBleBroadcasterAddressFilter broadcasterAddressFilter =
                ChreApiTest.ChreBleBroadcasterAddressFilter.newBuilder()
                        .setBroadcasterAddress(ByteString.copyFrom(macBytes))
                        .build();

        ChreApiTest.ChreBleScanFilter.Builder builder =
                ChreApiTest.ChreBleScanFilter.newBuilder()
                        .setRssiThreshold(RSSI_THRESHOLD)
                        .addBroadcasterAddressFilters(broadcasterAddressFilter);
        return builder.build();
    }

    /**
     * Parses the Mac String to Bytes
     */
    public static byte[] parseMacStringToBytes(String mac) {
        String[] parts = mac.split(":");
        byte[] macBytes = new byte[parts.length];
        for (int i = 0; i < parts.length; i++) {
            macBytes[i] = (byte) Integer.parseInt(parts[i], 16);
        }
        return macBytes;
    }

    /**
     * Generates a BLE scan filter that filters only for the known Google beacons:
     * Google Eddystone and Nearby Fastpair.
     */
    public static ChreApiTest.ChreBleScanFilter getGoogleServiceDataScanFilterChre() {
        return getDefaultScanFilter(true /* useEddystone */, true /* useNearbyFastpair */,
                    false /* useChreTestServiceData */);
    }

    /**
     * Generates a BLE scan filter that filters for a test service data UUID.
     */
    public static ChreApiTest.ChreBleScanFilter getTestServiceDataFilterChre() {
        return getDefaultScanFilter(false /* useEddystone */, false /* useNearbyFastpair */,
                    true /* useChreTestServiceData */);
    }

    /**
     * Generates a BLE scan filter that filters only for the known Google beacons:
     * Google Eddystone and Nearby Fastpair. We specify the filter data in (little-endian) LE
     * here as the CHRE code will take BE input and transform it to LE.
     */
    public static List<ScanFilter> getGoogleServiceDataScanFilterHost() {
        assertThat(CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16)
                .isEqualTo(ScanRecord.DATA_TYPE_SERVICE_DATA_16_BIT);

        ScanFilter scanFilter = new ScanFilter.Builder()
                .setAdvertisingDataTypeWithData(
                        ScanRecord.DATA_TYPE_SERVICE_DATA_16_BIT,
                        ByteString.copyFrom(HexFormat.of().parseHex("AAFE")).toByteArray(),
                        ByteString.copyFrom(HexFormat.of().parseHex("FFFF")).toByteArray())
                .build();
        ScanFilter scanFilter2 = new ScanFilter.Builder()
                .setAdvertisingDataTypeWithData(
                        ScanRecord.DATA_TYPE_SERVICE_DATA_16_BIT,
                        ByteString.copyFrom(HexFormat.of().parseHex("2CFE")).toByteArray(),
                        ByteString.copyFrom(HexFormat.of().parseHex("FFFF")).toByteArray())
                .build();

        return ImmutableList.of(scanFilter, scanFilter2);
    }

    /**
     * Generates a BLE scan filter for a Test Service Data UUID.
     * We specify the filter data in (little-endian) LE
     * here as the CHRE code will take BE input and transform it to LE.
     */
    public static List<ScanFilter> getTestServiceDataScanFilterHost() {
        assertThat(CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16)
                .isEqualTo(ScanRecord.DATA_TYPE_SERVICE_DATA_16_BIT);

        ScanFilter scanFilter = new ScanFilter.Builder()
                .setAdvertisingDataTypeWithData(
                        ScanRecord.DATA_TYPE_SERVICE_DATA_16_BIT,
                        ByteString.copyFrom(HexFormat.of().parseHex("CDAB")).toByteArray(),
                        ByteString.copyFrom(HexFormat.of().parseHex("FFFF")).toByteArray())
                .build();

        return ImmutableList.of(scanFilter);
    }

    /**
     * Generates a BLE scan filter that filters only for the known CHRE test specific
     * manufacturer ID.
     */
    public static List<ScanFilter> getManufacturerDataScanFilterHost() {
        assertThat(CHRE_BLE_AD_TYPE_MANUFACTURER_DATA)
                .isEqualTo(ScanRecord.DATA_TYPE_MANUFACTURER_SPECIFIC_DATA);

        ScanFilter scanFilter = new ScanFilter.Builder()
                .setAdvertisingDataTypeWithData(
                        ScanRecord.DATA_TYPE_MANUFACTURER_SPECIFIC_DATA,
                        ByteString.copyFrom(HexFormat.of().parseHex("EEEE")).toByteArray(),
                        ByteString.copyFrom(HexFormat.of().parseHex("FFFF")).toByteArray())
                .build();

        return ImmutableList.of(scanFilter);
    }

    /**
     * Generates a BLE scan filter that filters only for the known broadcaster MAC address
     */
    public List<ScanFilter> getBroadcastAddressFilterHost(String macAddress) {
        ScanFilter scanFilter = new ScanFilter.Builder()
                .setDeviceAddress(macAddress)
                .build();

        return ImmutableList.of(scanFilter);
    }

    /**
     * Starts a BLE scan and asserts it was started successfully in a synchronous manner.
     * This waits for the event to be received and returns the status in the event.
     *
     * @param scanFilter                The scan filter.
     */
    public void chreBleStartScanSync(ChreApiTest.ChreBleScanFilter scanFilter) throws Exception {
        ChreApiTest.ChreBleStartScanAsyncInput.Builder inputBuilder =
                ChreApiTest.ChreBleStartScanAsyncInput.newBuilder()
                        .setMode(ChreApiTest.ChreBleScanMode.CHRE_BLE_SCAN_MODE_FOREGROUND)
                        .setReportDelayMs(REPORT_DELAY_MS)
                        .setHasFilter(true);
        if (scanFilter != null) {
            inputBuilder.setFilter(scanFilter);
        }

        ChreApiTestUtil util = new ChreApiTestUtil();
        List<ChreApiTest.GeneralSyncMessage> response =
                util.callServerStreamingRpcMethodSync(getRpcClient(),
                        "chre.rpc.ChreApiTestService.ChreBleStartScanSync",
                        inputBuilder.build());
        assertThat(response).isNotEmpty();
        for (ChreApiTest.GeneralSyncMessage status: response) {
            assertThat(status.getStatus()).isTrue();
        }
    }

    /**
     * Stops a BLE scan and asserts it was started successfully in a synchronous manner.
     * This waits for the event to be received and returns the status in the event.
     */
    public void chreBleStopScanSync() throws Exception {
        ChreApiTestUtil util = new ChreApiTestUtil();
        List<ChreApiTest.GeneralSyncMessage> response =
                util.callServerStreamingRpcMethodSync(getRpcClient(),
                        "chre.rpc.ChreApiTestService.ChreBleStopScanSync");
        assertThat(response).isNotEmpty();
        for (ChreApiTest.GeneralSyncMessage status: response) {
            assertThat(status.getStatus()).isTrue();
        }
    }

    /**
     * Returns true if the required BLE capabilities and filter capabilities exist, otherwise
     * returns false.
     */
    public boolean doNecessaryBleCapabilitiesExist(
                int requiredCapabilities, int requiredFilterCapabilities)throws Exception {
        if (mBluetoothLeScanner == null) {
            return false;
        }

        ChreApiTest.Capabilities capabilitiesResponse =
                ChreApiTestUtil.callUnaryRpcMethodSync(getRpcClient(),
                        "chre.rpc.ChreApiTestService.ChreBleGetCapabilities");

        int capabilities = capabilitiesResponse.getCapabilities();
        Log.d(TAG, String.format("CHRE Capabilitites = 0x%08X, Required = 0x%08X",
                                    capabilities, requiredCapabilities));
        if ((capabilities & requiredCapabilities) != requiredCapabilities) {
            return false;
        }
        if (requiredFilterCapabilities != 0) {
            ChreApiTest.Capabilities filterCapabilitiesResponse =
                    ChreApiTestUtil.callUnaryRpcMethodSync(getRpcClient(),
                            "chre.rpc.ChreApiTestService.ChreBleGetFilterCapabilities");
            int filterCapabilities = filterCapabilitiesResponse.getCapabilities();
            Log.d(TAG, String.format("CHRE FilterCapabilitites = 0x%08X, Required = 0x%08X",
                                        filterCapabilities, requiredFilterCapabilities));
            return (filterCapabilities & requiredFilterCapabilities) == requiredFilterCapabilities;
        }
        return true;
    }

    /**
     * Returns true if the required BLE capabilities and filter capabilities exist on the AP,
     * otherwise returns false.
     */
    public boolean doNecessaryBleCapabilitiesExistOnTheAP() throws Exception {
        return mBluetoothLeAdvertiser != null;
    }

    /**
     * Starts a BLE scan on the host side with the given scan filters.
     */
    public void startBleScanOnHost(List<ScanFilter> scanFilter) throws Exception {
        if (scanFilter == null) {
            throw new IllegalAccessException("Scan filters must not be empty or null");
        }
        ScanSettings scanSettings = new ScanSettings.Builder()
                .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build();
        mBluetoothLeScanner.startScan(scanFilter,
                scanSettings, mScanCallback);
    }

    /**
     * Stops a BLE scan on the host side.
     */
    public void stopBleScanOnHost() {
        mBluetoothLeScanner.stopScan(mScanCallback);
    }

    /**
     * Starts broadcasting the Google Eddystone beacon from the AP.
     */
    public void startBleAdvertisingGoogleEddystone() throws InterruptedException {
        if (mIsAdvertising.get()) {
            return;
        }

        AdvertisingSetParameters parameters = new AdvertisingSetParameters.Builder()
                .setLegacyMode(true)
                .setConnectable(false)
                .setInterval(AdvertisingSetParameters.INTERVAL_LOW)
                .setTxPowerLevel(AdvertisingSetParameters.TX_POWER_MEDIUM)
                .build();

        AdvertiseData data = new AdvertiseData.Builder()
                .addServiceData(new ParcelUuid(EDDYSTONE_UUID), new byte[] {0})
                .setIncludeDeviceName(false)
                .setIncludeTxPowerLevel(true)
                .build();

        mBluetoothLeAdvertiser.startAdvertisingSet(parameters, data,
                /* ownAddress= */ null, /* periodicParameters= */ null,
                /* periodicData= */ null, mAdvertisingSetCallback);
        mAdvertisingStartLatch.await();
        assertThat(mIsAdvertising.get()).isTrue();
    }

    /**
     * Starts broadcasting the CHRE test Service Data beacon from the AP.
     */
    public void startBleAdvertisingTestServiceData() throws InterruptedException {
        if (mIsAdvertising.get()) {
            return;
        }

        AdvertisingSetParameters parameters = new AdvertisingSetParameters.Builder()
                .setLegacyMode(true)
                .setConnectable(false)
                .setInterval(AdvertisingSetParameters.INTERVAL_LOW)
                .setTxPowerLevel(AdvertisingSetParameters.TX_POWER_MEDIUM)
                .build();

        AdvertiseData data = new AdvertiseData.Builder()
                .addServiceData(new ParcelUuid(CHRE_TEST_SERVICE_DATA_UUID), new byte[] {0})
                .setIncludeDeviceName(false)
                .setIncludeTxPowerLevel(true)
                .build();

        mBluetoothLeAdvertiser.startAdvertisingSet(parameters, data,
                /* ownAddress= */ null, /* periodicParameters= */ null,
                /* periodicData= */ null, mAdvertisingSetCallback);
        mAdvertisingStartLatch.await();
        assertThat(mIsAdvertising.get()).isTrue();
    }

    /**
     * Starts broadcasting BLE advertising with no data
     */
    public void startBleAdvertisingWithNoData() throws InterruptedException {
        if (mIsAdvertising.get()) {
            return;
        }

        AdvertisingSetParameters parameters = new AdvertisingSetParameters.Builder()
                .setLegacyMode(true)
                .setConnectable(false)
                .setInterval(AdvertisingSetParameters.INTERVAL_LOW)
                .setTxPowerLevel(AdvertisingSetParameters.TX_POWER_MEDIUM)
                .setOwnAddressType(AdvertisingSetParameters.ADDRESS_TYPE_PUBLIC)
                .build();

        AdvertiseData data = new AdvertiseData.Builder()
                .setIncludeDeviceName(false)
                .setIncludeTxPowerLevel(true)
                .build();

        mBluetoothLeAdvertiser.startAdvertisingSet(parameters, data,
                 /* ownAddress= */ null, /* periodicParameters= */ null,
                /* periodicData= */ null, mAdvertisingSetCallback);
        mAdvertisingStartLatch.await();
        assertThat(mIsAdvertising.get()).isTrue();
    }

    /**
     * Starts broadcasting the CHRE test manufacturer Data from the AP.
     */
    public void startBleAdvertisingManufacturer() throws InterruptedException {
        if (mIsAdvertising.get()) {
            return;
        }

        AdvertisingSetParameters parameters = new AdvertisingSetParameters.Builder()
                .setLegacyMode(true)
                .setConnectable(false)
                .setInterval(AdvertisingSetParameters.INTERVAL_LOW)
                .setTxPowerLevel(AdvertisingSetParameters.TX_POWER_MEDIUM)
                .build();

        AdvertiseData data = new AdvertiseData.Builder()
                .addManufacturerData(CHRE_BLE_TEST_MANUFACTURER_ID, new byte[] {0})
                .setIncludeDeviceName(false)
                .setIncludeTxPowerLevel(true)
                .build();

        mBluetoothLeAdvertiser.startAdvertisingSet(parameters, data,
                /* ownAddress= */ null, /* periodicParameters= */ null,
                /* periodicData= */ null, mAdvertisingSetCallback);
        mAdvertisingStartLatch.await();
        assertThat(mIsAdvertising.get()).isTrue();
    }

    /**
     * Stops advertising data from the AP.
     */
    public void stopBleAdvertising() throws InterruptedException {
        if (!mIsAdvertising.get()) {
            return;
        }

        mBluetoothLeAdvertiser.stopAdvertisingSet(mAdvertisingSetCallback);
        mAdvertisingStopLatch.await();
        assertThat(mIsAdvertising.get()).isFalse();
    }

    /**
     * Converts short UUID to 128 bit UUID.
     */
    private static UUID to128BitUuid(short shortUuid) {
        return new UUID(
                ((shortUuid & 0xFFFFL) << BIT_INDEX_OF_16_BIT_UUID)
                        | BASE_UUID.getMostSignificantBits(),
                        BASE_UUID.getLeastSignificantBits());
    }

    /**
     * Gets the mac address for the device
     */
    public String getMacAddress() throws Exception {
        if (mBluetoothAdapter == null) {
            Log.e(TAG, "getMacAddress mBluetoothAdapter is equal to null");
        }

        return mBluetoothAdapter.getAddress();
    }
}
