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

import java.util.concurrent.TimeoutException;

/** A test to see if we can load a nanoapp concurrently without unexpected system failures. */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubConcurrentLoadNanoAppTest extends GtsContextHubTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final int NUM_TEST_CYCLES = 10;
    private final NanoAppBinary mNanoAppBinary1;
    private final NanoAppBinary mNanoAppBinary2;
    private final ContextHubLoadAndUnloadNanoAppsTestExecutor mExecutor;

    public GtsContextHubConcurrentLoadNanoAppTest() {
        ContextHubInfo contextHubInfo = getContextHubInfo();
        mExecutor =
                new ContextHubLoadAndUnloadNanoAppsTestExecutor(
                        getContextHubManager(), contextHubInfo);
        mNanoAppBinary1 = createNanoAppBinary(contextHubInfo, "do_nothing.napp");
        mNanoAppBinary2 = createNanoAppBinary(contextHubInfo, "echo_message.napp");
    }

    @Before
    public void setUp() throws Exception {
        mExecutor.init();
    }

    @After
    public void unregisterMessageClient() {
        mExecutor.deinit();
    }

    /** Load and unload 2 nanoapps concurrently and verify that we can find them through a query. */
    @Test
    public void runTest() throws InterruptedException, TimeoutException {
        mExecutor.loadUnloadConcurrentTest(mNanoAppBinary1, mNanoAppBinary2, NUM_TEST_CYCLES);
    }
}
