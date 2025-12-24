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

import android.hardware.Sensor;

import org.junit.Before;
import org.junit.Rule;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * A test of CHRE pressure sensor data as it compares to host data.
 */
@RunWith(JUnit4.class)
public final class GtsContextHubCrossValidationSensorPressureTest
        extends GtsContextHubCrossValidationSensorTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    @Before
    public void setUp() {
        setUpCrossValidator(Sensor.TYPE_PRESSURE);
    }
}

