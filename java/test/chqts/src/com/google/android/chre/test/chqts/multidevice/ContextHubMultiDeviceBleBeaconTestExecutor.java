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

package com.google.android.chre.test.chqts.multidevice;

import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.hardware.location.NanoAppBinary;
import android.os.ParcelUuid;
import android.util.Log;

import com.google.android.chre.test.chqts.ContextHubBleTestExecutor;
import com.google.android.utils.chre.ChreApiTestUtil;
import com.google.protobuf.ByteString;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Future;

import dev.chre.rpc.proto.ChreApiTest;

public class ContextHubMultiDeviceBleBeaconTestExecutor extends ContextHubBleTestExecutor {
    private static final String TAG = "ContextHubMultiDeviceBleBeaconTestExecutor";

    private static final int NUM_EVENTS_TO_GATHER_PER_CYCLE = 1000;

    private static final long TIMEOUT_IN_S = 1;

    private static final long TIMEOUT_IN_NS = TIMEOUT_IN_S * 1000000000L;

    private static final long TIMEOUT_IN_MS = 1000;

    private static final int NUM_EVENT_CYCLES_TO_GATHER = 5;

    /**
     * The minimum offset in bytes of a BLE advertisement report which includes the length
     * and type of the report.
     */
    private static final int BLE_ADVERTISEMENT_DATA_HEADER_OFFSET = 2;

    public ContextHubMultiDeviceBleBeaconTestExecutor(NanoAppBinary nanoapp) {
        super(nanoapp);
    }

    /**
     * Gathers BLE advertisement events from the nanoapp for NUM_EVENT_CYCLES_TO_GATHER
     * cycles, and for each cycle gathers for TIMEOUT_IN_NS or up to
     * NUM_EVENTS_TO_GATHER_PER_CYCLE events. This function returns true if all
     * chreBleAdvertisingReport's contain advertisements for Service Data and
     * there is at least one advertisement, otherwise it returns false.
     */
    public List<ChreApiTest.ChreBleAdvertisingReport>
                gatherAndVerifyChreBleAdvertisementsForServiceData() throws Exception {
        List<ChreApiTest.ChreBleAdvertisingReport> reports = new ArrayList<>();

        for (int i = 0; i < NUM_EVENT_CYCLES_TO_GATHER; i++) {
            List<ChreApiTest.GeneralEventsMessage> events = gatherChreBleEvents();
            if (events == null) {
                continue;
            }

            for (ChreApiTest.GeneralEventsMessage event: events) {
                if (!event.hasChreBleAdvertisementEvent()) {
                    continue;
                }

                ChreApiTest.ChreBleAdvertisementEvent bleAdvertisementEvent =
                        event.getChreBleAdvertisementEvent();
                for (int j = 0; j < bleAdvertisementEvent.getReportsCount(); ++j) {
                    ChreApiTest.ChreBleAdvertisingReport report =
                            bleAdvertisementEvent.getReports(j);
                    byte[] data = report.getData().toByteArray();
                    if (data == null || data.length < BLE_ADVERTISEMENT_DATA_HEADER_OFFSET) {
                        continue;
                    }

                    if (searchForServiceDataAdvertisement(data)) {
                        reports.add(report);
                    }
                }
            }
        }
        return reports;
    }

    /**
     * Gathers BLE advertisement events from the nanoapp for NUM_EVENT_CYCLES_TO_GATHER
     * cycles, and for each cycle gathers for TIMEOUT_IN_NS or up to
     * NUM_EVENTS_TO_GATHER_PER_CYCLE events. This function returns true if all
     * chreBleAdvertisingReport's contain advertisements with CHRE test manufacturer ID and
     * there is at least one advertisement, otherwise it returns false.
     */
    public List<ChreApiTest.ChreBleAdvertisingReport>
                gatherAndVerifyChreBleAdvertisementsWithManufacturerData() throws Exception {
        List<ChreApiTest.ChreBleAdvertisingReport> reports = new ArrayList<>();
        for (int i = 0; i < NUM_EVENT_CYCLES_TO_GATHER; i++) {
            List<ChreApiTest.GeneralEventsMessage> events = gatherChreBleEvents();
            if (events == null) {
                continue;
            }

            for (ChreApiTest.GeneralEventsMessage event: events) {
                if (!event.hasChreBleAdvertisementEvent()) {
                    continue;
                }

                ChreApiTest.ChreBleAdvertisementEvent bleAdvertisementEvent =
                        event.getChreBleAdvertisementEvent();
                for (int j = 0; j < bleAdvertisementEvent.getReportsCount(); ++j) {
                    ChreApiTest.ChreBleAdvertisingReport report =
                            bleAdvertisementEvent.getReports(j);
                    byte[] data = report.getData().toByteArray();
                    if (data == null || data.length < BLE_ADVERTISEMENT_DATA_HEADER_OFFSET) {
                        continue;
                    }

                    if (searchForManufacturerAdvertisement(data)) {
                        reports.add(report);
                    }
                }
            }
        }
        return reports;
    }

