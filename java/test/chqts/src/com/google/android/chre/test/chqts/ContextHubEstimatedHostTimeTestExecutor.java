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
import static com.google.common.truth.Truth.assertWithMessage;

import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.NanoAppBinary;
import android.os.SystemClock;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Verify estimated host time from nanoapp is relatively accurate.
 *
 * <p> The test is initiated from the nanoapp side which sends a message to the host. Starting from
 * there host sends an empty message to the nanoapp and wait for the response.
 * Once the response is received the time that CHRE received the message is calculated as
 * message_received_time = message_sent_time + RTT/2. Then message_received_time is used to
 * calculate a delta with the chreGetEstimatedHostTime(). This process is repeated to get a min
 * value.
 */
public class ContextHubEstimatedHostTimeTestExecutor extends ContextHubGeneralTestExecutor {
    private static final int MAX_ALLOWED_NIN_DELTA_NS = 10_000_000;     // 10 ms.
    private static final int NUM_OF_SAMPLES = 5;
    private long mMsgSendTimestampNs = 0;
    private static final String TAG = "EstimatedHostTimeTest";
    private final List<Long> mDeltas;
    private final long mNanoappId;

    public ContextHubEstimatedHostTimeTestExecutor(ContextHubManager manager, ContextHubInfo info,
            NanoAppBinary binary) {
        super(manager, info, new GeneralTestNanoApp(binary,
                ContextHubTestConstants.TestNames.ESTIMATED_HOST_TIME));
        mNanoappId = binary.getNanoAppId();
        mDeltas = new ArrayList<>();
    }

    @Override
    protected void handleMessageFromNanoApp(long nanoAppId,
            ContextHubTestConstants.MessageType type, byte[] data) {
        assertThat(type).isEqualTo(ContextHubTestConstants.MessageType.CONTINUE);

        if (mMsgSendTimestampNs != 0) {
            assertThat(data.length).isGreaterThan(0);
            long currentTimestampNs = SystemClock.elapsedRealtimeNanos();
            long chreTimestampNs = ByteBuffer.wrap(data)
                    .order(ByteOrder.LITTLE_ENDIAN)
                    .getLong();

            long middleTimestamp = (currentTimestampNs - mMsgSendTimestampNs) / 2
                    + mMsgSendTimestampNs;
            mDeltas.add(Math.abs(chreTimestampNs - middleTimestamp));

            if (mDeltas.size() == NUM_OF_SAMPLES) {
                Log.d(TAG, "Deltas (ns): " + mDeltas);
                assertWithMessage(String.format(
                        "The min delta between estimated host time and host real"
                                + " time is larger than %d ms. abs of deltas: %s",
                        MAX_ALLOWED_NIN_DELTA_NS, mDeltas)).that(Collections.min(mDeltas)).isAtMost(
                        MAX_ALLOWED_NIN_DELTA_NS);
                pass();
            }
        }

        if (mDeltas.size() < NUM_OF_SAMPLES) {
            mMsgSendTimestampNs = SystemClock.elapsedRealtimeNanos();
            sendMessageToNanoAppOrFail(mNanoappId,
                    ContextHubTestConstants.MessageType.CONTINUE.asInt(), /* data= */
                    new byte[0]);
        }
    }
}
