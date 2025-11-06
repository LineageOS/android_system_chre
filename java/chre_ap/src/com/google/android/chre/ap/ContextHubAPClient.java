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

import android.annotation.IntRange;
import android.util.Log;

import java.util.concurrent.Executor;

/**
 * ContextHubClient: Represents the connection between the application and a specific nanoapp on AP.
 * Responsible for forwarding messages to the simulator via JNI and executing callbacks on the
 * specified Executor.
 *
 * <p>Currently, we hold the assumption that the client has 1:1 mapping to nanoapps. Developer
 * should use ContextHubAPManager to create the client instead of creating directly.
 */
public final class ContextHubAPClient implements ContextHubClientInterface {

    private static final String TAG = "ContextHubAPClient";

    private final Executor mExecutor;
    private final ContextHubClientCallback mCallback;
    private final ContextHubAPManager mManager;

    private Integer mId = null;

    /**
     * Constructor.
     *
     * @param id       The ID of the client.
     * @param executor The executor for invoking callbacks.
     * @param callback The message receiving callback.
     */
    ContextHubAPClient(
            Integer id,
            Executor executor,
            ContextHubClientCallback callback) {
        mId = id;
        mExecutor = executor;
        mCallback = callback;
        mManager = ContextHubAPManager.getInstance();
    }

    /**
     * Sends a message to the simulated nanoapp.
     *
     * @param message the message object to send
     * @return the result of sending the message defined as in ContextHubTransaction.Result
     */
    @Override
    public int sendMessageToNanoApp(NanoAppMessage message) {
        Log.d(TAG, "Sending message to NanoApp ID: " + message);

        // Core: Send the message to the native simulator via JNI
        boolean success =
                ContextHubAPNative.sendMessage(
                        message.getNanoAppId(),
                        message.getMessageType(),
                        message.getMessageBody(),
                        message.getMessageBody().length);
        return success
                ? ContextHubTransaction.RESULT_SUCCESS
                : ContextHubTransaction.RESULT_FAILED_UNKNOWN;
    }

    @Override
    public ContextHubTransaction<Void> sendReliableMessageToNanoApp(
            NanoAppMessage message) {
        var res = sendMessageToNanoApp(message);
        ContextHubTransaction<Void> transaction =
                new ContextHubTransaction<>(ContextHubTransaction.TYPE_RELIABLE_MESSAGE);

        var result = sendMessageToNanoApp(message);
        transaction.setResponse(new ContextHubTransaction.Response<Void>(result, null));

        return transaction;
    }

    @Override
    @IntRange(from = 0, to = 65535)
    public int getId() {
        if (mId == null) {
            throw new IllegalStateException("ID was not set");
        }
        return (0x0000FFFF & mId);
    }

    /** Closes the connection to the nanoapp. */
    @Override
    public void close() {
        Log.i(TAG, "Closing client");
        mManager.unregisterClient(this);
    }

    /** Retrieves the client callback interface. */
    public ContextHubClientCallback getCallback() {
        return mCallback;
    }

    /** Retrieves the executor for message handling. */
    public Executor getExecutor() {
        return mExecutor;
    }
}
