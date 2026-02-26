/*
 * Copyright (C) 2026 The Android Open Source Project
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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.location.NanoAppBinary;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.os.SystemClock;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import com.google.android.utils.chre.ChreTestUtil;
import com.google.android.utils.chre.SettingsUtil;

import org.junit.Assert;
import org.junit.Assume;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import dev.chre.rpc.proto.ChreApiTest.Capabilities;
import dev.chre.rpc.proto.ChreApiTest.ChreWifiConfigureScanMonitorAsyncInput;
import dev.chre.rpc.proto.ChreApiTest.ChreWifiScanEvent;
import dev.chre.rpc.proto.ChreApiTest.ChreWifiScanResult;
import dev.chre.rpc.proto.ChreApiTest.GatherEventsInput;
import dev.chre.rpc.proto.ChreApiTest.GeneralEventsMessage;
import dev.chre.rpc.proto.ChreApiTest.Status;

/**
 * WiFi Cross Validator that uses RPC-based chre_api_test nanoapp.
 */
public class ChreCrossValidatorWifi extends ContextHubChreApiTestExecutor {
    private static final String TAG = "ChreCrossValidatorWifi";

    private static final long AWAIT_WIFI_SCAN_RESULT_TIMEOUT_SEC = 30;

    /** Wifi capabilities flags listed in //system/chre/chre_api/include/chre_api/chre/wifi.h */
    private static final int WIFI_CAPABILITIES_SCAN_MONITORING = 1;
    private static final int WIFI_CAPABILITIES_ON_DEMAND_SCAN = 2;
    private static final int WIFI_CAPABILITIES_VENUE_INFO = 32;

    private static final int EID_INTERWORKING = 107;
    private static final int IW_IE_LEN_WITH_VENUE_INFO = 3;
    private static final int IW_IE_LEN_WITH_VENUE_INFO_AND_HESSID = 9;

    private static final int CHRE_EVENT_WIFI_ASYNC_RESULT = 0x0310;
    private static final int CHRE_EVENT_WIFI_SCAN_RESULT = 0x0311;

    private static final int CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR = 1;

    private final WifiManager mWifiManager;
    private final BroadcastReceiver mWifiScanReceiver;
    private final SettingsUtil mSettingsUtil;

    private boolean mInitialWifiEnabled;
    private boolean mInitialWifiScanningAlwaysEnabled;
    private boolean mInitialLocationEnabled;

    private final AtomicReference<Capabilities> mWifiCapabilities = new AtomicReference<>();
    private final AtomicBoolean mApWifiScanSuccess = new AtomicBoolean(false);
    private final CountDownLatch mAwaitApWifiSetupScan = new CountDownLatch(1);

    /**
     * A class to hold the result of a CHRE operation or verification.
     */
    private static class Result {
        private final boolean mSuccess;
        private final String mErrorMessage;
        private final boolean mIsFatal;

        Result(boolean success, String errorMessage) {
            this(success, errorMessage, false);
        }

        Result(boolean success, String errorMessage, boolean isFatal) {
            this.mSuccess = success;
            this.mErrorMessage = errorMessage;
            this.mIsFatal = isFatal;
        }
    }

    private final LinkedBlockingQueue<Result> mResultQueue = new LinkedBlockingQueue<>();

    private static class ScanBatch {
        final List<ChreWifiScanResult> mResults = new ArrayList<>();
        final int mExpectedSize;
        int mLastIndex = -1;

        ScanBatch(int expectedSize) {
            this.mExpectedSize = expectedSize;
        }

        boolean isComplete() {
            return mResults.size() >= mExpectedSize;
        }

        boolean isOver() {
            return mResults.size() > mExpectedSize;
        }
    }

    private final List<ScanBatch> mChreScanBatches = new ArrayList<>();
    private List<ScanResult> mApScanResultsSnapshot;
    /**
     * Threshold for result count verification. If result count > 100, we skip strict
     * count matching because AP and CHRE scan results might differ slightly due to
     * timing or environment, and CHRE memory limits might drop some results.
     */
    private final int mExpectedMaxChreResultCanHandle = 100;

    private final long mNanoAppId;

    private static class WifiVenueInfo {
        private int mVenueGroup = 0;
        private int mVenueType = 0;
    }

