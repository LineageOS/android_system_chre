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

package com.google.android.chre.ap;

import android.annotation.Nullable;

import java.util.Arrays;
import java.util.Objects;

/**
 * A class describing messages send to or from nanoapps through the Context Hub Service.
 */
public final class NanoAppMessage {
    private static final int DEBUG_LOG_NUM_BYTES = 16;
    private long mNanoAppId;
    private int mMessageType;
    private byte[] mMessageBody;
    private boolean mIsBroadcasted;

    private NanoAppMessage(
            long nanoAppId,
            int messageType,
            byte[] messageBody,
            boolean broadcasted) {
        mNanoAppId = nanoAppId;
        mMessageType = messageType;
        mMessageBody = messageBody;
        mIsBroadcasted = broadcasted;
    }

    /**
     * Creates a NanoAppMessage object to send to a nanoapp.
     *
     * <p>This factory method can be used to generate a NanoAppMessage object to be used in the
     * ContextHubAPClient.sendMessageToNanoApp API.
     *
     * @param targetNanoAppId the ID of the nanoapp to send the message to
     * @param messageType     the nanoapp-dependent message type the value CHRE_MESSAGE_TYPE_RPC
     *                        (0x7FFFFFF5) is reserved by the framework for RPC messages
     * @param messageBody     the byte array message contents
     * @return the NanoAppMessage object
     */
    public static NanoAppMessage createMessageToNanoApp(
            long targetNanoAppId, int messageType, byte[] messageBody) {
        return new NanoAppMessage(
                targetNanoAppId,
                messageType,
                messageBody,
                false /* broadcasted */);
    }

    /**
     * Creates a NanoAppMessage object sent from a nanoapp.
     *
     * <p>This factory method is intended only to be used by the ContextHub AP when delivering
     * messages from a nanoapp to clients.
     *
     * @param sourceNanoAppId the ID of the nanoapp that the message was sent from
     * @param messageType     the nanoapp-dependent message type
     * @param messageBody     the byte array message contents
     * @param broadcasted     {@code true} if the message was broadcasted, {@code false} otherwise
     * @return the NanoAppMessage object
     */
    public static NanoAppMessage createMessageFromNanoApp(
            long sourceNanoAppId, int messageType, byte[] messageBody, boolean broadcasted) {
        return new NanoAppMessage(
                sourceNanoAppId,
                messageType,
                messageBody,
                broadcasted);
    }

    /**
     * @return the ID of the source or destination nanoapp
     */
    public long getNanoAppId() {
        return mNanoAppId;
    }

    /**
     * @return the type of the message that is nanoapp-dependent
     */
    public int getMessageType() {
        return mMessageType;
    }

    /**
     * @return the byte array contents of the message
     */
    public byte[] getMessageBody() {
        return mMessageBody;
    }

    /**
     * @return {@code true} if the message is broadcasted, {@code false} otherwise
     */
    public boolean isBroadcastMessage() {
        return mIsBroadcasted;
    }

    @Override
    public String toString() {
        int length = mMessageBody.length;

        StringBuilder out = new StringBuilder();
        out.append("NanoAppMessage[type = ");
        out.append(mMessageType);
        out.append(", length = ");
        out.append(mMessageBody.length);
        out.append(" bytes, ");
        out.append(mIsBroadcasted ? "broadcast" : "unicast");
        out.append(", nanoapp = 0x");
        out.append(Long.toHexString(mNanoAppId));
        out.append("](");

        if (length > 0) {
            out.append("data = 0x");
        }

        for (int i = 0; i < Math.min(length, DEBUG_LOG_NUM_BYTES); i++) {
            out.append(String.format("%02x", mMessageBody[i]));

            if ((i + 1) % 4 == 0) {
                out.append(" ");
            }
        }

        if (length > DEBUG_LOG_NUM_BYTES) {
            out.append("...");
        }
        out.append(")");

        return out.toString();
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (object == this) {
            return true;
        }

        boolean isEqual = false;
        if (object instanceof NanoAppMessage) {
            NanoAppMessage other = (NanoAppMessage) object;
            isEqual =
                    (other.getNanoAppId() == mNanoAppId)
                            && (other.getMessageType() == mMessageType)
                            && (other.isBroadcastMessage() == mIsBroadcasted)
                            && Arrays.equals(other.getMessageBody(), mMessageBody);
        }

        return isEqual;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                mNanoAppId,
                mMessageType,
                mIsBroadcasted,
                Arrays.hashCode(mMessageBody));
    }
}
