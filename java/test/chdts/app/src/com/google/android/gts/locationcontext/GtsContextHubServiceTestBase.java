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

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.content.Context;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;

import com.google.android.utils.chre.ContextHubHostTestUtil;

import java.util.List;

/**
 * A base class for all tests which are run on devices that support the Context Hub Service. Note
 * that starting 25Q2 release, a device can support the Context Hub Service but not have any Context
 * Hubs.
 */
public class GtsContextHubServiceTestBase {
    private final ContextHubInfo mContextHubInfo;
    private final ContextHubManager mContextHubManager;
    private final Context mContext;

    public GtsContextHubServiceTestBase() {
        mContext = getInstrumentation().getTargetContext();
        mContextHubManager = mContext.getSystemService(ContextHubManager.class);

        ContextHubHostTestUtil.checkDeviceShouldRunTest(mContext, mContextHubManager);
        List<ContextHubInfo> contextHubList = getContextHubManager().getContextHubs();
        mContextHubInfo = contextHubList.isEmpty() ? null : contextHubList.get(0);
    }

    /**
     * @return the ContextHubManager for this app
     */
    protected ContextHubManager getContextHubManager() {
        return mContextHubManager;
    }

    /**
     * @return the ContextHubInfo to use for all tests
     */
    protected ContextHubInfo getContextHubInfo() {
        return mContextHubInfo;
    }

    protected Context getContext() {
        return mContext;
    }
}
