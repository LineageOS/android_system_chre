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
import android.hardware.location.NanoAppBinary;

import com.google.android.chre.test.crossvalidator.ChreCrossValidatorWifi;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class GtsContextHubCrossValidationWifiTest extends GtsContextHubTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private ChreCrossValidatorWifi mCrossValidator;

    @Before
    public void setUp() {
        ContextHubInfo contextHubInfo = getContextHubInfo();
        NanoAppBinary nappBinary = ContextHubHostTestUtil.createNanoAppBinary(contextHubInfo,
                "chre_cross_validator_wifi.napp");
        mCrossValidator = new ChreCrossValidatorWifi(
                getContextHubManager(), contextHubInfo, nappBinary);
        mCrossValidator.init();
    }

    @Test
    public void runTest() throws InterruptedException {
        Assert.assertNotNull("mCrossValidator is null. setUp() method must not have been called.",
                             mCrossValidator);
        mCrossValidator.validate();
    }

    @After
    public void cleanUp() {
        if (mCrossValidator != null) {
            mCrossValidator.deinit();
        }
    }
}
