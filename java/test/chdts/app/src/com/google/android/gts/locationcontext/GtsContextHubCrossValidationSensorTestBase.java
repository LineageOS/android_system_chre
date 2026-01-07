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

import com.google.android.chre.test.crossvalidator.ChreCrossValidatorSensor;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;

/**
 * Base class for all context hub cross validation tests.
 */
public abstract class GtsContextHubCrossValidationSensorTestBase extends GtsContextHubTestBase {

    private ChreCrossValidatorSensor mCrossValidator;

    /**
     * Setup the cross validator object used in the test methods with sensor type provided.
     *
     * @param sensorType One of the values described in Sensor class that defines a type of sensor.
     */
    protected void setUpCrossValidator(int sensorType) {
        ContextHubInfo contextHubInfo = getContextHubInfo();
        NanoAppBinary nappBinary = ContextHubHostTestUtil.createNanoAppBinary(contextHubInfo,
                "chre_cross_validator_sensor.napp");
        mCrossValidator = new ChreCrossValidatorSensor(
                getContextHubManager(), contextHubInfo, nappBinary, sensorType);
        mCrossValidator.init();
    }

    @Test
    public void runTest() throws InterruptedException {
        Assert.assertNotNull("mCrossValidator is null. Call setUpCrossValidator in @Before method.",
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
