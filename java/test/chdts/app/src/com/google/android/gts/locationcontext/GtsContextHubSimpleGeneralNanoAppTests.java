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
package com.google.android.gts.locationcontext;

import android.os.Build;
import android.util.Log;

import com.android.compatibility.common.util.PropertyUtil;

import com.google.android.chre.test.chqts.ContextHubTestConstants.MessageType;
import com.google.android.chre.test.chqts.ContextHubTestConstants.TestNames;
import com.google.android.utils.chre.ChreTestUtil;

import org.junit.Assume;
import org.junit.Rule;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * Collection of simple "general" tests.
 *
 * All of these tests have a "simple" protocol.  Specifically, they tell
 * the test to start with their name, and then expect to get back SUCCESS.
 *
 * Protocol:
 * Host:    mTestName, no data
 * Nanoapp: SUCCESS, no data
 */
@RunWith(Parameterized.class)
public class GtsContextHubSimpleGeneralNanoAppTests
        extends GtsContextHubGeneralNanoAppTestBase {
    private static final String TAG = "GtsContextHubSimpleGeneralNanoAppTests";
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private final TestNames mTestName;
    private final int mTimeoutSeconds;

    private static final int STANDARD_TIMEOUT = 5;  // seconds
    private static final int LONGER_TIMEOUT = 10;  // seconds;
    private static final int BASIC_WIFI_TIMEOUT = 30;  // seconds;
    private static final int BASIC_SENSOR_TIMEOUT = STANDARD_TIMEOUT;
    private static final int RESTRICT_SENSORS_WAIT = 500;  // milliseconds;

    // Cause the thread to sleep. Don't throw Exceptions.
    private void sleep(int millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            Log.d(TAG, "sleep interrupted");
        }
    }

    // Restrict sensors during test.
    private boolean shouldRestrictSensors() {
        // Do it for all tests where the sensor is also available through
        // the Android APIs
        List<TestNames> testList = Arrays.asList(
                TestNames.BASIC_ACCELEROMETER,
                TestNames.BASIC_BAROMETER,
                TestNames.BASIC_GYROSCOPE,
                TestNames.BASIC_LIGHT_SENSOR,
                TestNames.BASIC_MAGNETOMETER,
                TestNames.BASIC_PROXIMITY);

        return testList.contains(getTestNames()[0]);
    }

    @Override
    protected void loadAndStart() {
        if (shouldRestrictSensors()) {
            Log.d(TAG, "restrict sensors for " + getTestNames()[0] + " test");
            ChreTestUtil.restrictSensors("com.google.android.gts.locationcontext"
                    + ".GtsContextHubSimpleGeneralNanoAppTests");
            sleep(RESTRICT_SENSORS_WAIT);
        }
        super.loadAndStart();
    }

    @Override
    protected void unload() {
        super.unload();
        if (shouldRestrictSensors()) {
            ChreTestUtil.unrestrictSensors();
        }
    }

    /**
     * Provides data for parameterized JUnit4 test
     */
    @Parameters(name = "{0}")
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {
                { "HelloWorld", TestNames.HELLO_WORLD, STANDARD_TIMEOUT },
                { "HeapAllocStress", TestNames.HEAP_ALLOC_STRESS,
                  STANDARD_TIMEOUT },
                // SendEvent sends many events to itself, and needs a
                // little bit longer.
                { "SendEvent", TestNames.SEND_EVENT, LONGER_TIMEOUT },
                { "BasicAccelerometer", TestNames.BASIC_ACCELEROMETER,
                  BASIC_SENSOR_TIMEOUT },
                { "BasicInstantMotionDetect",
                  TestNames.BASIC_INSTANT_MOTION_DETECT,
                  BASIC_SENSOR_TIMEOUT },
                { "BasicStationaryDetect", TestNames.BASIC_STATIONARY_DETECT,
                  BASIC_SENSOR_TIMEOUT },
                { "BasicGyroscope", TestNames.BASIC_GYROSCOPE,
                  BASIC_SENSOR_TIMEOUT },
                { "BasicMagnetometer", TestNames.BASIC_MAGNETOMETER,
                  BASIC_SENSOR_TIMEOUT },
                { "BasicBarometer", TestNames.BASIC_BAROMETER,
                  BASIC_SENSOR_TIMEOUT },
                { "BasicLightSensor", TestNames.BASIC_LIGHT_SENSOR,
                  BASIC_SENSOR_TIMEOUT },
                { "BasicProximity", TestNames.BASIC_PROXIMITY,
                  LONGER_TIMEOUT },
                { "VersionConsistency", TestNames.VERSION_CONSISTENCY,
                  STANDARD_TIMEOUT },
                // If logging is expensive, this might need more time.
                { "LoggingConsistency", TestNames.LOGGING_CONSISTENCY,
                  LONGER_TIMEOUT },
                // The nanoapp takes at least 3 seconds before success.
                { "TimerSet", TestNames.TIMER_SET, LONGER_TIMEOUT },
                { "TimerCancel", TestNames.TIMER_CANCEL, STANDARD_TIMEOUT },
                // Depending on how many timers the system supports, this could
                // take a while.
                { "TimerStress", TestNames.TIMER_STRESS, LONGER_TIMEOUT },
                // Sends many events to itself, and needs a bit longer.
                { "SendEventStress", TestNames.SEND_EVENT_STRESS,
                  LONGER_TIMEOUT },
                // Takes at least 5 seconds within the nanoapp.
                { "HeapExhaustionStability",
                  TestNames.HEAP_EXHAUSTION_STABILITY, LONGER_TIMEOUT },
                { "GnssCapabilities",
                  TestNames.GNSS_CAPABILITIES, STANDARD_TIMEOUT },
                { "WifiCapabilities",
                  TestNames.WIFI_CAPABILITIES, STANDARD_TIMEOUT },
                { "WwanCapabilities",
                  TestNames.WWAN_CAPABILITIES, STANDARD_TIMEOUT },
                { "SensorInfoTest", TestNames.SENSOR_INFO, STANDARD_TIMEOUT },
                // The async response time is already 5 seconds.
                { "WwanCellInfo", TestNames.WWAN_CELL_INFO, LONGER_TIMEOUT},
                { "BasicAudioTest", TestNames.BASIC_AUDIO_TEST, LONGER_TIMEOUT},
                { "HostAwakeSuspend", TestNames.HOST_AWAKE_SUSPEND, STANDARD_TIMEOUT},
                { "BasicGnssTest", TestNames.BASIC_GNSS_TEST, STANDARD_TIMEOUT},
                { "BasicWifiTest", TestNames.BASIC_WIFI_TEST, BASIC_WIFI_TIMEOUT},
                { "BasicSensorFlushAsyncTest", TestNames.BASIC_SENSOR_FLUSH_ASYNC_TEST,
                  STANDARD_TIMEOUT},
                { "BasicBleTest", TestNames.BASIC_BLE_TEST, STANDARD_TIMEOUT},
        });
    }

    public GtsContextHubSimpleGeneralNanoAppTests(String strName,
                                                  TestNames testName,
                                                  int timeoutSeconds) {
        // We intentionally ignore strName; it's just used for naming within
        // the testing infrastructure.

        // BasicWifiTest fails on P for Taimen/Walleye (see b/161065339).
        // So skip the test when SDK version < Q.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            Assume.assumeTrue(testName != TestNames.BASIC_WIFI_TEST);
        }
        // Basic GNSS may fail for older devices. See b/187080086.
        if (testName == TestNames.BASIC_GNSS_TEST) {
            boolean firstApiCheck = (PropertyUtil.getFirstApiLevel() >= Build.VERSION_CODES.Q);
            boolean sdkCheck = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S);
            Assume.assumeTrue(firstApiCheck && sdkCheck);
        }
        mTestName = testName;
        mTimeoutSeconds = timeoutSeconds;
    }

    @Override
    protected void handleMessageFromNanoApp(
            TestNames testName, MessageType type, byte[] data) {
        // Since SUCCESS is handled by the framework, we don't expect any
        // messages to make it here.
        unexpectedMessageFailure(testName, type, data);
    }

    @Override
    protected TestNames[] getTestNames() {
        TestNames[] ret = { mTestName };
        return ret;
    }

    @Override
    protected long getTestTimeoutSeconds() {
        return mTimeoutSeconds;
    }
}
