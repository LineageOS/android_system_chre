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

import android.chre.flags.Flags;
import android.platform.test.annotations.RequiresFlagsEnabled;
import android.platform.test.flag.junit.CheckFlagsRule;
import android.platform.test.flag.junit.DeviceFlagsValueProvider;

import com.android.compatibility.common.util.GmsTest;

import com.google.android.chre.test.endpoint.ContextHubEndpointDataFlowExecutor;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

public class GtsContextHubEndpointDataFlowTest extends GtsContextHubServiceTestBase {
    @Rule
    public final CheckFlagsRule mCheckFlagsRule = DeviceFlagsValueProvider.createCheckFlagsRule();

    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private ContextHubEndpointDataFlowExecutor mExecutor;

    @Before
    public void setUp() {
        if (getContextHubInfo() != null) {
            mExecutor =
                    new ContextHubEndpointDataFlowExecutor(
                            getContextHubManager(),
                            getContextHubInfo(),
                            ContextHubHostTestUtil.createNanoAppBinary(
                                    getContextHubInfo(), "endpoint_echo_test.napp"));
        } else {
            mExecutor = new ContextHubEndpointDataFlowExecutor(getContextHubManager());
        }
        mExecutor.init();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_FMCQ_API)
    public void testDataFlow() throws Exception {
        mExecutor.testDataFlow();
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }
}
