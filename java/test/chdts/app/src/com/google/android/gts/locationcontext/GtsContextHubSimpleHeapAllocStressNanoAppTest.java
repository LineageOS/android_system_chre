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

import com.google.android.chre.test.chqts.ContextHubTestConstants.TestNames;

import org.junit.Rule;

public class GtsContextHubSimpleHeapAllocStressNanoAppTest
        extends GtsContextHubSimpleNanoAppTestBase {

    private static final String TAG = "GtsContextHubSimpleHeapAllocStressNanoAppTest";
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    @Override
    protected TestNames[] getTestNames() {
        return new TestNames[] {TestNames.HEAP_ALLOC_STRESS};
    }

    @Override
    protected long getTestTimeoutSeconds() {
        return STANDARD_TIMEOUT;
    }
}
