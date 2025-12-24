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

import android.hardware.location.ContextHubInfo;
import android.hardware.location.NanoAppBinary;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.google.android.chre.test.chqts.ContextHubLoadAndUnloadNanoAppsTestExecutor;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * A test to see if we can enqueue load/unload async requests simultaneously, and verify that we can
 * receive results in the expected sequence.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubQueuedLoadUnloadNanoAppTest extends GtsContextHubTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final int NUM_TEST_CYCLES = 10;

    private final NanoAppBinary mNanoAppBinary;
    private final ContextHubLoadAndUnloadNanoAppsTestExecutor mExecutor;

    public GtsContextHubQueuedLoadUnloadNanoAppTest() {
        ContextHubInfo contextHubInfo = getContextHubInfo();
        mExecutor =
                new ContextHubLoadAndUnloadNanoAppsTestExecutor(
                        getContextHubManager(), contextHubInfo);
        mNanoAppBinary = createNanoAppBinary(contextHubInfo, "do_nothing.napp");
    }

    @Before
    public void registerLoadUnloadClient() throws Exception {
        mExecutor.init();
    }

    @After
    public void unregisterLoadUnloadClient() {
        mExecutor.deinit();
    }

    /**
     * Starts multiple load and unload transactions asynchronously (queued up at the service), and
     * verify all transactions succeed.
     */
    @Test
    public void runTest() throws InterruptedException {
        mExecutor.queuedLoadUnloadTest(mNanoAppBinary, NUM_TEST_CYCLES);
    }
}
