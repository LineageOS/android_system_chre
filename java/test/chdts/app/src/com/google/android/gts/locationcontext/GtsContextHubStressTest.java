/*
 * Copyright (C) 2026 The Android Open Source Project
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

import androidx.test.runner.AndroidJUnit4;

import com.google.android.chre.test.stress.ContextHubStressTestExecutor;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Assume;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeUnit;

/**
 * A test to run and validate the CHRE stress test nanoapp.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubStressTest extends GtsContextHubTestBase {
    private static final String TAG = "GtsContextHubStressTest";

    private final ContextHubStressTestExecutor mExecutor =
            new ContextHubStressTestExecutor(getContextHubManager(), getContextHubInfo(),
                    ContextHubHostTestUtil.createNanoAppBinary(
                                    getContextHubInfo(), "chre_stress_test.napp"));
    private final long mDuration = ContextHubHostTestUtil.getStressTestDurationSeconds();

    @Test
    public void stressTest() throws InterruptedException {
        Assume.assumeTrue(
                "Stress test is disabled because duration is <= 0. Duration: " + mDuration,
                mDuration > 0);

        mExecutor.init();
        mExecutor.runStressTest(mDuration, TimeUnit.SECONDS);
    }

    @After
    public void deinit() {
        mExecutor.deinit();
    }
}
