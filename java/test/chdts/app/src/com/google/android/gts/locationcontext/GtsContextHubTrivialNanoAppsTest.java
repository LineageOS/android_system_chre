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

import android.hardware.location.NanoApp;

import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests of the absolutely most basic valid NanoApps.
 *
 * This tests a NanoApp which does nothing but fail its nanoappStart()
 * method, and a NanoApp which does nothing but succeed its nanoappStart().
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubTrivialNanoAppsTest {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final String TAG = "ContextHubTrivialNanoAppsTest";
    private static final String NANO_APP_PUBLISHER = "Google";

    private GtsContextHubManagerWrapper mContextHubManager = null;
    private int mNanoAppHandle;

    /**
     * Test specific set up
     */
    @Before
    public void initContextHubManager() {
        mContextHubManager = new GtsContextHubManagerWrapper(null);
    }

    /**
     * Test specific tear down
     */
    @After
    public void uninitContextHubManager() {
        if (mContextHubManager != null) {
            mContextHubManager.close();
        }
    }

    private void runTest(String nanoAppFilename, String nanoAppName,
                         boolean expectLoad) {
        NanoApp app = GtsContextHubNanoAppCreator.create(
                mContextHubManager.getContextHubInfo(),
                nanoAppFilename);
        // TODO(b/30808791): Confirm that app.getId()'s five most significant
        //     bytes are GtsContextHubManagerWrapper.GTS_VENDOR_ID.

        app.setName(nanoAppName);
        app.setPublisher(NANO_APP_PUBLISHER);

        int nanoAppHandle = mContextHubManager.loadNanoApp(app);
        if (expectLoad) {
            Assert.assertTrue("Failed to load " + nanoAppName,
                              nanoAppHandle != -1);
        } else {
            Assert.assertTrue("Incorrectly claimed successful load of " + nanoAppName,
                              nanoAppHandle == -1);
            // This test is done, since the nanoapp never loaded.
            return;
        }

        int result = mContextHubManager.unloadNanoApp(nanoAppHandle);
        Assert.assertEquals("Failed to unload nanoapp " + nanoAppName,
                            0, result);
    }

    /**
     * Confirm that a nanoapp which does nothing other than succeed its
     * nanoappStart() method properly loads and unloads.
     */
    @Test
    public void trivialNanoApp() {
        runTest("do_nothing.napp", "Do Nothing", true);
    }

    /**
     * Confirm that a nanoapp which does nothing other than fail its
     * nanoappStart() method is properly reported as failing to load.
     */
    @Test
    public void failOnStartupNanoApp() {
        runTest("fail_startup.napp", "Fail on startup", false);
    }
}
