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

import android.hardware.location.ContextHubMessage;

import androidx.test.runner.AndroidJUnit4;

import com.google.android.chre.test.chqts.ContextHubTestConstants.MessageType;
import com.google.android.chre.test.chqts.ContextHubTestConstants.TestNames;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.runner.RunWith;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Check chreSendMessageToHost() works, along with an empty message from the
 * host to the nanoapp.
 *
 * Much of the logic for this test takes place within the nanoapp.  From
 * the Host side, we:
 * 1) Confirm we receive all of these message of various length
 * 2) Send one (edge-case) message after we've received everything
 * 3) Wait for the nanoapp to declare victory
 *
 * TODO(b/32114261): This code is a lot more complicated than it should
 *     be.  Specifically, the standard workaround for this bug involves
 *     putting more data within the 'message' to/from host/nanoapp.  But
 *     since we're specifically testing that data, we can't use that
 *     workaround.  When that workaround is gone, we can make this test
 *     much simpler.
 *
 * Protocol:
 * Host:    SEND_MESSAGE_TO_HOST, no data
 * Nanoapp: 3 bytes of 0xFE
 * Nanoapp: 3 bytes of 0xFE
 * Nanoapp: 3 bytes of 0xFE
 * Nanoapp: 3 bytes of 0xFE
 * Nanoapp: 0 bytes
 * Nanoapp: CONTINUE, 4 bytes (little endian) with `MessageMaxSize`
 * Nanoapp: `MessageMaxSize` bytes of 0xFE
 * Host:    0 bytes
 * Nanoapp: kSuccess
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubSendMessageToHostNanoAppTest extends GtsContextHubGeneralNanoAppTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final TestNames TEST_NAME = TestNames.SEND_MESSAGE_TO_HOST;
    private static final int SMALL_DATA_LENGTH = 3;
    private static final byte RAW_DATA_BYTE = (byte) 0xFE;

    // TODO(b/32114261): Remove this entire class.
    private class MyWrapper extends GtsContextHubManagerWrapper {
        boolean mSentStartMessage = false;

        // If a message has at least four bytes, and the first four bytes
        // are _not_ 0xFEFEFEFE, then it's a general protocol message.
        private boolean isGeneralProtocolMessage(byte[] data) {
            if (data.length < 4) {
                return false;
            }
            for (int i = 0; i < 4; i++) {
                if (data[i] != RAW_DATA_BYTE) {
                    return true;
                }
            }
            return false;
        }

        @Override
        protected ContextHubMessage hackMessageFromNanoappToHost(
                ContextHubMessage origMessage) {
            if (isGeneralProtocolMessage(origMessage.getData())) {
                // This is going to be something like FAILURE or SUCCESS
                // which we want our standard infrastructure to handle.
                return super.hackMessageFromNanoappToHost(origMessage);
            }
            // We'll use the CONTINUE message type for all messages from
            // the nanoapp, even though the nanoapp doesn't send that.
            origMessage.setMsgType(MessageType.CONTINUE.asInt());
            return origMessage;
        }

        @Override
        protected ContextHubMessage hackMessageFromHostToNanoapp(
                int messageType, byte[] origData) {
            if (!mSentStartMessage) {
                // Our start message, SEND_MESSAGE_TO_HOST, needs to
                // be sent via the standard workaround protocol.
                mSentStartMessage = true;
                return super.hackMessageFromHostToNanoapp(messageType,
                                                          origData);
            }
            // This should be our zero-length message, which we want to
            // send raw.
            return new ContextHubMessage(messageType, 0, origData);
        }

        MyWrapper() {
            super(getCallback());
        }
    }

    // TODO(b/32114261): Remove this.
    @Override
    protected GtsContextHubManagerWrapper createContextHubManager() {
        return new MyWrapper();
    }

    // This stage count is tracking the stage used within the nanoapp.  See
    // the nanoapp code for more details on this.  That will also make it
    // clearer why we skip one stage.
    private int mStageCount = 0;
    // We'll get this value from the nanoapp.
    private int mMessageMaxSize = 0;

    private void checkSmallData(byte[] data) {
        Assert.assertEquals("Wrong small data length in stage " + mStageCount,
                            SMALL_DATA_LENGTH, data.length);
        for (int i = 0; i < data.length; i++) {
            Assert.assertEquals("Bad data " + data[i] + " at index " + i
                    + " in stage " + mStageCount,
                    RAW_DATA_BYTE, data[i]);
        }
    }

    private void readMessageMaxSize(byte[] data) {
        mMessageMaxSize = ByteBuffer.wrap(data)
            .order(ByteOrder.LITTLE_ENDIAN)
            .getInt();
        Assert.assertTrue("Given bad max message size: " + mMessageMaxSize,
                          mMessageMaxSize > 0);
    }

    private void checkLargeData(byte[] data) {
        Assert.assertTrue("checkLargeData called without mMessageMaxSize",
                          mMessageMaxSize != 0);
        Assert.assertEquals("checkLargeData has bad data length",
                            mMessageMaxSize, data.length);
        for (int i = 0; i < data.length; i++) {
            Assert.assertEquals("Failed raw data check at index " + i,
                                RAW_DATA_BYTE, data[i]);
        }
    }

    private void sendEmptyMessage() {
        // Note that ContextHubService requires this to be non-null
        byte[] empty = new byte[0];
        sendMessageToNanoApp(TEST_NAME, MessageType.CONTINUE, empty);
    }

    @Override
    protected void handleMessageFromNanoApp(
            TestNames testName, MessageType type, byte[] data) {
        if (type != MessageType.CONTINUE) {
            unexpectedMessageFailure(TEST_NAME, type, data);
        }

        switch (mStageCount) {
            case 0:  // fall-through
            case 1:  // fall-through
            case 2:  // fall-through
            case 3:
                checkSmallData(data);
                break;

            case 4:
                Assert.assertEquals("Expected zero length data",
                                    0, data.length);
                break;

            case 5:
                readMessageMaxSize(data);
                // Note that we don't receive a message for stage 6, so we
                // skip it here.
                mStageCount++;
                break;

            case 7:
                checkLargeData(data);
                sendEmptyMessage();
                break;

            case 6:  // fall-through
            default:
                Assert.fail("Unexpected stage " + mStageCount);
                break;
        }
        // We expect the next message to be at the next stage.
        mStageCount++;
    }


    @Override
    protected TestNames[] getTestNames() {
        TestNames[] ret = { TEST_NAME };
        return ret;
    }

    @Override
    protected long getTestTimeoutSeconds() {
        // We have several messages coming from the nanoapp, and need to
        // wait for some back and forth traffic.  We give this a lot of time.
        return 15;
    }
}