    public ChreCrossValidatorWifi(NanoAppBinary nanoapp) {
        super(nanoapp);
        mNanoAppId = nanoapp.getNanoAppId();

        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mWifiManager = context.getSystemService(WifiManager.class);
        mSettingsUtil = new SettingsUtil(context);
        mWifiScanReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context c, Intent intent) {
                Log.i(TAG, "onReceive called");
                boolean success = intent.getBooleanExtra(WifiManager.EXTRA_RESULTS_UPDATED, false);
                mApWifiScanSuccess.set(success);
                mAwaitApWifiSetupScan.countDown();
            }
        };
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION);
        context.registerReceiver(mWifiScanReceiver, intentFilter);
    }

    /**
     * Initializes the WiFi cross validator.
     */
    @Override
    public void init() {
        super.init();
        mInitialWifiEnabled = mSettingsUtil.isWifiEnabled();
        mInitialWifiScanningAlwaysEnabled = mSettingsUtil.isWifiScanningAlwaysEnabled();
        mInitialLocationEnabled = mSettingsUtil.isLocationEnabled();

        if (!mInitialWifiEnabled) {
            mSettingsUtil.setWifi(true);
        }
        if (!mInitialWifiScanningAlwaysEnabled) {
            mSettingsUtil.setWifiScanningSettings(true);
        }

        if (!mInitialWifiEnabled || !mInitialWifiScanningAlwaysEnabled
                || !mInitialLocationEnabled) {
            // Wait for settings to propagate
            // TODO(b/485888531): Move away from sleep to waiting nanoapp response.
            try {
                for (int i = 0; i < 10; i++) {
                    if (mSettingsUtil.isWifiEnabled()
                            && mSettingsUtil.isWifiScanningAlwaysEnabled()
                            && mSettingsUtil.isLocationEnabled()) {
                        break;
                    }
                    TimeUnit.SECONDS.sleep(1);
                }
            } catch (InterruptedException e) {
                Assert.fail("Interrupted while waiting for settings to propagate: "
                        + e.getMessage());
            }
            Assert.assertTrue("Settings did not propagate in time",
                    mSettingsUtil.isWifiEnabled()
                            && mSettingsUtil.isWifiScanningAlwaysEnabled()
                            && mSettingsUtil.isLocationEnabled());
        }

        ChreTestUtil.executeShellCommand(
                InstrumentationRegistry.getInstrumentation(),
                "cmd wifi set-verbose-logging enabled");
    }

    /**
     * Deinitializes the WiFi cross validator.
     */
    @Override
    public void deinit() {
        ChreTestUtil.executeShellCommand(
                InstrumentationRegistry.getInstrumentation(),
                "cmd wifi set-verbose-logging disabled");
        InstrumentationRegistry.getInstrumentation().getTargetContext()
                .unregisterReceiver(mWifiScanReceiver);

        mSettingsUtil.setWifi(mInitialWifiEnabled);
        mSettingsUtil.setWifiScanningSettings(mInitialWifiScanningAlwaysEnabled);

        super.deinit();
    }

    /**
     * Validates the WiFi scan results by comparing them with AP results.
     */
    public void validate() throws AssertionError, InterruptedException {
        int version = ChreTestUtil.getNanoAppVersion(mContextHubManager, mContextHub, mNanoAppId);
        Log.i(TAG, "Nanoapp version: " + Integer.toHexString(version));
        Assert.assertTrue("Nanoapp version must be >= 0.1.0", version >= 0x00010000);

        // 1. Get Capabilities
        Capabilities capabilities =
                com.google.android.utils.chre.ChreApiTestUtil.callUnaryRpcMethodSync(
                        getRpcClient(), "chre.rpc.ChreApiTestService.ChreWifiGetCapabilities");
        Assert.assertNotNull("Failed to get capabilities from CHRE", capabilities);
        mWifiCapabilities.set(capabilities);
        Assume.assumeTrue("CHRE WiFi is not enabled", chreWifiHasCapabilities(capabilities));

        // 2. Start Gathering Events
        GatherEventsInput gatherInput = GatherEventsInput.newBuilder()
                .addEventTypes(CHRE_EVENT_WIFI_ASYNC_RESULT)
                .addEventTypes(CHRE_EVENT_WIFI_SCAN_RESULT)
                .setEventCount(1000) // Large enough to not stop prematurely
                .setTimeoutInNs(30L * 1000 * 1000 * 1000) // 30s
                .build();

        var gatherFuture = getRpcClient()
                .getMethodClient("chre.rpc.ChreApiTestService.GatherEvents")
                .invokeServerStreamingFuture(
                        gatherInput,
                        (GeneralEventsMessage message) -> {
                            if (message.hasChreWifiScanEvent()) {
                                handleChreScanEvent(message.getChreWifiScanEvent());
                            } else if (message.hasChreAsyncResult()) {
                                handleAsyncResult(message.getChreAsyncResult());
                            } else {
                                Log.e(TAG, "Received unexpected event type: "
                                        + message.getDataCase());
                            }
                        });

        try {
            // 3. Configure Scan Monitor
            ChreWifiConfigureScanMonitorAsyncInput setupInput =
                    ChreWifiConfigureScanMonitorAsyncInput.newBuilder()
                            .setEnable(true)
                            .build();
            Status status = com.google.android.utils.chre.ChreApiTestUtil.callUnaryRpcMethodSync(
                    getRpcClient(), "chre.rpc.ChreApiTestService.ChreWifiConfigureScanMonitorAsync",
                    setupInput);
            Assert.assertTrue("Failed to configure scan monitor", status.getStatus());

            Result configResult = mResultQueue.poll(
                    AWAIT_WIFI_SCAN_RESULT_TIMEOUT_SEC, TimeUnit.SECONDS);
            Assert.assertNotNull("Timeout waiting for scan monitor configuration", configResult);
            Assert.assertTrue(configResult.mErrorMessage, configResult.mSuccess);

            // Clear any events that might have been received before/during configuration
            mResultQueue.clear();

            // 4. Start AP Scan
            Assert.assertTrue("Wifi manager start scan failed", mWifiManager.startScan());
            waitForApScanResults();

            synchronized (mChreScanBatches) {
                List<ScanResult> apResults = mWifiManager.getScanResults();
                mApScanResultsSnapshot = new ArrayList<>();
                for (ScanResult result : apResults) {
                    if (result.getBand() != ScanResult.WIFI_BAND_6_GHZ) {
                        mApScanResultsSnapshot.add(result);
                    }
                }
                // Verify any batches that arrived before AP results were ready
                for (int i = 0; i < mChreScanBatches.size(); i++) {
                    if (mChreScanBatches.get(i).isComplete()) {
                        verifyBatch(i);
                    }
                }
            }

            // 5. Wait for verification
            // We loop here because we might receive results for older/incomplete batches
            // before the correct batch arrives. We want to pass if ANY batch succeeds within the
            // timeout.
            long deadline = SystemClock.elapsedRealtimeNanos() + TimeUnit.SECONDS.toNanos(
                    AWAIT_WIFI_SCAN_RESULT_TIMEOUT_SEC);
            boolean success = false;
            String lastError = "No results received";

            while (true) {
                long remaining = deadline - SystemClock.elapsedRealtimeNanos();
                if (remaining <= 0) {
                    break;
                }

                Result result = mResultQueue.poll(remaining, TimeUnit.NANOSECONDS);
                if (result != null) {
                    if (result.mSuccess) {
                        success = true;
                        break;
                    } else {
                        lastError = result.mErrorMessage;
                        if (result.mIsFatal) {
                            Assert.fail("CHRE scan results verification failed (Fatal): "
                                    + lastError);
                        }
                        Log.w(TAG, "Received failed result, keeping waiting: " + lastError);
                    }
                }
            }

            Assert.assertTrue("CHRE scan results verification failed. Last error: " + lastError,
                    success);
        } finally {
            gatherFuture.cancel(true);
        }
    }

    /**
     * Checks if CHRE wifi has the required capabilities.
     * @param capabilities The wifi capabilities message from CHRE.
     * @return true if CHRE wifi has the necessary capabilities to run the test.
     */
    private boolean chreWifiHasCapabilities(Capabilities capabilities) {
        return (capabilities.getCapabilities() & WIFI_CAPABILITIES_SCAN_MONITORING) != 0
                && (capabilities.getCapabilities() & WIFI_CAPABILITIES_ON_DEMAND_SCAN) != 0;
    }

    /**
     * Waits for AP scan results to be available.
     */
    private void waitForApScanResults() throws InterruptedException {
        boolean success = mAwaitApWifiSetupScan.await(AWAIT_WIFI_SCAN_RESULT_TIMEOUT_SEC,
                                                      TimeUnit.SECONDS);
        Assert.assertTrue("Timeout waiting for AP scan results", success);
        Assert.assertTrue("AP wifi scan failed", mApWifiScanSuccess.get());
    }

    private void handleAsyncResult(dev.chre.rpc.proto.ChreApiTest.ChreAsyncResult result) {
        if (result.getRequestType() == CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR) {
            if (result.getSuccess()) {
                mResultQueue.offer(new Result(true, null));
            } else {
                mResultQueue.offer(new Result(false,
                        "Async operation failed. Type: " + result.getRequestType()
                        + ", Error: " + result.getErrorCode()));
            }
        } else {
            Log.e(TAG, "Received unexpected async result type: " + result.getRequestType());
        }
    }

    private void handleChreScanEvent(ChreWifiScanEvent event) {
        synchronized (mChreScanBatches) {
            ScanBatch lastBatch = mChreScanBatches.isEmpty()
                    ? null : mChreScanBatches.get(mChreScanBatches.size() - 1);

            int eventIndex = event.getEventIndex();
            ScanBatch targetBatch;
            // TODO(b/486040497): Create a new field in ChreWifiScanEvent to make sure we
            // are receiving events in order.
            if (lastBatch != null && !lastBatch.isComplete()) {
                // Continuation of the current batch
                if (eventIndex != lastBatch.mLastIndex + 1 && eventIndex != lastBatch.mLastIndex) {
                    setFatalErrorStr("Received out-of-order event. Expected index "
                            + (lastBatch.mLastIndex + 1) + " or " + lastBatch.mLastIndex
                            + ", got " + eventIndex);
                }
                targetBatch = lastBatch;
            } else {
                if (eventIndex != 0) {
                    setFatalErrorStr(
                            "Dropping event because it's not a start of a new batch (Index "
                            + eventIndex + " != 0).");
                    return;
                }
                targetBatch = new ScanBatch(event.getTotalNumResults());
                mChreScanBatches.add(targetBatch);
            }

            targetBatch.mLastIndex = eventIndex;
            targetBatch.mResults.addAll(event.getResultsList());

            if (targetBatch.isComplete()) {
                Log.i(TAG, "Batch " + (mChreScanBatches.size() - 1) + " complete.");
                verifyBatch(mChreScanBatches.size() - 1);
            } else if (targetBatch.isOver()) {
                String error = "Batch overflow! Expected " + targetBatch.mExpectedSize
                        + " but got " + targetBatch.mResults.size();
                Log.e(TAG, error);
                setFatalErrorStr(error);
            }
        }
    }

    private void verifyBatch(long scanIndex) {
        synchronized (mChreScanBatches) {
            ScanBatch batch = null;
            if (scanIndex < mChreScanBatches.size()) {
                batch = mChreScanBatches.get((int) scanIndex);
            }
            if (batch == null) return;

            if (batch.isOver()) {
                String error = String.format("Batch %d: More results than expected (%d > %d)",
                        scanIndex, batch.mResults.size(), batch.mExpectedSize);
                setErrorStr(error);
                return;
            }

            List<ChreWifiScanResult> chreResults = batch.mResults;
            List<ScanResult> apResults = mApScanResultsSnapshot;
            if (apResults == null) {
                Log.i(TAG, "AP results not ready for batch " + scanIndex);
                return;
            }

            StringBuilder errorMsg = new StringBuilder();
            boolean success = verifyScanResultCounts(apResults, chreResults, errorMsg)
                    && verifyChreResultsMatchAp(chreResults, apResults, errorMsg);

            if (success) {
                Log.i(TAG, "Verification successful for batch " + scanIndex);
                mResultQueue.offer(new Result(true, null));
            } else {
                mResultQueue.offer(new Result(false, errorMsg.toString()));
            }
        }
    }

    private boolean verifyScanResultCounts(List<ScanResult> apResults,
                                           List<ChreWifiScanResult> chreResults,
                                           StringBuilder errorMsg) {
        int apCount = apResults.size();
        int chreCount = chreResults.size();
        int expectedMinCount = Math.min(apCount, mExpectedMaxChreResultCanHandle);

        Log.i(TAG, "Counts: AP=" + apCount + ", CHRE=" + chreCount + ", MAX="
                   + mExpectedMaxChreResultCanHandle);
        if (chreCount < expectedMinCount) {
            errorMsg.append(String.format("Count too low: AP=%d, CHRE=%d, expected at least %d; ",
                    apCount, chreCount, expectedMinCount));
            return false;
        }
        return true;
    }

    private boolean verifyChreResultsMatchAp(List<ChreWifiScanResult> chreResults,
                                             List<ScanResult> apResults, StringBuilder errorMsg) {
        boolean allMatched = true;
        Set<String> seenApBssids = new HashSet<>();
        for (ChreWifiScanResult chreResult : chreResults) {
            String chreBssid = bssidBytesToString(chreResult.getBssid().toByteArray());
            ScanResult match = findMatchingApResult(chreBssid, apResults);
            if (match != null) {
                if (seenApBssids.contains(chreBssid)) {
                    errorMsg.append("Duplicate CHRE BSSID: ").append(chreBssid).append("; ");
                    allMatched = false;
                }
                if (!chreWifiResultMatchesAp(chreResult, match, errorMsg)) allMatched = false;
                seenApBssids.add(chreBssid);
            } else {
                errorMsg.append("CHRE BSSID not in AP: ").append(chreBssid).append("; ");
                allMatched = false;
            }
        }
        if (!verifyApResultsSeenInChre(apResults, seenApBssids, errorMsg)) allMatched = false;

        return allMatched;
    }

    private boolean verifyApResultsSeenInChre(List<ScanResult> apResults,
                                              Set<String> seenApBssids,
                                              StringBuilder errorMsg) {
        boolean allMatched = true;
        for (ScanResult apResult : apResults) {
            if (!seenApBssids.contains(apResult.BSSID)) {
                if (apResults.size() <= mExpectedMaxChreResultCanHandle) {
                    errorMsg.append("AP BSSID not in CHRE: ").append(apResult.BSSID).append("; ");
                    allMatched = false;
                }
            }
        }
        return allMatched;
    }

    private boolean chreWifiResultMatchesAp(ChreWifiScanResult chreResult, ScanResult apResult,
                                            StringBuilder errorMsg) {
        if (!chreResult.getSsid().equals(apResult.SSID)) {
            errorMsg.append("SSID mismatch for ").append(apResult.BSSID).append("; ");
            return false;
        }
        if ((mWifiCapabilities.get().getCapabilities() & WIFI_CAPABILITIES_VENUE_INFO) == 0) {
            return true;
        }
        WifiVenueInfo venueInfo = getVenueInfo(apResult);
        if (chreResult.getVenueGroup() != venueInfo.mVenueGroup
                || chreResult.getVenueType() != venueInfo.mVenueType) {
            errorMsg.append("Venue mismatch for ").append(apResult.BSSID)
                    .append(". AP: ").append(venueInfo.mVenueGroup).append("/")
                    .append(venueInfo.mVenueType).append(", CHRE: ")
                    .append(chreResult.getVenueGroup()).append("/")
                    .append(chreResult.getVenueType()).append("; ");
            return false;
        }
        return true;
    }

    private WifiVenueInfo getVenueInfo(ScanResult result) {
        WifiVenueInfo venueInfo = new WifiVenueInfo();
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.R) {
            return venueInfo;
        }
        for (ScanResult.InformationElement element : result.getInformationElements()) {
            if (element != null && element.getId() == EID_INTERWORKING) {
                ByteBuffer payload = element.getBytes();
                if (payload.limit() == IW_IE_LEN_WITH_VENUE_INFO
                        || payload.limit() == IW_IE_LEN_WITH_VENUE_INFO_AND_HESSID) {
                    venueInfo.mVenueGroup = convertBufferByte(payload, 1);
                    venueInfo.mVenueType = convertBufferByte(payload, 2);
                    break;
                } else {
                    Log.e(TAG, "Ignoring Interworking IE with unexpected length: "
                            + payload.limit());
                }
            }
        }
        return venueInfo;
    }

    private static int convertBufferByte(ByteBuffer buffer, int index) {
        if (buffer.limit() > index) {
            return Byte.toUnsignedInt(buffer.get(index));
        }
        return 0;
    }

    private ScanResult findMatchingApResult(String bssid, List<ScanResult> apResults) {
        for (ScanResult apResult : apResults) {
            if (apResult.BSSID.equalsIgnoreCase(bssid)) return apResult;
        }
        return null;
    }

    private String bssidBytesToString(byte[] bssid) {
        StringBuilder sb = new StringBuilder(18);
        for (byte b : bssid) {
            if (sb.length() > 0) sb.append(':');
            sb.append(String.format("%02x", b & 0xFF));
        }
        return sb.toString();
    }

    protected void setErrorStr(String errorStr) {
        mResultQueue.offer(new Result(false, errorStr));
    }

    protected void setFatalErrorStr(String errorStr) {
        // TODO(b/397357827): Platform bug - CHRE sending out-of-order events or invalid batches.
        mResultQueue.offer(new Result(false, errorStr, true));
    }
}
