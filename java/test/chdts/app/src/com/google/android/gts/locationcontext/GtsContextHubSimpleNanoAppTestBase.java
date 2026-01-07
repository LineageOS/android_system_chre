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

import android.util.Log;

import com.google.android.chre.test.chqts.ContextHubTestConstants.MessageType;
import com.google.android.chre.test.chqts.ContextHubTestConstants.TestNames;


public abstract class GtsContextHubSimpleNanoAppTestBase
        extends GtsContextHubGeneralNanoAppTestBase {

    private static final String TAG = "GtsContextHubSimpleNanoAppTestBase";
    protected static final int STANDARD_TIMEOUT = 5;  // seconds
    protected static final int LONGER_TIMEOUT = 10;  // seconds;
    protected static final int BASIC_WIFI_TIMEOUT = 30;  // seconds;
    protected static final int BASIC_SENSOR_TIMEOUT = STANDARD_TIMEOUT;
    protected static final int RESTRICT_SENSORS_WAIT = 500;  // milliseconds;


    protected void sleep(int millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            Log.d(TAG, "Sleep interrupted");
        }
    }

    @Override
    protected void handleMessageFromNanoApp(
            TestNames testName, MessageType type, byte[] data) {
        // Since SUCCESS is handled by the framework, we don't expect any
        // messages to make it here.
        unexpectedMessageFailure(testName, type, data);
    }
}
