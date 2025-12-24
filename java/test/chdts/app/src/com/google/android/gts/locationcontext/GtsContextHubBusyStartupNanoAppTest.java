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

import static com.google.android.utils.chre.ContextHubHostTestUtil.createNanoAppBinary;

import com.google.android.chre.test.chqts.ContextHubBusyStartupTestExecutor;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

/**
 * Java side of the test confirming nanoappStart() can properly call various methods.
 *
 * <p>See the doc of {@link ContextHubBusyStartupTestExecutor} for what exactly is being tested.
 */
public class GtsContextHubBusyStartupNanoAppTest extends GtsContextHubTestBase {
    @Rule
    public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final long TIMEOUT_SECONDS = 10;

    private final ContextHubBusyStartupTestExecutor mExecutor =
            new ContextHubBusyStartupTestExecutor(getContextHubManager(), getContextHubInfo(),
                    createNanoAppBinary(
                            getContextHubInfo(), "busy_startup.napp"));

    @Before
    public void setUp() {
        mExecutor.init();
    }

    @Test
    public void busyStartupTest() throws InterruptedException {
        mExecutor.run(TIMEOUT_SECONDS);
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }
}
