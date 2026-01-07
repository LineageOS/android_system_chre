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

import androidx.test.runner.AndroidJUnit4;

import com.google.android.chre.test.chqts.ContextHubTestConstants.MessageType;
import com.google.android.chre.test.chqts.ContextHubTestConstants.TestNames;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.runner.RunWith;

/**
 * Performs simple allocation and freeing from the heap.
 *
 * Requires the host to send an additional message to tell us to free.
 *
 * Protocol:
 * Host:    SIMPLE_HEAP_ALLOC
 * Nanoapp: CONTINUE, no data
 * Host:    CONTINUE, no data
 * Nanoapp: SUCCESS, no data
 *
 * This is paired with nanoapps/general_test/simple_heap_alloc_test.h.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubSimpleHeapAllocNanoAppTest extends GtsContextHubGeneralNanoAppTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private boolean mFirstMessage = true;
    private static final TestNames TEST_NAME = TestNames.SIMPLE_HEAP_ALLOC;

    @Override
    protected void handleMessageFromNanoApp(
            TestNames testName, MessageType type, byte[] data) {
        if (type != MessageType.CONTINUE) {
            unexpectedMessageFailure(testName, type, data);
            return;
        }

        Assert.assertTrue("Multiple CONTINUE messages from nanoapp",
                          mFirstMessage);
        mFirstMessage = false;
        Assert.assertEquals("Expected 0 bytes data from nanoapp CONTINUE; "
                + "got " + data.length + " bytes",
                0, data.length);

        byte[] emptyData = new byte[0];
        sendMessageToNanoApp(TEST_NAME, MessageType.CONTINUE, emptyData);
    }

    @Override
    protected TestNames[] getTestNames() {
        TestNames[] ret = { TEST_NAME };
        return ret;
    }

    @Override
    protected long getTestTimeoutSeconds() {
        // If we haven't done our entire back and forth within five seconds,
        // something must be fundamentally wrong.
        return 5;
    }
}
