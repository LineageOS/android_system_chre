/*
 * Copyright (C) 2025 The Android Open Source Project
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

package com.google.android.utils.chre;

import static com.google.common.truth.Truth.assertThat;

import static java.util.concurrent.TimeUnit.SECONDS;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.BluetoothSocketSettings;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.os.ParcelUuid;
import android.util.Log;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;

/**
 * This class is used to connect to the Socket Server and exchange data with it.
 */
public class ContextHubBluetoothSocketClient extends ContextHubBluetoothSocket {
    private static final String TAG = "ContextHubBluetoothSocketClient";

    private Context mContext;
    private boolean mOffload = false;
    private long mOffloadEndpointId = 0;
    private long mHubId = 0;
    private BluetoothManager mBluetoothManager;
    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothLeScanner mScanner;
    private BluetoothGatt mBluetoothGatt;
    private BluetoothDevice mDevice;
    private int mGattConnectionState = BluetoothProfile.STATE_DISCONNECTED;
    private int mPsm = -1;
    private ConnectThread mConnectThread = null;
    private CountDownLatch mAdvertisementFoundBlocker = null;
    private CountDownLatch mGattConnectBlocker = null;
    private CountDownLatch mServiceDiscoveredBlocker = null;
    private CountDownLatch mCharacteristicBlocker = null;
    private CountDownLatch mSocketConnectBlocker = null;

    public ContextHubBluetoothSocketClient(
            Context context, boolean offload, long offloadEndpointId, long hubId) {
        mContext = context;
        mOffload = offload;
        mOffloadEndpointId = offloadEndpointId;
        mHubId = hubId;
        mBluetoothManager = context.getSystemService(BluetoothManager.class);
        assertThat(mBluetoothManager).isNotNull();
        mBluetoothAdapter = mBluetoothManager.getAdapter();
    }

    /**
     * Start the client to connect to the Socket Server.
     *
     * @return true if scanning for the Socket Server was started successfully.
     */
    public boolean startClient() {
        return mBluetoothAdapter.isEnabled()
                && discoverServer()
                && connectToServer()
                && discoverGattService()
                && readPsmCharacteristic()
                && connectSocket();
    }

    private boolean discoverServer() {
        mScanner = mBluetoothAdapter.getBluetoothLeScanner();
        Log.d(TAG, "startScan");
        List<ScanFilter> filter =
                Arrays.asList(
                        new ScanFilter.Builder()
                                .setServiceUuid(
                                        new ParcelUuid(
                                                ContextHubBluetoothSocketServer
                                                        .ADV_SOCKET_UUID))
                                .build());
        ScanSettings setting =
                new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
        mScanner.startScan(filter, setting, mScanCallback);

        mAdvertisementFoundBlocker = new CountDownLatch(1);
        boolean timeout = false;
        try {
            timeout = !mAdvertisementFoundBlocker.await(CALLBACK_TIMEOUT_SEC, SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "", e);
            timeout = true;
        }
        if (timeout) {
            Log.e(TAG, "Did not find server");
        }
        stopScan();
        return !timeout;
    }

    private void stopScan() {
        if (mScanner != null) {
            Log.d(TAG, "stopScan");
            mScanner.stopScan(mScanCallback);
            mScanner = null;
        }
    }

    private final ScanCallback mScanCallback =
            new ScanCallback() {
                @Override
                public void onScanResult(int callbackType, ScanResult result) {
                    if (mBluetoothGatt != null) {
                        return;
                    }
                    // verify the validity of the advertisement packet.
                    boolean isSocketAdvertisement = false;
                    List<ParcelUuid> uuids = result.getScanRecord().getServiceUuids();
                    for (ParcelUuid uuid : uuids) {
                        if (uuid.getUuid()
                                .equals(
                                        ContextHubBluetoothSocketServer
                                                .ADV_SOCKET_UUID)) {
                            Log.d(TAG, "onScanResult: Found ADV with Socket UUID.");
                            isSocketAdvertisement = true;
                            break;
                        }
                    }
                    if (!isSocketAdvertisement) {
                        Log.e(TAG, "onScanResult: No valid service in Advertisement");
                        return;
                    }
                    stopScan();

                    mDevice = result.getDevice();
                    Log.d(
                            TAG,
                            "onScanResult: Found ADV with Socket UUID on device=" + mDevice);
                    mAdvertisementFoundBlocker.countDown();
                }
            };

    private boolean connectToServer() {
        mGattConnectBlocker = new CountDownLatch(1);
        mBluetoothGatt =
                mDevice.connectGatt(mContext, false, mGattCallbacks, BluetoothDevice.TRANSPORT_LE);
        boolean timeout = false;
        try {
            timeout = !mGattConnectBlocker.await(CALLBACK_TIMEOUT_SEC, SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "", e);
            timeout = true;
        }
        if (timeout) {
            Log.e(TAG, "Did not connect to server");
        }
        return !timeout;
    }

    private final BluetoothGattCallback mGattCallbacks =
            new BluetoothGattCallback() {
                @Override
                public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
                    Log.d(
                            TAG,
                            "onConnectionStateChange: status=" + status + ", newState=" + newState);
                    if (status == BluetoothGatt.GATT_SUCCESS) {
                        mGattConnectionState = newState;
                        if (newState == BluetoothProfile.STATE_CONNECTED) {
                            mGattConnectBlocker.countDown();
                        }
                    }
                }

