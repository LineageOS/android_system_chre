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
import android.os.Build;
import android.platform.test.annotations.RequiresFlagsEnabled;
import android.platform.test.flag.junit.CheckFlagsRule;
import android.platform.test.flag.junit.DeviceFlagsValueProvider;

import androidx.test.filters.SdkSuppress;

import com.android.compatibility.common.util.GmsTest;

import com.google.android.chre.test.endpoint.ContextHubEndpointEchoExecutor;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

/** A class testing echo endpoint service. */
public class GtsContextHubEndpointEchoTest extends GtsContextHubServiceTestBase {
    @Rule
    public final CheckFlagsRule mCheckFlagsRule = DeviceFlagsValueProvider.createCheckFlagsRule();

    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private ContextHubEndpointEchoExecutor mExecutor;

    @Before
    public void setUp() {
        if (getContextHubInfo() != null) {
            mExecutor =
                    new ContextHubEndpointEchoExecutor(
                            getContextHubManager(),
                            getContextHubInfo(),
                            ContextHubHostTestUtil.createNanoAppBinary(
                                    getContextHubInfo(), "endpoint_echo_test.napp"));
        } else {
            mExecutor = new ContextHubEndpointEchoExecutor(getContextHubManager());
        }
        mExecutor.init();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testFindEndpointEchos() throws Exception {
        mExecutor.getEchoServiceList();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testRegisterEndpointDefault() throws Exception {
        mExecutor.testDefaultEndpointRegistration();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testOpenEndpointSession() throws Exception {
        mExecutor.testOpenEndpointSession();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testOpenCloseEndpointSession() throws Exception {
        mExecutor.testOpenCloseEndpointSession();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testEndpointMessaging() throws Exception {
        mExecutor.testEndpointMessaging();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testEndpointThreadedMessaging() throws Exception {
        mExecutor.testEndpointThreadedMessaging();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testEndpointDiscovery() throws Exception {
        mExecutor.testEndpointDiscovery();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testThreadedEndpointDiscovery() throws Exception {
        mExecutor.testThreadedEndpointDiscovery();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testEndpointIdDiscovery() throws Exception {
        mExecutor.testEndpointIdDiscovery();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testThreadedEndpointIdDiscovery() throws Exception {
        mExecutor.testThreadedEndpointIdDiscovery();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @RequiresFlagsEnabled(Flags.FLAG_OFFLOAD_API)
    public void testApplicationEchoService() throws Exception {
        mExecutor.testApplicationEchoService();
    }

    @Test
    @GmsTest(requirement = "GMS-6.17-001")
    @SdkSuppress(minSdkVersion = Build.VERSION_CODES.CINNAMON_BUN, codeName = "CinnamonBun")
    @RequiresFlagsEnabled(Flags.FLAG_GET_HUBS_API)
    public void testGetHubs() throws Exception {
        mExecutor.testGetHubs();
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }
}
