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
    private boolean mHasFeature;
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

    @Option(name = "externalNanoAppPath",
            description = "The path to the directory which contains test nanoapps "
            + "for ContextHubHostTest")
    private String mExternalNanoAppPath = null;

    @Before
    public void setUp() throws Exception {
        mDevice = getDevice();

        boolean hasContextHub = mDevice.hasFeature(FEATURE_CONTEXT_HUB);

        mHasFeature = ApiLevelUtil.isAtLeast(mDevice, NYC) && hasContextHub;

        mHasServiceFeature = ApiLevelUtil.isAtLeast(mDevice, PI) && hasContextHub;

        mHasPendingIntentFeature = ApiLevelUtil.isAtLeast(mDevice, QT) && hasContextHub;

        mRunSettingsTest = ApiLevelUtil.isAtLeast(mDevice, RVC) && hasContextHub;

        mRunMicDisableSettingsTest = ApiLevelUtil.isAtLeast(mDevice, SC) && hasContextHub;

        mRunCrossValidationTest = ApiLevelUtil.isAtLeast(mDevice, RVC) && hasContextHub
                && PropertyUtil.getFirstApiLevel(mDevice) >= RVC;

        mRunCrossValidationWifiTest = ApiLevelUtil.isAtLeast(mDevice, RVC) && hasContextHub
                && PropertyUtil.getFirstApiLevel(mDevice) >= SC;

        mRunAudioConcurrencyTest = ApiLevelUtil.isAtLeast(mDevice, RVC) && hasContextHub;

        mRunPermissionTest = ApiLevelUtil.isAtLeast(mDevice, SC) && hasContextHub
                && PropertyUtil.getFirstApiLevel(mDevice) >= SC;

        boolean runUdcTests = ApiLevelUtil.isAtLeast(mDevice, UDC) && hasContextHub
                && PropertyUtil.getFirstApiLevel(mDevice) >= UDC
                && PropertyUtil.getVendorApiLevel(mDevice) >= UDC;

        mRunRpcServiceTest = runUdcTests;
        mRunNanoAppRequirementsTest = runUdcTests;
        mRunBleConcurrencyTest = runUdcTests;
        mRunBleSettingsTest = runUdcTests;
        mRunHostEndpointTest = runUdcTests;
        mRunChreConcurrencyTest = runUdcTests;

        boolean runVicTests = ApiLevelUtil.isAtLeast(mDevice, VIC) && hasContextHub
                && PropertyUtil.getFirstApiLevel(mDevice) >= VIC
                && PropertyUtil.getVendorApiLevel(mDevice) >= VIC;
        mRunReliableMessageTest = runVicTests;

        boolean runBaklavaTests = ApiLevelUtil.isAtLeast(mDevice, BAKLAVA)
                && hasContextHub
                && PropertyUtil.getFirstApiLevel(mDevice) >= BAKLAVA
                && PropertyUtil.getVendorApiLevel(mDevice) >= BAKLAVA;
        mRunEndpointTests = runBaklavaTests;
        mRunCrossValidationWwanTest = runBaklavaTests;

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

        if (mExternalNanoAppPath != null) {
            deviceTestRunOptions.addInstrumentationArg("externalNanoAppPath", mExternalNanoAppPath);
        }

        Assert.assertTrue(fullClassName + " failed.", runDeviceTests(deviceTestRunOptions));

        if (mExternalNanoAppPath != null && PropertyUtil.isUserBuild(mDevice)) {
            Assert.fail("Cannot pass test when using external nanoapps for user build.");
        }
    }

    private boolean hasSensor(String sensorFeatureName) throws Exception {
        return mDevice.hasFeature(sensorFeatureName);
    }

    // --- Tests below ---

    @Test
    public void testContextHubBusyStartupNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubBusyStartupNanoAppTest");
    }

    @Test
    public void testContextHubEstimatedHostTimeTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubEstimatedHostTimeTest");
    }

    @Test
    public void testContextHubEventBetweenAppsNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubEventBetweenAppsNanoAppTest");
    }

    @Test
    public void testContextHubGetTimeNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubGetTimeNanoAppTest");
    }

    @Test
    public void testContextHubLoadBadNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubLoadBadNanoAppTest");
    }

    @Test
    public void testContextHubSendMessageToHostNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSendMessageToHostNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHelloWorldNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleHelloWorldNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHeapAllocStressNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleHeapAllocStressNanoAppTest");
    }

    @Test
    public void testContextHubSimpleSendEventNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleSendEventNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicAccelerometerNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicAccelerometerNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicInstantMotionDetectNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicInstantMotionDetectNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicStationaryDetectNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicStationaryDetectNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicGyroscopeNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicGyroscopeNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicMagnetometerNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicMagnetometerNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicBarometerNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicBarometerNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicLightSensorNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicLightSensorNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicProximityNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicProximityNanoAppTest");
    }

    @Test
    public void testContextHubSimpleVersionConsistencyNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleVersionConsistencyNanoAppTest");
    }

    @Test
    public void testContextHubSimpleLoggingConsistencyNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleLoggingConsistencyNanoAppTest");
    }

    @Test
    public void testContextHubSimpleTimerSetNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleTimerSetNanoAppTest");
    }

    @Test
    public void testContextHubSimpleTimerCancelNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleTimerCancelNanoAppTest");
    }

    @Test
    public void testContextHubSimpleTimerStressNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleTimerStressNanoAppTest");
    }

    @Test
    public void testContextHubSimpleSendEventStressNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleSendEventStressNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHeapExhaustionStabilityNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleHeapExhaustionStabilityNanoAppTest");
    }

    @Test
    public void testContextHubSimpleGnnsCapabilitiesNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleGnnsCapabilitiesNanoAppTest");
    }

    @Test
    public void testContextHubSimpleWifiCapabilitiesNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleWifiCapabilitiesNanoAppTest");
    }

    @Test
    public void testContextHubSimpleWwanCapabilitiesNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleWwanCapabilitiesNanoAppTest");
    }

    @Test
    public void testContextHubSimpleSensorInfoNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleSensorInfoNanoAppTest");
    }

    @Test
    public void testContextHubSimpleWwanCellInfoNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleWwanCellInfoNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicAudioTestNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicAudioTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHostAwakeSuspendNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleHostAwakeSuspendNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicGnssTestNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicGnssTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicWifiTestNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicWifiTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicSensorFlushAsyncTestNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicSensorFlushAsyncTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleBasicBleTestNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleBasicBleTestNanoAppTest");
    }

    @Test
    public void testContextHubSimpleHeapAllocNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubSimpleHeapAllocNanoAppTest");
    }

    @Test
    public void testContextHubTrivialNanoAppsTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubTrivialNanoAppsTest");
    }

    @Test
    public void testContextHubNanoAppInfoByIdTests() throws Exception {
        if (mHasFeature) runTest("GtsContextHubNanoAppInfoByIdTests");
    }

    @Test
    public void testContextHubNanoAppInfoEventsNanoAppTest() throws Exception {
        if (mHasFeature) runTest("GtsContextHubNanoAppInfoEventsNanoAppTest");
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
}
