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

import com.android.compatibility.common.util.PropertyUtil;

import com.google.android.chre.test.chqts.ContextHubTestConstants.TestNames;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;

public class GtsContextHubSimpleBasicGnssTestNanoAppTest
        extends GtsContextHubSimpleNanoAppTestBase {

    private static final String TAG = "GtsContextHubSimpleBasicGnssTestNanoAppTest";
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    @Override
    protected TestNames[] getTestNames() {
        return new TestNames[] {TestNames.BASIC_GNSS_TEST};
    }

    @Override
    protected long getTestTimeoutSeconds() {
        return STANDARD_TIMEOUT;
    }

    @Before
    public void assumeSupportedDevice() {
        boolean firstApiCheck = (PropertyUtil.getFirstApiLevel() >= Build.VERSION_CODES.Q);
        boolean sdkCheck = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S);
        Assume.assumeTrue(firstApiCheck && sdkCheck);
    }
}
