/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.google.android.chre.test.chqts;

import static com.google.common.truth.Truth.assertThat;

import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.NanoAppBinary;

import com.google.android.utils.chre.ChreTestUtil;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * A test executor to verify that the nanoapp version can be queried by App ID or Instance ID.
 *
 * <p>This test verifies that the nanoapp version retrieved by the host-side API matches the
 * nanoapp version retrieved by the nanoapp via CHRE APIs. The test nanoapp sends a message to
 * this executor, which then retrieves the nanoapp version using the host-side API and sends it
 * back to the nanoapp. The nanoapp then compares this version with the one it retrieved via CHRE
 * APIs and verifies that they match.
 */

public class ContextHubInfoByIdTestExecutor extends ContextHubGeneralTestExecutor {

    public ContextHubInfoByIdTestExecutor(ContextHubManager manager, ContextHubInfo info,
            NanoAppBinary binary, ContextHubTestConstants.TestNames testName) {
        super(manager, info, new GeneralTestNanoApp(binary, testName));
    }

    @Override
    protected void handleMessageFromNanoApp(long nanoAppId,
            ContextHubTestConstants.MessageType type, byte[] data) {
        assertThat(type).isEqualTo(ContextHubTestConstants.MessageType.CONTINUE);

        int version =
                ChreTestUtil.getNanoAppVersion(getContextHubManager(), getContextHubInfo(),
                        nanoAppId);
        ByteBuffer buffer = ByteBuffer.allocate(4)
                .order(ByteOrder.LITTLE_ENDIAN)
                .putInt(version);

        sendMessageToNanoAppOrFail(nanoAppId, ContextHubTestConstants.MessageType.CONTINUE.asInt(),
                buffer.array());
    }
}
