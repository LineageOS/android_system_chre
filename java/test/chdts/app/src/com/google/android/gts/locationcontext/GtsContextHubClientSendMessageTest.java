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

import android.hardware.location.ContextHubInfo;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.google.android.chre.test.chqts.ContextHubClientSendMessageTestExecutor;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * A test to see if we can send a message to a nanoapp that reflects the message back to the client,
 * and verify that we receive the same message.
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubClientSendMessageTest extends GtsContextHubTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final int NUM_TEST_CYCLES = 10;
    private static final int NUM_BURST_MESSAGES = 32;
    private static final int NUM_CLIENTS = 5;
    private static final int NUM_MESSAGES = 5;

    private final ContextHubClientSendMessageTestExecutor mExecutor;

    public GtsContextHubClientSendMessageTest() {
        ContextHubInfo contextHubInfo = getContextHubInfo();
        mExecutor =
                new ContextHubClientSendMessageTestExecutor(
                        getContextHubManager(),
                        contextHubInfo,
                        createNanoAppBinary(contextHubInfo, "echo_message.napp"));
    }

    @Before
    public void setUp() throws Exception {
        mExecutor.init();
    }

    /**
     * Sends a unicast message to an echo_message nanoapp, and verify that the client receives the
     * same message back.
     */
    @Test
    public void clientSendOneMessageTest() throws InterruptedException {
        mExecutor.testSingleMessage(NUM_TEST_CYCLES);
    }

    /**
     * Sends distinct short messages from a ContextHubClient to the echo_message nanoapp, and verify
     * that the client receives all messages back.
     */
    @Test
    public void clientShortBurstMessageTest() throws InterruptedException {
        mExecutor.testBurstMessages(NUM_TEST_CYCLES, NUM_BURST_MESSAGES);
    }

    /**
     * Sends different messages from {@link #NUM_CLIENTS} ContextHubClients concurrently to the
     * echo_message nanoapp, and verify that each client receives its own message back.
     */
    @Test
    public void clientConcurrentMessageTest() throws InterruptedException {
        mExecutor.testConcurrentMessages(NUM_TEST_CYCLES, NUM_CLIENTS, NUM_MESSAGES);
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }
}
