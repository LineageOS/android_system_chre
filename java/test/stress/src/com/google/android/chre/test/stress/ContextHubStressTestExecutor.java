/*
 * Copyright (C) 2021 The Android Open Source Project
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
package com.google.android.chre.test.stress;

import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.Manifest.permission.BLUETOOTH_CONNECT;
import static android.Manifest.permission.BLUETOOTH_PRIVILEGED;
import static android.Manifest.permission.BLUETOOTH_SCAN;

import android.Manifest;
import android.app.Instrumentation;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.hardware.location.ContextHubClient;
import android.hardware.location.ContextHubClientCallback;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.ContextHubTransaction;
import android.hardware.location.NanoAppBinary;
import android.hardware.location.NanoAppMessage;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.telephony.CellInfo;
import android.telephony.TelephonyManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.google.android.chre.nanoapp.proto.ChreStressTest;
import com.google.android.chre.nanoapp.proto.ChreTestCommon;
import com.google.android.utils.chre.BleHostClientUtil;
import com.google.android.utils.chre.ChreTestUtil;
import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Assert;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A class that can execute the CHRE Stress test.
 */
public class ContextHubStressTestExecutor extends ContextHubClientCallback {
    private static final String TAG = "ContextHubStressTestExecutor";

    /**
     * Wifi capabilities flags listed in
     * //system/chre/chre_api/include/chre_api/chre/wifi.h
     */
    private static final int WIFI_CAPABILITIES_SCAN_MONITORING = 1;

    private final NanoAppBinary mNanoAppBinary;

    private final long mNanoAppId;

    private ContextHubClient mContextHubClient;

    private final AtomicReference<ChreTestCommon.TestResult> mTestResult =
            new AtomicReference<>();

    private final AtomicBoolean mChreReset = new AtomicBoolean(false);

    private final ContextHubManager mContextHubManager;

    private final ContextHubInfo mContextHubInfo;

    private CountDownLatch mCountDownLatch;

    private ChreStressTest.Capabilities mCapabilities;

    // Set to true to have the test suite only load the nanoapp and start the test.
    // This can be useful for long-running stress tests, where we do not want to wait a fixed
    // time to wait for successful completion.
    private boolean mLoadAndStartOnly = false;

    private final AtomicBoolean mWifiScanMonitorTriggered = new AtomicBoolean(false);

    private final Instrumentation mInstrumentation = InstrumentationRegistry.getInstrumentation();

    // Used for generating AP workload
    private final AtomicBoolean mStopApStress = new AtomicBoolean(false);
    private ExecutorService mApStressExecutor;
    private List<Future<?>> mApStressTasks = new ArrayList<>();

    // AP workload stats
    private final AtomicLong mWifiScanCount = new AtomicLong(0);
    private final AtomicLong mGnssLocationCount = new AtomicLong(0);
    private final AtomicLong mSensorEventCount = new AtomicLong(0);
    private final AtomicLong mAudioEventCount = new AtomicLong(0);
    private final AtomicLong mAudioBytesRead = new AtomicLong(0);
    private final AtomicLong mBleScanCount = new AtomicLong(0);
    private final AtomicLong mWwanRequestCount = new AtomicLong(0);

    public ContextHubStressTestExecutor(ContextHubManager manager, ContextHubInfo info,
            NanoAppBinary binary) {
        mNanoAppBinary = binary;
        mNanoAppId = mNanoAppBinary.getNanoAppId();
        mContextHubManager = manager;
        mContextHubInfo = info;
    }

    // Used for managing AP side sensor listening.
    private final SensorEventListener mSensorListener =
            new SensorEventListener() {
                @Override
                public void onSensorChanged(SensorEvent event) {
                    long total = mSensorEventCount.incrementAndGet();
                    if (total % 200 == 0) {
                        Log.i(TAG, "[AP_LOAD] Sensors received " + total + " events so far.");
                    }
                }

                @Override
                public void onAccuracyChanged(Sensor sensor, int accuracy) {}
            };

