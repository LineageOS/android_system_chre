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

import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Verify chreConfigureNanoAppInfoEvents.
 *
 * Protocol:
 * There are two nanoapps here, so we'll talk in terms of the observer
 * and the performer. All data involving the host is sent little endian
 *
 * Host to observer: NANOAPP_INFO_EVENTS_OBSERVER, no data
 * observer to Host: CONTINUE
 * [Host starts performer]
 * performer to Host: CONTINUE, 64-bit app ID, 32-bit instance ID
 * [Host stops performer]
 * Host to observer: CONTINUE, performer's 32-bit instance ID
 * observer to host: SUCCESS
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubNanoAppInfoEventsNanoAppTest
        extends GtsContextHubGeneralNanoAppTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final TestNames OBSERVER =
            TestNames.NANO_APP_INFO_EVENTS_OBSERVER;
    private static final TestNames PERFORMER =
            TestNames.NANO_APP_INFO_EVENTS_PERFORMER;

    private ExecutorService mExecutorService = Executors.newCachedThreadPool();

    private class MessageHandler implements Runnable {
        private TestNames mTestName;
        private MessageType mType;
        private byte[] mData;

        MessageHandler(TestNames testName, MessageType type, byte[] data) {
            mTestName = testName;
            mType = type;
            mData = data;
        }

        @Override
        public void run() {
            if (mType != MessageType.CONTINUE) {
                unexpectedMessageFailure(mTestName, mType, mData);
            }

            if (mTestName == OBSERVER) {
                // Blocks until receiving success/failure
                loadSingleNanoApp(PERFORMER, 1);

                // Send message to have the PERFORMER actually start
                sendMessageToNanoApp(PERFORMER, PERFORMER.asInt(),
                        new byte[0] /* data */);
            } else {
                ByteBuffer buffer = ByteBuffer.wrap(mData)
                        .order(ByteOrder.LITTLE_ENDIAN);

                int performerInstanceId = 0;

                try {
                    // Ignore the App id
                    buffer.getLong();
                    performerInstanceId = buffer.getInt();
                } catch (BufferUnderflowException ex) {
                    Assert.fail("Not enough data provided in CONTINUE message");
                }

                Assert.assertFalse("Too much data provided in CONTINUE message",
                        buffer.hasRemaining());

                buffer = ByteBuffer.allocate(4)
                        .order(ByteOrder.LITTLE_ENDIAN)
                        .putInt(performerInstanceId);

                // Blocks until receiving success/failure
                unloadSingleNanoApp(PERFORMER);

                sendMessageToNanoApp(OBSERVER, MessageType.CONTINUE, buffer.array());
            }
        }
    }

    /**
     * Handle processing of individual nanoapp messages in separate threads.
     *
     * This is necessary for allowing programmatic starting/stopping of
     * additional nanoapps. Such calls block the calling thread while waiting
     * for ContextHubOsMessages to indicate success/failure to be processed
     */
    @Override
    protected void handleMessageFromNanoApp(TestNames testName,
            MessageType type, byte[] data) {
        mExecutorService.submit(new MessageHandler(testName, type, data));
    }

    @Override
    protected TestNames[] getTestNames() {
        TestNames[] ret = { OBSERVER };

        return ret;
    }

    @Override
    protected long getTestTimeoutSeconds() {
        // Use a longer timeout for this test since it loads a nanoapp after
        // starting the test.
        return TimeUnit.SECONDS.toSeconds(20);
    }
}
