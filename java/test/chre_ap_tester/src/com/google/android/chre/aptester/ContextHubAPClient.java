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

package com.google.android.chre.aptester;

import android.annotation.NonNull;
import android.hardware.location.NanoAppMessage;
import android.util.Log;

import java.util.concurrent.Executor;

/**
 * ContextHubClient: Represents the connection between the application and a specific nanoapp on AP.
 * Responsible for forwarding messages to the simulator via JNI and executing callbacks on the
 * specified Executor.
 *
 * Currently, we hold the assumption that the client has 1:1 mapping to nanoapps. Developer
 * should use ContextHubAPManager to create the client instead of creating directly.
 */
public final class ContextHubAPClient {

    private static final String TAG = "ContextHubAPClient";

    private final long mNanoAppId;
    private final Executor mExecutor;
    private final Callback mCallback;
    private final ContextHubAPManager mManager;

    /** Nanoapp event callback interface. */
    public interface Callback {
        /**
         * Called when a message is received from the simulated nanoapp.
         *
         * @param client The client that received the message.
         * @param message The message data.
         */
        void onMessageFromNanoApp(@NonNull ContextHubAPClient client, @NonNull byte[] message);
    }

    /**
     * Constructor.
     *
     * @param nanoAppId The ID of the target nanoapp.
     * @param executor The executor for invoking callbacks.
     * @param callback The message receiving callback.
     */
    ContextHubAPClient(long nanoAppId, @NonNull Executor executor, @NonNull Callback callback) {
        mNanoAppId = nanoAppId;
        mExecutor = executor;
        mCallback = callback;
        mManager = ContextHubAPManager.getInstance();
    }

    /**
     * Sends a message to the simulated nanoapp.
     *
     * @param message The data to send.
     * @return true if the message was successfully queued for the simulator.
     */
    public boolean sendMessageToNanoApp(@NonNull NanoAppMessage message) {
        Log.d(TAG, "Sending message to NanoApp ID: " + message);

        // Core: Send the message to the native simulator via JNI
        return Native.sendMessage(
                mNanoAppId,
                message.getMessageType(),
                message.getMessageBody(),
                message.getMessageBody().length);
    }

    /** Closes the connection to the nanoapp. */
    public void close() {
        Log.i(TAG, "Closing client for NanoApp ID: " + mNanoAppId);
        mManager.unregisterClient(mNanoAppId);
    }

    /** Retrieves the client callback interface. */
    @NonNull
    public Callback getCallback() {
        return mCallback;
    }

    /** Retrieves the executor for message handling. */
    public Executor getExecutor() {
        return mExecutor;
    }
}
