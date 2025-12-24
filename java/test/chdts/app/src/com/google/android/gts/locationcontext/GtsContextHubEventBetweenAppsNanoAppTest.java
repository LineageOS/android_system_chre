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

/**
 * Verify sending a CHRE event between two nanoapps.
 *
 * We also check that we get the correct App IDs, and valid
 * instance IDs, for these two nanoapps.
 *
 * Protocol:
 * There are two nanoapps here, so we'll talk in term of app0 and app1.
 * All data involving the host is sent little endian.
 *
 * Host to app0:  EVENT_BETWEEN_APPS0, no data
 * Host to app1:  EVENT_BETWEEN_APPS1, no data
 * app0 to host:  CONTINUE, 64-bit app ID, 32-bit instance ID
 * app1 to host:  CONTINUE, 64-bit app ID, 32-bit instance ID
 * Host to app1:  CONTINUE, app0's 32-bit instance ID
 * Host to app0:  CONTINUE, app1's 32-bit instance ID
 * [app0 sends message to app1]
 * app1 to host:  SUCCESS
 */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubEventBetweenAppsNanoAppTest extends GtsContextHubGeneralNanoAppTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private static final TestNames TEST_NAME0 = TestNames.EVENT_BETWEEN_APPS0;
    private static final TestNames TEST_NAME1 = TestNames.EVENT_BETWEEN_APPS1;

    private class AppInfo {
        public long appId;
        public int instanceId;
        public boolean haveInfo = false;
    };
    private AppInfo[] mAppInfo = new AppInfo[] { new AppInfo(), new AppInfo() };

    @Override
    protected void handleMessageFromNanoApp(
            TestNames testName, MessageType type, byte[] data) {
        if (type != MessageType.CONTINUE) {
            unexpectedMessageFailure(testName, type, data);
        }
        int index;
        if (testName == TestNames.EVENT_BETWEEN_APPS0) {
            index = 0;
        } else {
            Assert.assertEquals(testName, TestNames.EVENT_BETWEEN_APPS1);
            index = 1;
        }
        Assert.assertFalse("Multiple CONTINUE messages from app " + testName,
                           mAppInfo[index].haveInfo);

        ByteBuffer buffer = ByteBuffer.wrap(data)
                .order(ByteOrder.LITTLE_ENDIAN);
        try {
            mAppInfo[index].appId = buffer.getLong();
            mAppInfo[index].instanceId = buffer.getInt();
            mAppInfo[index].haveInfo = true;
        } catch (BufferUnderflowException e) {
            Assert.fail("Not enough data provided in CONTINUE message from " + testName);
        }
        Assert.assertFalse("Too much data provided in CONTINUE message from " + testName,
                           buffer.hasRemaining());

        int otherIndex = 1 - index;
        if (!mAppInfo[otherIndex].haveInfo) {
            // We need to wait to get the info from the other app.
            return;
        }

        // TODO(b/31727154): These will end up being the same app ID when
        // we fix this bug and check the test infrastructure; we'll need
        // to update these checks accordingly.
        Assert.assertEquals("Incorrect app ID given for " + TEST_NAME0,
                            GtsContextHubManagerWrapper.GTS_VENDOR_ID | 0x0,
                            mAppInfo[0].appId);
        Assert.assertEquals("Incorrect app ID given for " + TEST_NAME1,
                            GtsContextHubManagerWrapper.GTS_VENDOR_ID | 0x1,
                            mAppInfo[1].appId);

        Assert.assertTrue("Both nanoapps given identical instance IDs",
                          mAppInfo[0].instanceId != mAppInfo[1].instanceId);

        // Checks pass - we'll send the data down.
        buffer = ByteBuffer.allocate(4)
            .order(ByteOrder.LITTLE_ENDIAN)
            .putInt(mAppInfo[0].instanceId);
        sendMessageToNanoApp(TEST_NAME1, MessageType.CONTINUE, buffer.array());

        buffer.clear();
        buffer.putInt(mAppInfo[1].instanceId);
        sendMessageToNanoApp(TEST_NAME0, MessageType.CONTINUE, buffer.array());
    }

    @Override
    protected TestNames[] getTestNames() {
        TestNames[] ret = { TEST_NAME0, TEST_NAME1 };
        return ret;
    }

    @Override
    protected long getTestTimeoutSeconds() {
        // This should be an excessive amount of time.
        return 10;
    }
}
