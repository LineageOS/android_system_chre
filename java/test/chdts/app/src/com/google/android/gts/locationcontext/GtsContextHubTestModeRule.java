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
import android.os.Build.VERSION_CODES;
import android.util.Log;

import com.android.compatibility.common.util.PropertyUtil;

import com.google.android.utils.chre.ContextHubServiceTestHelper;

import org.junit.rules.ExternalResource;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * A rule that enables and disables test mode before and after tests. This rule
 * should be added to all tests in CHQTS as a @Rule.
 */
public class GtsContextHubTestModeRule extends ExternalResource {
    private final ContextHubManager mContextHubManager;

    private static final String TAG = "GtsContextHubTestModeRule";

    private static final long MTK_PLATFORM_ID_PREFIX = 0x476F6F676C003000L;
    private static final long MTK_PLATFORM_ID_MASK   = 0xFFFFFFFFFFFFF000L; // 52 bits

    public GtsContextHubTestModeRule() {
        Context context = getInstrumentation().getTargetContext();
        mContextHubManager = context.getSystemService(ContextHubManager.class);
    }

    /**
     * Returns true if the vendor API is at least running U.
     *
     * <p>Note that the system's version can be newer than the vendor's version but not the other
     * way around.
     */
    private static boolean isVendorAtLeastU() {
        return PropertyUtil.getVendorApiLevel() >= VERSION_CODES.UPSIDE_DOWN_CAKE;
    }

    /**
     * Unloads all the nanoapps.
     *
     * <p>For vendor api level less than U or MTK devices running U the nanoapps are unloaded
     * directly because they don't support the test mode. Otherwise, test mode is used.
     */
    private void unloadAllNanoappsForTests()
            throws InterruptedException, TimeoutException {
        if (mContextHubManager != null) {
            int vendorApiLevel = PropertyUtil.getVendorApiLevel();
            List<ContextHubInfo> hubs = mContextHubManager.getContextHubs();
            boolean isMtkDevice = hubs.stream()
                    .anyMatch(hub -> (hub.getChrePlatformId() & MTK_PLATFORM_ID_MASK)
                            == MTK_PLATFORM_ID_PREFIX);
            boolean unloadNanoappsForMTkInU =
                    vendorApiLevel == VERSION_CODES.UPSIDE_DOWN_CAKE && isMtkDevice;

            if (vendorApiLevel < VERSION_CODES.UPSIDE_DOWN_CAKE || unloadNanoappsForMTkInU) {
                for (ContextHubInfo info : hubs) {
                    ContextHubServiceTestHelper helper = new ContextHubServiceTestHelper(info,
                            mContextHubManager);
                    helper.unloadAllNanoApps();
                }
            } else {
                if (!mContextHubManager.enableTestMode()) {
                    Log.e(TAG, "Failed to enable test mode");
                }
            }
        }
    }

    @Override
    protected void before() throws Throwable {
        unloadAllNanoappsForTests();
    }

    @Override
    protected void after() {
        if (mContextHubManager != null && isVendorAtLeastU()) {
            if (!mContextHubManager.disableTestMode()) {
                Log.e(TAG, "Failed to disable test mode");
            }
        }
    }
}