    /**
     * Gathers CHRE BLE advertisement events.
     */
    private List<ChreApiTest.GeneralEventsMessage> gatherChreBleEvents() throws Exception {
        Future<List<ChreApiTest.GeneralEventsMessage>> eventsFuture =
                new ChreApiTestUtil().gatherEvents(
                        mRpcClients.get(0),
                        Arrays.asList(CHRE_EVENT_BLE_ADVERTISEMENT),
                        NUM_EVENTS_TO_GATHER_PER_CYCLE,
                        TIMEOUT_IN_NS);
        List<ChreApiTest.GeneralEventsMessage> events = eventsFuture.get();
        return events;
    }

    /**
     * Gathers BLE advertisement events received from the Android scan which match
     * the CHRE test Service Data UUID.
     */
    public List<ScanResult>
                gatherAndVerifyAndroidBleAdvertisementsForServiceData() throws Exception {
        List<ScanResult> scanResultsList = new ArrayList<>();
        for (int i = 0; i < NUM_EVENT_CYCLES_TO_GATHER; i++) {
            List<ScanResult> scanResults =
                        gatherAndroidBleEvents(NUM_EVENT_CYCLES_TO_GATHER, TIMEOUT_IN_MS);

            for (ScanResult result : scanResults) {
                ScanRecord record = result.getScanRecord();
                if (record == null) {
                    Log.w(TAG, "ScanRecord was null, skipping result: " + result.toString());
                    continue;
                }
                byte [] serviceData =
                        record.getServiceData(new ParcelUuid(CHRE_TEST_SERVICE_DATA_UUID));
                if (serviceData != null) {
                    scanResultsList.add(result);
                }
            }
        }
        return scanResultsList;
    }

    /**
     * Gathers BLE advertisement events received from the Android scan
     * which match the CHRE test Manufacturer Data ID.
     */
    public List<ScanResult>
                gatherAndVerifyAndroidBleAdvertisementsWithManufacturerData() throws Exception {
        List<ScanResult> scanResultsList = new ArrayList<>();
        for (int i = 0; i < NUM_EVENT_CYCLES_TO_GATHER; i++) {

            List<ScanResult> events =
                            gatherAndroidBleEvents(NUM_EVENT_CYCLES_TO_GATHER, TIMEOUT_IN_MS);

            for (ScanResult result : events) {
                ScanRecord record = result.getScanRecord();
                if (record == null) {
                    continue;
                }
                if (record.getManufacturerSpecificData(CHRE_BLE_TEST_MANUFACTURER_ID) != null) {
                    scanResultsList.add(result);
                }
            }
        }
        return scanResultsList;
    }

    /**
     * Gathers Android BLE advertisement events.
     */
    private List<ScanResult> gatherAndroidBleEvents(int maxEvents,
                                                 long timeoutMillis) throws Exception {
        List<ScanResult> gathered = new ArrayList<>();
        long startTime = System.currentTimeMillis();

        synchronized (mScanResults) {
            Long nowTime = System.currentTimeMillis();
            while ((nowTime - startTime) < timeoutMillis
                                                                && gathered.size() < maxEvents) {

                while (!mScanResults.isEmpty() && gathered.size() < maxEvents) {
                    ScanResult result = mScanResults.remove(0);
                    gathered.add(result);
                }
                if (gathered.size() >= maxEvents) {
                    break;
                }

                long timeLeft = timeoutMillis - (nowTime - startTime);
                if (timeLeft > 0) {
                    try {
                        mScanResults.wait(timeLeft);
                    } catch (InterruptedException e) {
                        Log.e(TAG, "Wait interrupted" + e.getMessage());
                    }
                }
            }
        }
        return gathered;
    }

    /**
     * Starts a BLE scan with the Service Data filter.
     */
    public void chreBleStartScanSyncWithServiceDataFilter() throws Exception {
        chreBleStartScanSync(getTestServiceDataFilterChre());
    }

    /**
     * Starts a BLE scan with test manufacturer data.
     */
    public void chreBleStartScanSyncWithManufacturerData() throws Exception {
        chreBleStartScanSync(getManufacturerDataScanFilterChre());
    }

