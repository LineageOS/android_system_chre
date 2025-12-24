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
import android.platform.test.flag.junit.CheckFlagsRule;
import android.platform.test.flag.junit.DeviceFlagsValueProvider;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.google.android.chre.test.chqts.ContextHubReliableMessageTestExecutor;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Test sending reliable messages across the host and a nanoapp. */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubReliableMessageNanoAppTest extends GtsContextHubTestBase {
    private static final String TAG = "GtsContextHubReliableMessageNanoAppTest";

    @Rule
    public final GtsContextHubTestModeRule testModeRule =
            new GtsContextHubTestModeRule();

    @Rule
    public final CheckFlagsRule mCheckFlagsRule =
            DeviceFlagsValueProvider.createCheckFlagsRule();

    private final ContextHubReliableMessageTestExecutor mExecutor;

    private final ContextHubInfo mContextHubInfo;

    public GtsContextHubReliableMessageNanoAppTest() {
        mContextHubInfo = getContextHubInfo();
        mExecutor = new ContextHubReliableMessageTestExecutor(
                getContextHubManager(), mContextHubInfo,
                createNanoAppBinary(mContextHubInfo, "chre_reliable_message_test.napp"));
    }

    @Before
    public void setUp() throws Exception {
        mExecutor.init();
    }

    @Test
    public void maximumMessageSizeTest() throws InterruptedException {
        if (!mContextHubInfo.supportsReliableMessages()) {
            Log.i(TAG, "Skipping reliable message maximumMessageSizeTest");
            return;
        }

        mExecutor.maximumMessageSizeTest();
    }

    @Test
    public void clientSendOneMessageTest() throws InterruptedException {
        if (!mContextHubInfo.supportsReliableMessages()) {
            Log.i(TAG, "Skipping reliable message clientSendOneMessageTest");
            return;
        }

        mExecutor.messageToHostTest(/* numMessages= */ 1, mExecutor.DEFAULT_MESSAGE_SIZE);
    }

    @Test
    public void clientSendOneMaxSizeMessageTest() throws InterruptedException {
        if (!mContextHubInfo.supportsReliableMessages()) {
            Log.i(TAG, "Skipping reliable message clientSendOneMaxSizeMessageTest");
            return;
        }

        mExecutor.messageToHostTest(/* numMessages= */ 1, mExecutor.MAX_MESSAGE_SIZE);
    }

    @Test
    public void clientSendOneEmptyMessageTest() throws InterruptedException {
        if (!mContextHubInfo.supportsReliableMessages()) {
            Log.i(TAG, "Skipping reliable message clientSendOneEmptyMessageTest");
            return;
        }

        mExecutor.messageToHostTest(/* numMessages= */ 1, /* messageSize= */ 0);
    }

    @Test
    public void clientSendMultipleMessagesTest() throws InterruptedException {
        if (!mContextHubInfo.supportsReliableMessages()) {
            Log.i(TAG, "Skipping reliable message clientSendMultipleMessagesTest");
            return;
        }

        mExecutor.messageToHostTest(mExecutor.NUM_MESSAGES_TO_SEND,
                mExecutor.DEFAULT_MESSAGE_SIZE);
    }

    @Test
    public void hostSendOneMessageTest() throws InterruptedException {
        if (!mContextHubInfo.supportsReliableMessages()) {
            Log.i(TAG, "Skipping reliable message hostSendOneMessageTest");
            return;
        }

        mExecutor.messageToNanoappTest(/* numMessages= */ 1, mExecutor.DEFAULT_MESSAGE_SIZE);
    }

    @Test
    public void hostSendOneMaxSizeMessageTest() throws InterruptedException {
        if (!mContextHubInfo.supportsReliableMessages()) {
            Log.i(TAG, "Skipping reliable message hostSendOneMaxSizeMessageTest");
            return;
        }

        mExecutor.messageToNanoappTest(/* numMessages= */ 1, mExecutor.MAX_MESSAGE_SIZE);
    }

    @Test
    public void hostSendOneEmptyMessageTest() throws InterruptedException {
        if (!mContextHubInfo.supportsReliableMessages()) {
            Log.i(TAG, "Skipping reliable message hostSendOneEmptyMessageTest");
            return;
        }

        mExecutor.messageToNanoappTest(/* numMessages= */ 1, /* messageSize= */ 0);
    }

    @Test
    public void hostSendMultipleMessagesTest() throws InterruptedException {
        if (!mContextHubInfo.supportsReliableMessages()) {
            Log.i(TAG, "Skipping reliable message hostSendMultipleMessagesTest");
            return;
        }

        mExecutor.messageToNanoappTest(mExecutor.NUM_MESSAGES_TO_SEND,
                mExecutor.DEFAULT_MESSAGE_SIZE);
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }
}