    @Override
    public void onMessageFromNanoApp(ContextHubClient client, NanoAppMessage message) {
        if (message.getNanoAppId() == mNanoAppId) {
            Log.d(TAG, "Got message from nanoapp: " + message);
            boolean valid = false;
            switch (message.getMessageType()) {
                case ChreStressTest.MessageType.TEST_RESULT_VALUE: {
                    try {
                        mTestResult.set(
                                ChreTestCommon.TestResult.parseFrom(message.getMessageBody()));
                        valid = true;
                    } catch (InvalidProtocolBufferException e) {
                        Log.e(TAG, "Failed to parse message: " + e.getMessage());
                    }
                    break;
                }
                case ChreStressTest.MessageType.TEST_WIFI_SCAN_MONITOR_TRIGGERED_VALUE: {
                    mWifiScanMonitorTriggered.set(true);
                    valid = true;
                    break;
                }
                case ChreStressTest.MessageType.CAPABILITIES_VALUE: {
                    try {
                        mCapabilities =
                                ChreStressTest.Capabilities.parseFrom(message.getMessageBody());
                        valid = true;
                    } catch (InvalidProtocolBufferException e) {
                        Log.e(TAG, "Failed to parse message: " + e.getMessage());
                    }
                    break;
                }
                default: {
                    Log.e(TAG, "Unknown message type " + message.getMessageType());
                }
            }

            if (valid && mCountDownLatch != null) {
                mCountDownLatch.countDown();
            }
        }
    }

    @Override
    public void onHubReset(ContextHubClient client) {
        mChreReset.set(true);
        if (mCountDownLatch != null) {
            mCountDownLatch.countDown();
        }
    }

    /**
     * Should be invoked before run() is invoked to set up the test, e.g. in a @Before method.
     */
    public void init() {
        init(false /* loadAndStartOnly */, false /* unloadOnly */);
    }

    /**
     * Same version of init, but specifies mLoadAndStartOnly.
     *
     * @param loadAndStartOnly Sets mLoadAndStartOnly.
     */
    public void init(boolean loadAndStartOnly) {
        init(loadAndStartOnly, false /* unloadOnly */);
    }

    /**
     * Same version of init, but specifies mLoadAndStartOnly and unloadOnly.
     *
     * @param loadAndStartOnly Sets mLoadAndStartOnly.
     * @param unloadOnly       Set to true if the nanoapp is already loaded.
     */
    public void init(boolean loadAndStartOnly, boolean unloadOnly) {
        mLoadAndStartOnly = loadAndStartOnly;
        if (!unloadOnly) {
            ChreTestUtil.loadNanoAppAssertSuccess(mContextHubManager, mContextHubInfo,
                    mNanoAppBinary);
        }
        mContextHubClient = mContextHubManager.createClient(mContextHubInfo, this);
    }

    /**
     * @param timeout The amount of time to run the stress test.
     * @param unit    The unit for timeout.
     */
    public void runStressTest(long timeout, TimeUnit unit) throws InterruptedException {
        ChreStressTest.TestCommand.Feature[] features = {
            ChreStressTest.TestCommand.Feature.WIFI_ON_DEMAND_SCAN,
            ChreStressTest.TestCommand.Feature.GNSS_LOCATION,
            ChreStressTest.TestCommand.Feature.GNSS_MEASUREMENT,
            ChreStressTest.TestCommand.Feature.WWAN,
            ChreStressTest.TestCommand.Feature.SENSORS,
            ChreStressTest.TestCommand.Feature.AUDIO,
            ChreStressTest.TestCommand.Feature.BLE,
        };

        // Acquires necessary AP permissions
        mInstrumentation
                .getUiAutomation()
                .adoptShellPermissionIdentity(
                        Manifest.permission.ACCESS_FINE_LOCATION,
                        Manifest.permission.ACCESS_COARSE_LOCATION,
                        Manifest.permission.ACCESS_BACKGROUND_LOCATION,
                        Manifest.permission.CHANGE_WIFI_STATE,
                        Manifest.permission.ACCESS_WIFI_STATE,
                        Manifest.permission.BLUETOOTH_SCAN,
                        Manifest.permission.BLUETOOTH_CONNECT,
                        Manifest.permission.BLUETOOTH_PRIVILEGED,
                        Manifest.permission.RECORD_AUDIO,
                        Manifest.permission.READ_PHONE_STATE);

        mTestResult.set(null);
        mCountDownLatch = new CountDownLatch(1);

        // Starts AP side stress test
        if (!mLoadAndStartOnly) {
            startApSideStressLoads(features);
        }

        // Starts nanoapp side stress test
        for (ChreStressTest.TestCommand.Feature feature : features) {
            sendTestMessage(feature, true /* start */);
        }

        if (!mLoadAndStartOnly) {
            mCountDownLatch.await(timeout, unit);

            // Stops AP side stress test
            stopApSideStressLoads();

            checkTestFailure();

            for (ChreStressTest.TestCommand.Feature feature : features) {
                sendTestMessage(feature, false /* start */);
            }

            try {
                // Add a short delay to make sure the stop command did not cause issues.
                Thread.sleep(10000);
            } catch (InterruptedException e) {
                Assert.fail(e.getMessage());
            }
        }

        // Release permissions.
        mInstrumentation.getUiAutomation().dropShellPermissionIdentity();
    }