     /**
     *  Starts an Android BLE scan with Service Data filter.
     */
    public void androidBleStartScanSyncWithServiceDataFilter() throws Exception {
        startBleScanOnHost(getTestServiceDataScanFilterHost());
    }


     /**
     *  Starts an Android BLE scan with Google Manufacturer filter.
     */
    public void androidBleStartScanSyncWithManufacturerData() throws Exception {
        startBleScanOnHost(getManufacturerDataScanFilterHost());
    }

     /**
     * Returns true if the data contains an advertisement for Service Data,
     * otherwise returns false.
     */
    private boolean searchForServiceDataAdvertisement(byte[] data) {
        if (data.length < 2) {
            return false;
        }

        for (int j = 0; j < data.length - 1; ++j) {
            if (Byte.compare(data[j], (byte) 0xCD) == 0
                    && Byte.compare(data[j + 1], (byte) 0xAB) == 0) {
                return true;
            }
        }
        return false;
    }

    /**
     * Returns true if the data contains an advertisement for CHRE test manufacturer data,
     * otherwise returns false.
     */
    private boolean searchForManufacturerAdvertisement(byte[] data) {
        if (data.length < 2) {
            return false;
        }

        for (int j = 0; j < data.length - 1; ++j) {
            if (Byte.compare(data[j], (byte) 0xEE) == 0
                    && Byte.compare(data[j + 1], (byte) 0xEE) == 0) {
                return true;
            }
        }

        return false;
    }

    /**
     * Compares Android and CHRE advertisment
     */
    private boolean verifyBleAdvertisementsMatch(List<ScanResult> androidResults,
                List<ChreApiTest.ChreBleAdvertisingReport> chreReports) throws Exception {

        if (androidResults.isEmpty() || chreReports.isEmpty()) {
            Log.e(TAG, "Advertisment was not received "
                        + "androidResult.size()= " + androidResults.size()
                        + "chreResult.size()= " + chreReports.size());
            return false;
        }

        int minSize = Math.min(androidResults.size(), chreReports.size());
        for (int i = 0; i < minSize; i++) {
            ScanResult androidResult = androidResults.get(i);
            ChreApiTest.ChreBleAdvertisingReport chreReport = chreReports.get(i);

            String androidMac = androidResult.getDevice().getAddress();
            String chreMac = formatMacAddress(chreReport.getAddress());

            if (!androidMac.equalsIgnoreCase(chreMac)) {
                Log.e(TAG, "Mac address mismatch at index " + i
                            + " Android Mac= " + androidMac
                            + " CHRE Mac= " + chreMac);
                return false;
            }
            int androidTxPower = androidResult.getScanRecord().getTxPowerLevel();
            int chreTxPower = chreReport.getTxPower();

            if (androidTxPower != chreTxPower) {
                Log.e(TAG, "Error power at index: " + i);
                return false;
            }
        }

        return true;
    }

    /**
     * Formats the address byte code
     */
    private static String formatMacAddress(ByteString macBytes) {
        byte[] macArray = macBytes.toByteArray();
        StringBuilder macBuilder = new StringBuilder();
        for (int i = 0; i < macArray.length; i++) {
            macBuilder.append(String.format("%02X", macArray[i]));
            if (i != macArray.length - 1) {
                macBuilder.append(":");
            }
        }
        return macBuilder.toString();
    }

    /**
     * Starts Android/CHRE BLE event verification for Service Data
     */
    public boolean verifyAndroidAndChreServiceDataEventsMatch()
                                                        throws Exception {

        List<ScanResult> androidResults =
                    gatherAndVerifyAndroidBleAdvertisementsForServiceData();
        List<ChreApiTest.ChreBleAdvertisingReport> chreResults =
                    gatherAndVerifyChreBleAdvertisementsForServiceData();

        return verifyBleAdvertisementsMatch(androidResults, chreResults);

    }

    /**
     * Starts Android/CHRE BLE event verification for Manufacturer Data
     */
    public boolean verifyAndroidAndChreManufacturerDataEventsMatch()
                                                        throws Exception {

        List<ScanResult> androidResults =
                    gatherAndVerifyAndroidBleAdvertisementsWithManufacturerData();
        List<ChreApiTest.ChreBleAdvertisingReport> chreResults =
                    gatherAndVerifyChreBleAdvertisementsWithManufacturerData();

        return verifyBleAdvertisementsMatch(androidResults, chreResults);

    }
}
