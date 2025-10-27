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

import android.annotation.NonNull;

import java.io.Closeable;

/**
 * An interface for a client of the Context Hub Service.
 *
 * <p>Clients can send messages to nanoapps at a Context Hub through an object implementing this
 * interface. The APIs supported are thread-safe and can be used without external synchronization.
 */
public interface ContextHubClientInterface extends Closeable {

    /**
     * Returns the system-wide unique identifier for this ContextHubClient.
     *
     * <p>This value can be used as an identifier for the messaging channel between a
     * ContextHubClient and the Context Hub. This may be used as a routing mechanism between various
     * ContextHubClient objects within an application.
     *
     * <p>The value returned by this method will remain the same if it is associated with the same
     * client reference at the ContextHubService (for instance, the ID of a PendingIntent
     * ContextHubClient will remain the same even if the local object has been regenerated with the
     * equivalent PendingIntent). If the ContextHubClient is newly generated (e.g. any regeneration
     * of a callback client, or generation of a non-equal PendingIntent client), the ID will not be
     * the same.
     *
     * @return The ID of this ContextHubClient, in the range [0, 65535].
     */
    int getId();

    /**
     * Closes the connection for this client.
     *
     * <p>When this function is invoked, the messaging associated with this client is invalidated.
     * All future messages targeted for this client are dropped at the service, and the
     * ContextHubAPClient is unregistered from the AP service.
     */
    void close();

    /**
     * Sends a message to a nanoapp through the ContextHub AP.
     *
     * <p>This function returns RESULT_SUCCESS if the message has reached the HAL, but does not
     * guarantee delivery of the message to the target nanoapp.
     *
     * <p>Before sending the first message to your nanoapp, it's recommended that the following
     * operations should be performed: 1) Invoke {@link
     * ContextHubAPManager#queryNanoApps()} to verify the nanoapp is present. 2)
     * Validate that you have the permissions to communicate with the nanoapp by looking at {@link
     * NanoAppState#getNanoAppPermissions}. 3) If you don't have permissions, send an idempotent
     * message to the nanoapp ensuring any work your app previously may have asked it to do is
     * stopped. This is useful if your app restarts due to permission changes and no longer has the
     * permissions when it is started again. 4) If you have valid permissions, send a message to
     * your nanoapp to resubscribe so that it's aware you have restarted or so you can initially
     * subscribe if this is the first time you have sent it a message.
     *
     * @param message the message object to send
     * @return the result of sending the message defined as in ContextHubTransaction.Result
     * @throws NullPointerException if NanoAppMessage is null
     * @throws SecurityException    if this client doesn't have permissions to send a message to the
     *                              nanoapp.
     * @see NanoAppMessage
     * @see ContextHubTransaction.Result
     */
    @ContextHubTransaction.Result
    int sendMessageToNanoApp(@NonNull NanoAppMessage message);

    /**
     * Sends a reliable message to a nanoapp.
     *
     * <p>The transaction succeeds after we received an ACK from CHRE without error. In all other
     * cases the transaction will fail.
     *
     * @param message The message to send.
     * @return A {@link ContextHubTransaction} that can be used to track the reliable message.
     */
    @NonNull
    ContextHubTransaction<Void> sendReliableMessageToNanoApp(@NonNull NanoAppMessage message);
}
