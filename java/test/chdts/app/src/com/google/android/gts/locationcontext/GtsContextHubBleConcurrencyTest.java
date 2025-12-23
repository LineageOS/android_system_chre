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

import com.google.android.chre.test.bleconcurrency.ContextHubBleConcurrencyTestExecutor;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * A test to ensure CHRE BLE works well with concurrent AP usage.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubBleConcurrencyTest extends GtsContextHubTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private final ContextHubBleConcurrencyTestExecutor mExecutor =
            new ContextHubBleConcurrencyTestExecutor(
                    ContextHubHostTestUtil.createNanoAppBinary(getContextHubInfo(),
                            "chre_api_test.napp"));

    @Before
    public void setUp() {
        mExecutor.init();
    }

    @Test
    public void bleConcurrencyTest() throws Exception {
        mExecutor.run();
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }
}
