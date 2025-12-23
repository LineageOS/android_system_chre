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

import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.NanoAppBinary;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.google.android.chre.test.chqts.ContextHubLoadAndUnloadNanoAppsTestExecutor;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * A test to see if we can load/unload a nanoapp multiple times without unexpected system failures.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubLoadUnloadNanoAppTest extends GtsContextHubTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private final ContextHubLoadAndUnloadNanoAppsTestExecutor mExecutor;
    private final NanoAppBinary mNanoAppBinary;

    public GtsContextHubLoadUnloadNanoAppTest() {
        ContextHubInfo contextHubInfo = getContextHubInfo();
        ContextHubManager contextHubManager = getContextHubManager();
        mNanoAppBinary =
                ContextHubHostTestUtil.createNanoAppBinary(contextHubInfo, "do_nothing.napp");
        mExecutor =
                new ContextHubLoadAndUnloadNanoAppsTestExecutor(contextHubManager, contextHubInfo);
    }

    @Before
    public void setUp() throws Exception {
        mExecutor.init();
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }

    /**
     * Repeatedly loads and unloads a nanoapp synchronously, and verifies that the naonapp is loaded
     * successfully.
     */
    @Test
    public void loadUnloadSyncTest() throws Exception {
        mExecutor.loadUnloadSyncTest(mNanoAppBinary);
    }

    /**
     * Repeatedly loads and unloads a nanoapp asynchronously, and verifies that the naonapp is
     * loaded successfully.
     */
    @Test
    public void loadUnloadAsyncTest() throws Exception {
        mExecutor.loadUnloadAsyncTest(mNanoAppBinary);
    }
}
