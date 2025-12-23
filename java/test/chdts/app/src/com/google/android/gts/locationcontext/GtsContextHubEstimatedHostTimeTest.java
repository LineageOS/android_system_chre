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

import static com.google.android.utils.chre.ContextHubHostTestUtil.createNanoAppBinary;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.google.android.chre.test.chqts.ContextHubEstimatedHostTimeTestExecutor;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Verify estimated host time from nanoapp.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubEstimatedHostTimeTest extends GtsContextHubTestBase {
    @Rule
    public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private final ContextHubEstimatedHostTimeTestExecutor mExecutor;
    private static final long TIMEOUT_SECONDS = 5;

    public GtsContextHubEstimatedHostTimeTest() {
        mExecutor =
                new ContextHubEstimatedHostTimeTestExecutor(getContextHubManager(),
                        getContextHubInfo(),
                        createNanoAppBinary(getContextHubInfo(), "general_test.napp"));
    }

    @Before
    public void setUp() {
        mExecutor.init();
    }

    @Test
    public void estimatedHostTimeTest() throws InterruptedException {
        mExecutor.run(TIMEOUT_SECONDS);
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }
}