                @Override
                public void onServicesDiscovered(BluetoothGatt gatt, int status) {
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        Log.e(TAG, "GATT service discovery failed with status: " + status);
                        return;
                    }
                    BluetoothGattService service =
                            mBluetoothGatt.getService(
                                    ContextHubBluetoothSocketServer.GATT_SERVICE_UUID);
                    if (service == null) {
                        Log.e(TAG, "GATT service not found");
                        return;
                    }
                    mServiceDiscoveredBlocker.countDown();
                }

                @Override
                public void onCharacteristicRead(
                        BluetoothGatt gatt,
                        BluetoothGattCharacteristic characteristic,
                        byte[] value,
                        int status) {
                    UUID uid = characteristic.getUuid();
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        Log.e(TAG, "Failed to read characteristic: " + status + " : " + uid);
                        return;
                    }
                    if (!characteristic
                            .getUuid()
                            .equals(
                                    ContextHubBluetoothSocketServer
                                            .LE_PSM_CHARACTERISTIC_UUID)) {
                        Log.e(TAG, "onCharacteristicRead: Note: unknown uuid=" + uid);
                        return;
                    }
                    mPsm = (int) (value[0] & 0xFF);
                    Log.d(TAG, "onCharacteristicRead: reading PSM=" + mPsm);
                    mCharacteristicBlocker.countDown();
                }
            };

    /** Start the LE Discovery to find the GATT service on the remote device. */
    private boolean discoverGattService() {
        if (mBluetoothGatt == null || mGattConnectionState != BluetoothProfile.STATE_CONNECTED) {
            Log.e(TAG, "Bluetooth LE GATT not connected.");
            return false;
        }

        mServiceDiscoveredBlocker = new CountDownLatch(1);
        mBluetoothGatt.discoverServices();
        boolean timeout = false;
        try {
            timeout = !mServiceDiscoveredBlocker.await(CALLBACK_TIMEOUT_SEC, SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "", e);
            timeout = true;
        }
        if (timeout) {
            Log.e(TAG, "Did not discover service");
        }
        return !timeout;
    }

    private boolean readPsmCharacteristic() {
        if (mBluetoothGatt == null || mGattConnectionState != BluetoothProfile.STATE_CONNECTED) {
            Log.e(TAG, "Bluetooth LE GATT not connected.");
            return false;
        }
        BluetoothGattService service =
                mBluetoothGatt.getService(ContextHubBluetoothSocketServer.GATT_SERVICE_UUID);
        BluetoothGattCharacteristic characteristic =
                service.getCharacteristic(
                        ContextHubBluetoothSocketServer.LE_PSM_CHARACTERISTIC_UUID);
        if (characteristic == null) {
            Log.e(TAG, "Could not find PSM characteristic");
            return false;
        }
        mCharacteristicBlocker = new CountDownLatch(1);
        mBluetoothGatt.readCharacteristic(characteristic);
        boolean timeout = false;
        try {
            timeout = !mCharacteristicBlocker.await(CALLBACK_TIMEOUT_SEC, SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "", e);
            timeout = true;
        }
        if (timeout) {
            Log.e(TAG, "Did not read characteristic");
        }
        return !timeout;
    }

    /** Start the ConnectThread to initiate a socket connection to a remote device. */
    private boolean connectSocket() {
        if (mConnectThread != null) {
            mConnectThread = null;
        }

        mSocketConnectBlocker = new CountDownLatch(1);
        mConnectThread = new ConnectThread();
        mConnectThread.start();
        boolean timeout = false;
        try {
            timeout = !mSocketConnectBlocker.await(CALLBACK_TIMEOUT_SEC, SECONDS);
        } catch (InterruptedException e) {
            Log.e(TAG, "", e);
            timeout = true;
        }
        if (timeout) {
            Log.e(TAG, "Did not connect to socket");
        }
        return !timeout;
    }

    /**
     * This thread runs while attempting to make an outgoing connection with a device. The
     * connection either succeeds or fails.
     */
    private class ConnectThread extends Thread {
        ConnectThread() {
            try {
                BluetoothSocketSettings.Builder builder =
                        new BluetoothSocketSettings.Builder()
                                .setSocketType(BluetoothSocket.TYPE_LE)
                                .setEncryptionRequired(false)
                                .setL2capPsm(mPsm)
                                .setAuthenticationRequired(false);
                if (mOffload) {
                    builder.setDataPath(BluetoothSocketSettings.DATA_PATH_HARDWARE_OFFLOAD)
                            .setSocketName("ContextHubSocket_L2CAP_COC_Socket")
                            .setHubId(mHubId)
                            .setEndpointId(mOffloadEndpointId)
                            .setRequestedMaximumPacketSize(2048);
                }
                BluetoothSocketSettings settings = builder.build();
                mSocket = mDevice.createUsingSocketSettings(settings);
                Log.d(TAG, "Created L2CAP COC socket using BluetoothSocketSettings API");
            } catch (IOException e) {
                Log.e(TAG, "socket create() failed", e);
            }
        }

        public void run() {
            Log.i(TAG, "BEGIN mConnectThread offload=" + mOffload);
            setName("ConnectThread");

            // Cancel discovery because it will slow down a connection
            mBluetoothAdapter.cancelDiscovery();

            try {
                // This is a blocking call and will only return on a
                // successful connection or an exception
                mSocket.connect();
            } catch (IOException e) {
                Log.e(TAG, "socket connect() failed ", e);
                try {
                    mSocket.close();
                } catch (IOException e2) {
                    Log.e(TAG, "socket unable to close() during connection failure", e2);
                }
                return;
            }

            handleSocketConnected();
            Log.i(TAG, "END mConnectThread offload=" + mOffload);
        }
    }

    private void handleSocketConnected() {
        mConnectThread = null;
        startSocket();
        mSocketConnectBlocker.countDown();
    }
}