    /**
     * Sends a command to enable scan monitoring.
     */
    public void sendScanMonitorCommand() {
        sendTestMessage(ChreStressTest.TestCommand.Feature.WIFI_SCAN_MONITOR, true /* start */);
    }

    /**
     * A test to verify whether a scan monitor request persists through WLAN restarts.
     *
     * The test framework is expected to perform the following operations prior to running this
     * method.
     * 1. Load the nanoapp through init() (with loadAndStartOnly enabled)
     * 2. Invoke sendScanMonitorCommand
     * 3. Restart the WLAN.
     * 4. Keep the nanoapp loaded, and then run this test.
     * 5. Unload the nanoapp after this test ends.
     */
    public void runWifiScanMonitorRestartTest() throws InterruptedException {
        // Since the host connection may have reset, inform the nanoapp about this event.
        NanoAppMessage message = NanoAppMessage.createMessageToNanoApp(
                mNanoAppId, ChreStressTest.MessageType.TEST_HOST_RESTARTED_VALUE,
                new byte[0]);
        sendMessageToNanoApp(message);

        mCountDownLatch = new CountDownLatch(1);
        message = NanoAppMessage.createMessageToNanoApp(
                mNanoAppId, ChreStressTest.MessageType.GET_CAPABILITIES_VALUE,
                new byte[0]);
        sendMessageToNanoApp(message);

        boolean success = mCountDownLatch.await(30, TimeUnit.SECONDS);
        Assert.assertTrue("Timeout waiting for signal: wifi scan monitor restart test", success);

        if ((mCapabilities.getWifi() & WIFI_CAPABILITIES_SCAN_MONITORING) != 0) {
            WifiManager manager =
                    (WifiManager)
                            mInstrumentation.getContext().getSystemService(Context.WIFI_SERVICE);
            Assert.assertNotNull(manager);

            mWifiScanMonitorTriggered.set(false);
            mCountDownLatch = new CountDownLatch(1);
            Assert.assertTrue(manager.startScan());

            success = mCountDownLatch.await(30, TimeUnit.SECONDS);
            Assert.assertTrue("Timeout waiting for signal: trigger scan monitor", success);
            Assert.assertTrue(mWifiScanMonitorTriggered.get());
            checkTestFailure();
        }

        sendTestMessage(ChreStressTest.TestCommand.Feature.WIFI_SCAN_MONITOR, false /* start */);

        // Add a short delay to ensure the scan monitor request is stopped at the nanoapp.
        // This avoids the requests to leak beyond this test.
        // TODO(b/144189870): Remove when unload safety is implemented in CHRE.
        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            Assert.fail(e.getMessage());
        }
    }

    /**
     * Tests concurrent BLE scans from the AP and from CHRE.
     */
    public void runBleScanConcurrencyStressTest() throws InterruptedException {
        sendTestMessage(ChreStressTest.TestCommand.Feature.BLE, true /* start */);

        mInstrumentation.getUiAutomation().adoptShellPermissionIdentity(BLUETOOTH_SCAN,
                BLUETOOTH_CONNECT, BLUETOOTH_PRIVILEGED, ACCESS_FINE_LOCATION);
        BleHostClientUtil bleClient = new BleHostClientUtil(mInstrumentation.getContext());
        Assert.assertTrue("BLE capabilities are available", bleClient.isBleAvailable());

        while (true) {
            mCountDownLatch = new CountDownLatch(1);
            bleClient.start();
            mCountDownLatch.await(100, TimeUnit.MILLISECONDS);
            bleClient.stop();
        }
    }

    /**
     * Cleans up the test, should be invoked in e.g. @After method.
     */
    public void deinit() {
        if (!mLoadAndStartOnly) {
            ChreTestUtil.unloadNanoApp(mContextHubManager, mContextHubInfo, mNanoAppId);
        }
        if (mContextHubClient != null) {
            mContextHubClient.close();
        }

        if (mChreReset.get()) {
            Assert.fail("CHRE reset during the test");
        }
    }

    /**
     * @param feature The feature to start testing for.
     * @param start   true to start the test, false to stop.
     */
    private void sendTestMessage(ChreStressTest.TestCommand.Feature feature, boolean start) {
        ChreStressTest.TestCommand testCommand = ChreStressTest.TestCommand.newBuilder()
                .setFeature(feature).setStart(start).build();

        NanoAppMessage message = NanoAppMessage.createMessageToNanoApp(
                mNanoAppId, ChreStressTest.MessageType.TEST_COMMAND_VALUE,
                testCommand.toByteArray());
        sendMessageToNanoApp(message);
    }

    private void sendMessageToNanoApp(NanoAppMessage message) {
        int result = mContextHubClient.sendMessageToNanoApp(message);
        if (result != ContextHubTransaction.RESULT_SUCCESS) {
            Assert.fail("Failed to send message: result = " + result);
        }
    }

    private void checkTestFailure() {
        if (mTestResult.get() != null
                && mTestResult.get().getCode() == ChreTestCommon.TestResult.Code.FAILED) {
            if (mTestResult.get().hasErrorMessage()) {
                Assert.fail(new String(mTestResult.get().getErrorMessage().toByteArray(),
                        StandardCharsets.UTF_8));
            } else {
                Assert.fail("Stress test failed");
            }
        }
    }

    // ====== AP stress generating functions below. ======

    private void startApSideStressLoads(ChreStressTest.TestCommand.Feature[] features) {
        mStopApStress.set(false);
        mApStressExecutor = Executors.newCachedThreadPool();
        mApStressTasks.clear();

        Context context = mInstrumentation.getContext();

        for (ChreStressTest.TestCommand.Feature feature : features) {
            switch (feature) {
                case WIFI_ON_DEMAND_SCAN:
                    mApStressTasks.add(mApStressExecutor.submit(this::runApWifiScanLoad));
                    break;
                case GNSS_LOCATION:
                case GNSS_MEASUREMENT:
                    mApStressTasks.add(mApStressExecutor.submit(this::runApGnssLoad));
                    break;
                case WWAN:
                    mApStressTasks.add(mApStressExecutor.submit(this::runApWwanLoad));
                    break;
                case SENSORS:
                    mApStressTasks.add(mApStressExecutor.submit(this::runApSensorLoad));
                    break;
                case AUDIO:
                    mApStressTasks.add(mApStressExecutor.submit(this::runApAudioLoad));
                    break;
                case BLE:
                    mApStressTasks.add(mApStressExecutor.submit(this::runApBleLoad));
                    break;
                default:
                    break;
            }
        }
    }

    private void stopApSideStressLoads() {
        mStopApStress.set(true);
        if (mApStressExecutor != null) {
            for (Future<?> task : mApStressTasks) {
                task.cancel(true);
            }
            mApStressExecutor.shutdownNow();
            try {
                mApStressExecutor.awaitTermination(5, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Log.e(TAG, "[AP_LOAD] Interrupted while waiting for AP stress tasks to stop");
            }
            mApStressExecutor = null;
        }

        // Clear sensor registration
        Context context = mInstrumentation.getContext();
        SensorManager sm = context.getSystemService(SensorManager.class);
        if (sm != null) {
            sm.unregisterListener(mSensorListener);
        }

        // Print final report
        StringBuilder sb = new StringBuilder();
        sb.append("\n================ AP STRESS LOAD SUMMARY ================\n");
        sb.append(String.format("WiFi Scans Triggered:  %d\n", mWifiScanCount.get()));
        sb.append(String.format("GNSS Locations Rx:     %d\n", mGnssLocationCount.get()));
        sb.append(String.format("Sensor Events Rx:      %d\n", mSensorEventCount.get()));
        sb.append(String.format("Audio Events Rx:      %d\n", mAudioEventCount.get()));
        sb.append(String.format("Audio Bytes Recorded:  %d bytes\n", mAudioBytesRead.get()));
        sb.append(String.format("BLE Scan Cycles:       %d\n", mBleScanCount.get()));
        sb.append(String.format("WWAN Info Requests:    %d\n", mWwanRequestCount.get()));
        sb.append("========================================================\n");

        Log.i(TAG, sb.toString());
    }

    private void runApWifiScanLoad() {
        WifiManager wm =
                (WifiManager) mInstrumentation.getContext().getSystemService(Context.WIFI_SERVICE);
        Assert.assertNotNull("WifiManager not found", wm);

        Log.i(TAG, "[AP_LOAD] Starting WiFi Scan load...");
        while (!mStopApStress.get() && !Thread.currentThread().isInterrupted()) {
            boolean success = wm.startScan();
            long count = mWifiScanCount.incrementAndGet();
            Log.i(TAG, "[AP_LOAD] WiFi Scan Triggered #" + count + ", success=" + success);

            try {
                // Scan interval
                Thread.sleep(4000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    private void runApGnssLoad() {
        LocationManager lm =
                (LocationManager)
                        mInstrumentation.getContext().getSystemService(Context.LOCATION_SERVICE);
        Assert.assertNotNull("LocationManager not found", lm);

        LocationListener listener =
                new LocationListener() {
                    @Override
                    public void onLocationChanged(Location location) {
                        long count = mGnssLocationCount.incrementAndGet();
                        Log.i(
                                TAG,
                                "[AP_LOAD] GNSS Location updated #"
                                        + count
                                        + ": "
                                        + location.getProvider());
                    }

                    @Override
                    public void onProviderDisabled(String provider) {}

                    @Override
                    public void onProviderEnabled(String provider) {}

                    @Override
                    public void onStatusChanged(String provider, int status, Bundle extras) {}
                };

        Log.i(TAG, "[AP_LOAD] Registered GNSS updates.");

        // Use Looper to run stress load
        mInstrumentation.runOnMainSync(
                () -> {
                    try {
                        if (lm.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                            lm.requestLocationUpdates(
                                    LocationManager.GPS_PROVIDER, 1000, 0, listener);
                        }
                        if (lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
                            lm.requestLocationUpdates(
                                    LocationManager.NETWORK_PROVIDER, 1000, 0, listener);
                        }
                    } catch (SecurityException e) {
                        Log.e(TAG, "[AP_LOAD] Security exception requesting location updates", e);
                    }
                });

        while (!mStopApStress.get() && !Thread.currentThread().isInterrupted()) {
            try {
                // Scan interval
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        mInstrumentation.runOnMainSync(() -> lm.removeUpdates(listener));
    }

    private void runApSensorLoad() {
        SensorManager sm =
                (SensorManager)
                        mInstrumentation.getContext().getSystemService(Context.SENSOR_SERVICE);
        Assert.assertNotNull("SensorManager not found", sm);

        Log.i(TAG, "[AP_LOAD] Starting AP Sensor load");
        List<Sensor> sensors = sm.getSensorList(Sensor.TYPE_ALL);
        int registeredCount = 0;

        for (Sensor s : sensors) {
            if (s.getType() == Sensor.TYPE_ACCELEROMETER || s.getType() == Sensor.TYPE_GYROSCOPE) {
                boolean success =
                        sm.registerListener(mSensorListener, s, SensorManager.SENSOR_DELAY_GAME);
                Log.i(
                        TAG,
                        "[AP_LOAD] Registering sensor: "
                                + s.getName()
                                + " (Type "
                                + s.getType()
                                + "), success="
                                + success);
                if (success) registeredCount++;
            }
        }

        if (registeredCount == 0) {
            Log.w(TAG, "[AP_LOAD] No target sensors (Accel/Gyro) were registered!");
        }
        // Unregister logic will be handled in stopApSideStressLoads
    }

    private void runApAudioLoad() {
        int sampleRate = 44100;
        int channelConfig = AudioFormat.CHANNEL_IN_MONO;
        int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
        int minBufSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);

        if (minBufSize == AudioRecord.ERROR || minBufSize == AudioRecord.ERROR_BAD_VALUE) {
            Log.e(TAG, "[AP_LOAD] Invalid AudioRecord parameters");
            return;
        }

        Log.i(TAG, "[AP_LOAD] Starting AP Audio load");

        // Note: if CHRE is using mic, AP may fail or received all 0 data.
        // We are creating resource racing here.
        try {
            AudioRecord recorder =
                    new AudioRecord(
                            MediaRecorder.AudioSource.MIC,
                            sampleRate,
                            channelConfig,
                            audioFormat,
                            minBufSize * 2);

            if (recorder.getState() == AudioRecord.STATE_INITIALIZED) {
                recorder.startRecording();
                Log.i(TAG, "[AP_LOAD] Audio recording started.");

                byte[] buffer = new byte[minBufSize];
                while (!mStopApStress.get() && !Thread.currentThread().isInterrupted()) {
                    int bytesRead = recorder.read(buffer, 0, minBufSize);
                    if (bytesRead > 0) {
                        mAudioBytesRead.addAndGet(bytesRead);
                    }
                    long count = mAudioEventCount.incrementAndGet();
                    if (count % 50 == 0) {
                        Log.i(
                                TAG,
                                "[AP_LOAD] Audio recording read count: "
                                        + count
                                        + ", read bytes: "
                                        + bytesRead);
                    }
                }
                recorder.stop();
                recorder.release();
            }
        } catch (Exception e) {
            Log.e(TAG, "[AP_LOAD] Failed to run Audio load", e);
        }
    }

    private void runApWwanLoad() {
        Log.i(TAG, "[AP_LOAD] Starting AP WWAN load setup...");

        TelephonyManager tm =
                (TelephonyManager)
                        mInstrumentation.getContext().getSystemService(Context.TELEPHONY_SERVICE);

        if (tm == null) {
            Log.e(TAG, "[AP_LOAD] WWAN Load Aborted: TelephonyManager is NULL");
            return;
        }

        // Checks if location service is enabled.
        LocationManager lm =
                (LocationManager)
                        mInstrumentation.getContext().getSystemService(Context.LOCATION_SERVICE);
        boolean isLocationEnabled = lm != null && lm.isLocationEnabled();
        Log.i(TAG, "[AP_LOAD] Location enabled status: " + isLocationEnabled);

        Log.i(TAG, "[AP_LOAD] Starting WWAN Request Loop...");

        while (!mStopApStress.get() && !Thread.currentThread().isInterrupted()) {
            try {
                List<CellInfo> cellInfo = tm.getAllCellInfo();
                long count = mWwanRequestCount.incrementAndGet();

                // Preint results every 5 times or on error
                if (cellInfo != null) {
                    if (count % 5 == 0) {
                        Log.i(
                                TAG,
                                "[AP_LOAD] WWAN Request #"
                                        + count
                                        + " success. Cells found: "
                                        + cellInfo.size());
                    }
                } else {
                    Log.w(
                            TAG,
                            "[AP_LOAD] WWAN Request #"
                                    + count
                                    + " returned NULL (Modem busy or Location off?)");
                }

                // Request interval
                Thread.sleep(2000);

            } catch (SecurityException e) {
                Log.e(TAG, "[AP_LOAD] WWAN SecurityException: " + e.getMessage());
                try {
                    Thread.sleep(5000);
                } catch (InterruptedException ex) {
                    Thread.currentThread().interrupt();
                }

            } catch (IllegalStateException e) {
                Log.e(
                        TAG,
                        "[AP_LOAD] WWAN IllegalStateException (Service not ready?): "
                                + e.getMessage());
                try {
                    Thread.sleep(2000);
                } catch (InterruptedException ex) {
                    Thread.currentThread().interrupt();
                }

            } catch (Exception e) {
                Log.e(TAG, "[AP_LOAD] WWAN Critical Unknown Error", e);
                try {
                    Thread.sleep(2000);
                } catch (InterruptedException ex) {
                    Thread.currentThread().interrupt();
                }
            }
        }

        Log.i(TAG, "[AP_LOAD] AP WWAN load thread stopped.");
    }

    private void runApBleLoad() {
        BleHostClientUtil bleClient = new BleHostClientUtil(mInstrumentation.getContext());
        if (!bleClient.isBleAvailable()) {
            Log.w(TAG, "[AP_LOAD] BLE not available on AP, skipping load");
            return;
        }

        Log.i(TAG, "[AP_LOAD] Starting AP BLE load");
        while (!mStopApStress.get() && !Thread.currentThread().isInterrupted()) {
            bleClient.start();
            long count = mBleScanCount.incrementAndGet();
            Log.i(TAG, "[AP_LOAD] BLE Scan cycle started #" + count);
            try {
                // Scan period
                Thread.sleep(2000);
                bleClient.stop();
                // Request interval
                Thread.sleep(100);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        bleClient.stop();
    }
}
