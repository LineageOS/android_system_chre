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

package com.android.chre.chdts;

import com.android.compatibility.common.util.ApiLevelUtil;
import com.android.compatibility.common.util.PropertyUtil;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.DeviceTestRunOptions;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Context Hub Device Test Suite (CHDTS) Host Tests.
 * Ported from GtsGmsCoreLocationContextTestApp.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class ChdtsHostTestCases extends BaseHostJUnit4Test {

    // Package name defined in app/AndroidManifest.xml
    private static final String PACKAGE = "com.android.chre.chdts.app";

    private static final String TEST_CLASS_PACKAGE = "com.google.android.gts.locationcontext";

    // Feature constants
    private static final String FEATURE_CONTEXT_HUB = "android.hardware.context_hub";
    private static final String SENSOR_FEATURE_NAME_ACCELEROMETER =
            "android.hardware.sensor.accelerometer";
    private static final String SENSOR_FEATURE_NAME_GYROSCOPE = "android.hardware.sensor.gyroscope";
    private static final String SENSOR_FEATURE_NAME_MAGNETIC_FIELD =
            "android.hardware.sensor.compass";
    private static final String SENSOR_FEATURE_NAME_PRESSURE = "android.hardware.sensor.barometer";
    private static final String SENSOR_FEATURE_NAME_LIGHT = "android.hardware.sensor.light";
    private static final String SENSOR_FEATURE_NAME_PROXIMITY = "android.hardware.sensor.proximity";
    private static final String SENSOR_FEATURE_NAME_STEP_COUNTER =
            "android.hardware.sensor.stepcounter";

    // Build Version Codes (Local definition to avoid XTS dependencies)
    private static final int NYC = 24; // Android 7.0
    private static final int PI = 28;  // Android 9.0
    private static final int QT = 29;  // Android 10.0
    private static final int RVC = 30; // Android 11.0
    private static final int SC = 31;  // Android 12.0
    private static final int UDC = 34; // Android 14.0
    private static final int VIC = 35; // Android 15.0
    private static final int BAKLAVA = 36; // Android 16 (Planned)

    private ITestDevice mDevice;
    private boolean mHasServiceFeature;
    private boolean mHasPendingIntentFeature;
    private boolean mRunSettingsTest;
    private boolean mRunMicDisableSettingsTest;
    private boolean mRunCrossValidationTest;
    private boolean mRunCrossValidationWifiTest;
    private boolean mRunCrossValidationWwanTest;
    private boolean mRunAudioConcurrencyTest;
    private boolean mRunPermissionTest;
    private boolean mRunRpcServiceTest;
    private boolean mRunNanoAppRequirementsTest;
    private boolean mRunBleConcurrencyTest;
    private boolean mRunBleSettingsTest;
    private boolean mRunHostEndpointTest;
    private boolean mRunChreConcurrencyTest;
    private boolean mRunReliableMessageTest;
    private boolean mRunEndpointTests;
    private boolean mRunStressTests;

    @Option(name = "externalNanoAppPath",
            description = "The path to the directory which contains test nanoapps "
            + "for ContextHubHostTest")
    private String mExternalNanoAppPath = null;

    @Option(name = "stressTestDurationSeconds",
            description = "The duration of stress test (in seconds) "
            + "for ContextHubHostTest.")
    private String mStressTestDurationSeconds = null;

    @Option(name = "static_nanoapps",
            description = "Whether to use built in static nanoapps "
            + "instead of dynamically loading them.")
    private boolean mStaticNanoapps = false;

    @Before
    public void setUp() throws Exception {
        mDevice = getDevice();

        mHasServiceFeature = ApiLevelUtil.isAtLeast(mDevice, PI);

        mHasPendingIntentFeature = ApiLevelUtil.isAtLeast(mDevice, QT);

        mRunSettingsTest = ApiLevelUtil.isAtLeast(mDevice, RVC);

        mRunMicDisableSettingsTest = ApiLevelUtil.isAtLeast(mDevice, SC);

        mRunCrossValidationTest = ApiLevelUtil.isAtLeast(mDevice, RVC)
                && PropertyUtil.getFirstApiLevel(mDevice) >= RVC;

        mRunCrossValidationWifiTest = ApiLevelUtil.isAtLeast(mDevice, RVC)
                && PropertyUtil.getFirstApiLevel(mDevice) >= SC;

        mRunAudioConcurrencyTest = ApiLevelUtil.isAtLeast(mDevice, RVC);

        mRunPermissionTest = ApiLevelUtil.isAtLeast(mDevice, SC)
                && PropertyUtil.getFirstApiLevel(mDevice) >= SC;

        boolean runUdcTests = ApiLevelUtil.isAtLeast(mDevice, UDC)
                && PropertyUtil.getFirstApiLevel(mDevice) >= UDC
                && PropertyUtil.getVendorApiLevel(mDevice) >= UDC;

        mRunRpcServiceTest = runUdcTests;
        mRunNanoAppRequirementsTest = runUdcTests;
        mRunBleConcurrencyTest = runUdcTests;
        mRunBleSettingsTest = runUdcTests;
        mRunHostEndpointTest = runUdcTests;
        mRunChreConcurrencyTest = runUdcTests;

        boolean runVicTests = ApiLevelUtil.isAtLeast(mDevice, VIC)
                && PropertyUtil.getFirstApiLevel(mDevice) >= VIC
                && PropertyUtil.getVendorApiLevel(mDevice) >= VIC;
        mRunReliableMessageTest = runVicTests;

        boolean runBaklavaTests = ApiLevelUtil.isAtLeast(mDevice, BAKLAVA)
                && PropertyUtil.getFirstApiLevel(mDevice) >= BAKLAVA
                && PropertyUtil.getVendorApiLevel(mDevice) >= BAKLAVA;
        mRunEndpointTests = runBaklavaTests;
        mRunCrossValidationWwanTest = runBaklavaTests;

        mRunStressTests = (mStressTestDurationSeconds != null);

        // Ensure Wifi is enabled for tests requiring it
        if (!getDevice().isWifiEnabled()) {
            getDevice().executeShellV2Command("svc wifi enable");
        }

        // Note: APK installation is handled by Tradefed target_preparer in chdts.xml
    }

    private void runTest(String className) throws Exception {
        String fullClassName = String.format("%s.%s", TEST_CLASS_PACKAGE, className);

        DeviceTestRunOptions deviceTestRunOptions = new DeviceTestRunOptions(PACKAGE);

        deviceTestRunOptions.setTestClassName(fullClassName);

        // Avoid placing files in isolated storage (b/124903752)
        deviceTestRunOptions.setDisableIsolatedStorage(true);

        if (mStaticNanoapps) {
            deviceTestRunOptions.addInstrumentationArg(
                    "static_nanoapps", String.valueOf(mStaticNanoapps));
        }

        if (mExternalNanoAppPath != null) {
            deviceTestRunOptions.addInstrumentationArg("externalNanoAppPath", mExternalNanoAppPath);
        }

        if (className.equals("GtsContextHubStressTest") && mStressTestDurationSeconds != null) {
            deviceTestRunOptions.addInstrumentationArg(
                    "stressTestDurationSeconds", mStressTestDurationSeconds);
            try {
                // Set the stress test timeout to mStressTestDurationSeconds + 60s to make
                // sure the stress test is not interrupted by the test timeout.
                long stressTestTimeoutMs =
                        (Long.parseLong(mStressTestDurationSeconds) + 60) * 1000L;
                if (stressTestTimeoutMs > 0) {
                    deviceTestRunOptions.setTestTimeoutMs(stressTestTimeoutMs);
                    deviceTestRunOptions.setMaxTimeToOutputMs(stressTestTimeoutMs);
                }
            } catch (NumberFormatException e) { }
        }

        Assert.assertTrue(fullClassName + " failed.", runDeviceTests(deviceTestRunOptions));
    }

    private boolean hasSensor(String sensorFeatureName) throws Exception {
        return mDevice.hasFeature(sensorFeatureName);
    }

    // --- Tests below ---
    @Test
    public void testContextHubEnabled() throws Exception {
        Assert.assertTrue("ContextHub feature not found.", mDevice.hasFeature(FEATURE_CONTEXT_HUB));
    }

    @Test
    public void testContextHubBusyStartupNanoAppTest() throws Exception {
        runTest("GtsContextHubBusyStartupNanoAppTest");
    }

    @Test
    public void testContextHubEstimatedHostTimeTest() throws Exception {
        runTest("GtsContextHubEstimatedHostTimeTest");
    }

    @Test
    public void testContextHubEventBetweenAppsNanoAppTest() throws Exception {
        runTest("GtsContextHubEventBetweenAppsNanoAppTest");
    }

    @Test
    public void testContextHubGetTimeNanoAppTest() throws Exception {
        runTest("GtsContextHubGetTimeNanoAppTest");
    }

    @Test
    public void testContextHubLoadBadNanoAppTest() throws Exception {
        runTest("GtsContextHubLoadBadNanoAppTest");
    }

    @Test
    public void testContextHubSendMessageToHostNanoAppTest() throws Exception {
        runTest("GtsContextHubSendMessageToHostNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHelloWorldNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleHelloWorldNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHeapAllocStressNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleHeapAllocStressNanoAppTest");
    }

    @Test
    public void testContextHubSimpleSendEventNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleSendEventNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicAccelerometerNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicAccelerometerNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicInstantMotionDetectNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicInstantMotionDetectNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicStationaryDetectNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicStationaryDetectNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicGyroscopeNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicGyroscopeNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicMagnetometerNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicMagnetometerNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicBarometerNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicBarometerNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicLightSensorNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicLightSensorNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicProximityNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicProximityNanoAppTest");
    }

    @Test
    public void testContextHubSimpleVersionConsistencyNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleVersionConsistencyNanoAppTest");
    }

    @Test
    public void testContextHubSimpleLoggingConsistencyNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleLoggingConsistencyNanoAppTest");
    }

    @Test
    public void testContextHubSimpleTimerSetNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleTimerSetNanoAppTest");
    }

    @Test
    public void testContextHubSimpleTimerCancelNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleTimerCancelNanoAppTest");
    }

    @Test
    public void testContextHubSimpleTimerStressNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleTimerStressNanoAppTest");
    }

    @Test
    public void testContextHubSimpleSendEventStressNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleSendEventStressNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHeapExhaustionStabilityNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleHeapExhaustionStabilityNanoAppTest");
    }

    @Test
    public void testContextHubSimpleGnnsCapabilitiesNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleGnnsCapabilitiesNanoAppTest");
    }

    @Test
    public void testContextHubSimpleWifiCapabilitiesNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleWifiCapabilitiesNanoAppTest");
    }

    @Test
    public void testContextHubSimpleWwanCapabilitiesNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleWwanCapabilitiesNanoAppTest");
    }

    @Test
    public void testContextHubSimpleSensorInfoNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleSensorInfoNanoAppTest");
    }

    @Test
    public void testContextHubSimpleWwanCellInfoNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleWwanCellInfoNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicAudioTestNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicAudioTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHostAwakeSuspendNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleHostAwakeSuspendNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicGnssTestNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicGnssTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicWifiTestNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicWifiTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicSensorFlushAsyncTestNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicSensorFlushAsyncTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicBleTestNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleBasicBleTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHeapAllocNanoAppTest() throws Exception {
        runTest("GtsContextHubSimpleHeapAllocNanoAppTest");
    }

    @Test
    public void testContextHubTrivialNanoAppsTest() throws Exception {
        runTest("GtsContextHubTrivialNanoAppsTest");
    }

    @Test
    public void testContextHubNanoAppInfoByIdTests() throws Exception {
        runTest("GtsContextHubNanoAppInfoByIdTests");
    }

    @Test
    public void testContextHubNanoAppInfoEventsNanoAppTest() throws Exception {
        runTest("GtsContextHubNanoAppInfoEventsNanoAppTest");
    }

    @Test
    public void testContextHubLoadUnloadNanoAppTest() throws Exception {
        if (mHasServiceFeature) runTest("GtsContextHubLoadUnloadNanoAppTest");
    }

    @Test
    public void testContextHubConcurrentLoadNanoAppTest() throws Exception {
        if (mHasServiceFeature) runTest("GtsContextHubConcurrentLoadNanoAppTest");
    }

    @Test
    public void testContextHubQueuedLoadUnloadNanoAppTest() throws Exception {
        if (mHasServiceFeature) runTest("GtsContextHubQueuedLoadUnloadNanoAppTest");
    }

    @Test
    public void testContextHubClientMessageTest() throws Exception {
        if (mHasServiceFeature) runTest("GtsContextHubClientSendMessageTest");
    }

    @Test
    public void testContextHubPendingIntentTest() throws Exception {
        if (mHasPendingIntentFeature) runTest("GtsContextHubPendingIntentTest");
    }

    @Test
    public void testWifiSettingsTest() throws Exception {
        if (mRunSettingsTest) runTest("GtsContextHubWifiSettingsTest");
    }

    @Test
    public void testBleSettingsTest() throws Exception {
        if (mRunBleSettingsTest) runTest("GtsContextHubBleSettingsTest");
    }

    @Test
    public void testGnssSettingsTest() throws Exception {
        if (mRunSettingsTest) runTest("GtsContextHubGnssSettingsTest");
    }

    @Test
    public void testWwanSettingsTest() throws Exception {
        if (mRunSettingsTest) runTest("GtsContextHubWwanSettingsTest");
    }

    @Test
    public void testMicDisableSettingsTest() throws Exception {
        if (mRunMicDisableSettingsTest) runTest("GtsContextHubMicDisableSettingsTest");
    }

    @Test
    public void testContextHubCrossValidationSensorAccelerometerTest() throws Exception {
        if (mRunCrossValidationTest && hasSensor(SENSOR_FEATURE_NAME_ACCELEROMETER)) {
            runTest("GtsContextHubCrossValidationSensorAccelerometerTest");
        }
    }

    @Test
    public void testContextHubCrossValidationSensorGyroscopeTest() throws Exception {
        if (mRunCrossValidationTest && hasSensor(SENSOR_FEATURE_NAME_GYROSCOPE)) {
            runTest("GtsContextHubCrossValidationSensorGyroscopeTest");
        }
    }

    @Test
    public void testContextHubCrossValidationSensorMagneticFieldTest() throws Exception {
        if (mRunCrossValidationTest && hasSensor(SENSOR_FEATURE_NAME_MAGNETIC_FIELD)) {
            runTest("GtsContextHubCrossValidationSensorMagneticFieldTest");
        }
    }

    @Test
    public void testContextHubCrossValidationSensorPressureTest() throws Exception {
        if (mRunCrossValidationTest && hasSensor(SENSOR_FEATURE_NAME_PRESSURE)) {
            runTest("GtsContextHubCrossValidationSensorPressureTest");
        }
    }

    @Test
    public void testContextHubCrossValidationSensorLightTest() throws Exception {
        if (mRunCrossValidationTest && hasSensor(SENSOR_FEATURE_NAME_LIGHT)) {
            runTest("GtsContextHubCrossValidationSensorLightTest");
        }
    }

    @Test
    public void testContextHubCrossValidationSensorProximityTest() throws Exception {
        if (mRunCrossValidationTest && hasSensor(SENSOR_FEATURE_NAME_PROXIMITY)) {
            runTest("GtsContextHubCrossValidationSensorProximityTest");
        }
    }

    @Test
    public void testContextHubCrossValidationSensorStepCounterTest() throws Exception {
        if (mRunCrossValidationTest && hasSensor(SENSOR_FEATURE_NAME_STEP_COUNTER)) {
            runTest("GtsContextHubCrossValidationSensorStepCounterTest");
        }
    }

    @Test
    public void testAudioConcurrencyTest() throws Exception {
        if (mRunAudioConcurrencyTest) runTest("GtsContextHubAudioConcurrencyTest");
    }

    @Test
    public void testBleConcurrencyTest() throws Exception {
        if (mRunBleConcurrencyTest) runTest("GtsContextHubBleConcurrencyTest");
    }

    @Test
    public void testChreConcurrencyTest() throws Exception {
        if (mRunChreConcurrencyTest) runTest("GtsContextHubChreConcurrencyTest");
    }

    @Test
    public void testContextHubCrossValidationWifiTest() throws Exception {
        if (mRunCrossValidationWifiTest) runTest("GtsContextHubCrossValidationWifiTest");
    }

    @Test
    public void testContextHubCrossValidationWwanTest() throws Exception {
        if (mRunCrossValidationWwanTest) runTest("GtsContextHubCrossValidationWwanTest");
    }

    @Test
    public void testContextHubFrameworkPermissionsTest() throws Exception {
        if (mRunPermissionTest) runTest("GtsContextHubFrameworkPermissionsTest");
    }

    @Test
    public void testPermissionsGatedApiTest() throws Exception {
        if (mRunPermissionTest) runTest("GtsContextHubChrePermissionsTest");
    }

    @Test
    public void testRpcServiceTest() throws Exception {
        if (mRunRpcServiceTest) runTest("GtsContextHubRpcServiceTest");
    }

    @Test
    public void testNanoAppRequirementsTest() throws Exception {
        if (mRunNanoAppRequirementsTest) runTest("GtsContextHubNanoAppRequirementsTest");
    }

    @Test
    public void testHostEndpointTest() throws Exception {
        if (mRunHostEndpointTest) runTest("GtsContextHubHostEndpointTest");
    }

    @Test
    public void testContextHubReliableMessage() throws Exception {
        if (mRunReliableMessageTest) runTest("GtsContextHubReliableMessageNanoAppTest");
    }

    @Test
    public void testContextHubEndpointEcho() throws Exception {
        if (mRunEndpointTests) runTest("GtsContextHubEndpointEchoTest");
    }

    @Test
    public void testContextHubStress() throws Exception {
        if (mRunStressTests) runTest("GtsContextHubStressTest");
    }
}
