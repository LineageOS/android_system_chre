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

import com.google.android.chre.test.chqts.ContextHubTestConstants.TestNames;
import com.google.android.utils.chre.ChreTestUtil;

import org.junit.Rule;

public class GtsContextHubSimpleBasicProximityNanoAppTest
        extends GtsContextHubSimpleNanoAppTestBase {

    private static final String TAG = "GtsContextHubSimpleBasicProximityNanoAppTest";
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    @Override
    protected TestNames[] getTestNames() {
        return new TestNames[] {TestNames.BASIC_PROXIMITY};
    }

    @Override
    protected long getTestTimeoutSeconds() {
        return LONGER_TIMEOUT;
    }

    @Override
    protected void loadAndStart() {
        Log.d(TAG, "restrict sensors for BASIC_PROXIMITY test");
        ChreTestUtil.restrictSensors("com.google.android.gts.locationcontext"
                    + ".GtsContextHubSimpleBasicProximityNanoAppTest");
        sleep(RESTRICT_SENSORS_WAIT);
        super.loadAndStart();
    }

    @Override
    protected void unload() {
        super.unload();
        ChreTestUtil.unrestrictSensors();
    }
}
