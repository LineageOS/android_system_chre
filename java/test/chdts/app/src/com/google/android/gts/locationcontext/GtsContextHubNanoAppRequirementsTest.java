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

import androidx.test.runner.AndroidJUnit4;

import com.google.android.chre.test.chqts.ContextHubNanoAppRequirementsTestExecutor;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * A test to check for requirements to run nanoapps.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubNanoAppRequirementsTest extends GtsContextHubTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final long ACTIVITY_RECOGNITION_NANOAPP_ID = 0x476f6f676c00100bL;
    private static final long CAR_CRASH_NANOAPP_ID = 0x476f6f676c00100dL;
    private static final long NEARBY_NANOAPP_ID = 0x476f6f676c001031L;

    private final ContextHubNanoAppRequirementsTestExecutor mExecutor =
            new ContextHubNanoAppRequirementsTestExecutor(
                    ContextHubHostTestUtil.createNanoAppBinary(getContextHubInfo(),
                            "chre_api_test.napp"));

    @Before
    public void setUp() {
        mExecutor.init();
    }

    @Test
    public void nanoAppRequirementsTest() throws Exception {
        if (mExecutor.isNanoappPreloaded(ACTIVITY_RECOGNITION_NANOAPP_ID)) {
            mExecutor.assertActivitySensors();
        }

        if (mExecutor.isNanoappPreloaded(CAR_CRASH_NANOAPP_ID)) {
            mExecutor.assertMovementSensors();
        }

        if (mExecutor.isNanoappPreloaded(NEARBY_NANOAPP_ID)) {
            mExecutor.assertBleSensors();
        }
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }
}
