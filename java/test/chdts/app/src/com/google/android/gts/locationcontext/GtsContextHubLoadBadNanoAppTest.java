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
import android.os.Build;

import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

/**
 * These are a series of tests which attempt to load various invalid NanoApps.
 *
 * We want to make sure these fail to load, and the system doesn't throw
 * unexpected exceptions or crash while attempting this.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubLoadBadNanoAppTest {
    @Rule public final ExpectedException thrown = ExpectedException.none();

    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final String TAG = "ContextHubLoadBadNanoAppTest";

    private static final long APP_ID = -1;
    private static final String APP_NAME = "Bad app";
    private static final String APP_PUBLISHER = "GTS Testing";

    private GtsContextHubManagerWrapper mContextHubManager = null;

    /**
     * Test specific setup
     */
    @Before
    public void initContextHubManager() {
        mContextHubManager = new GtsContextHubManagerWrapper(null);
    }

    /**
     * Test specific teardown
     */
    @After
    public void uninitContextHubManager() {
        if (mContextHubManager != null) {
            mContextHubManager.close();
        }
    }

    private void loadNanoApp(NanoApp app, int expected) {
        // TODO(b/467212059): We should test across all hubs when ContextHubManager is
        //     fixed up.
        int result = mContextHubManager.loadNanoApp(app);
        Assert.assertEquals("Unexpected result of loadNanoApp()",
                            expected, result);
    }


    /**
     * Confirm that we get an exception when a NanoApp doesn't have
     * setAppBinary() called on it.
     */
    @Test
    public void noAppBinary() {
        thrown.expect(IllegalStateException.class);

        NanoApp app = new NanoApp();
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
            app.setAppId((int) APP_ID);
        } else {
            // long version of this method replaces the incorrect int version
            // used in N
            app.setAppId(APP_ID);
        }
        loadNanoApp(app, -1);
    }

    /**
     * Confirm that we get an exception when a NanoApp doesn't have
     * setAppId() called on it.
     */
    @Test
    public void noAppId() {
        thrown.expect(IllegalStateException.class);

        NanoApp app = new NanoApp();
        app.setAppBinary(new byte[0]);
        loadNanoApp(app, -1);
    }


    private void tryInvalidBinary(int byteCount) {
        byte[] ff = new byte[byteCount];
        java.util.Arrays.fill(ff, (byte) 0xFF);

        NanoApp app;
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
            app = new NanoApp((int) APP_ID, ff);
        } else {
            // long version of this constructor replaces the incorrect int
            // version used in N
            app = new NanoApp(APP_ID, ff);
        }
        app.setName(APP_NAME);
        app.setPublisher(APP_PUBLISHER);

        loadNanoApp(app, -1);
    }

    /**
     * Confirm that we properly reject a NanoApp with an empty binary.
     */
    @Test
    public void emptyAppBinary() {
        tryInvalidBinary(0);
    }

    /**
     * Confirm that we fail to load a NanoApp with a small invalid binary.
     */
    @Test
    public void smallInvalidAppBinary() {
        tryInvalidBinary(64);
    }

    /**
     * Confirm that we fail to load a NanoApp with a large invalid binary.
     */
    @Test
    public void largeInvalidAppBinary() {
        tryInvalidBinary(8192);
    }
}
