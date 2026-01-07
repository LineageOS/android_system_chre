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

import android.hardware.SensorPrivacyManager;
import android.hardware.SensorPrivacyManager.Sensors;
import android.os.Build;

import androidx.test.runner.AndroidJUnit4;

import com.google.android.chre.test.setting.ContextHubMicDisableSettingsTestExecutor;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * A test to check for behavior when MIC_DISABLE settings are changed.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubMicDisableSettingsTest extends GtsContextHubTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private final ContextHubMicDisableSettingsTestExecutor mExecutor =
            new ContextHubMicDisableSettingsTestExecutor(
                    ContextHubHostTestUtil.createNanoAppBinary(
                            getContextHubInfo(), "chre_settings_test.napp"));
    private boolean mSupportsMicToggle = false;

    @Before
    public void setUp() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            SensorPrivacyManager sensorPrivacyManager =
                    getContext().getSystemService(SensorPrivacyManager.class);
            if (sensorPrivacyManager != null) {
                mSupportsMicToggle = sensorPrivacyManager.supportsSensorToggle(Sensors.MICROPHONE);
            }
        } else {
            mSupportsMicToggle = false;
        }

        if (mSupportsMicToggle) {
            mExecutor.setUp();
        }
    }

    @Test
    public void micDisableSettingsTest() throws InterruptedException {
        if (mSupportsMicToggle) {
            mExecutor.runMicDisableSettingsTest();
        }
    }

    @After
    public void tearDown() {
        if (mSupportsMicToggle) {
            mExecutor.tearDown();
        }
    }
}
