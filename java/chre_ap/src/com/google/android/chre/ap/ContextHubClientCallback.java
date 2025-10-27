/*
 * Copyright 2025 The Android Open Source Project
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

import java.util.concurrent.Executor;

/**
 * A class for {@link ContextHubClientInterface} to receive messages and life-cycle events from
 * nanoapps in the Context Hub at which the client is attached to.
 *
 * <p>This callback is registered through the {@link
 * ContextHubAPManager#createClient(ContextHubClientCallback, Executor) creation} of {@link
 * ContextHubAPClient}. Callbacks are invoked in the following ways: 1) Messages from nanoapps
 * delivered through onMessageFromNanoApp may either be broadcasted or targeted to a specific
 * client. 2) Nanoapp or Context Hub events (the remaining callbacks) are broadcasted to all
 * clients, and the client can choose to ignore the event by filtering through the parameters.
 *
 */
public class ContextHubClientCallback {
    /**
     * Callback invoked when receiving a message from a nanoapp.
     *
     * <p>The message contents of this callback may either be broadcasted or targeted to the client
     * receiving the invocation.
     *
     * @param client  the client that is associated with this callback
     * @param message the message sent by the nanoapp
     */
    public void onMessageFromNanoApp(ContextHubClientInterface client, NanoAppMessage message) {
    }

    /**
     * Callback invoked when the attached Context Hub has reset.
     *
     * @param client the client that is associated with this callback
     */
    public void onHubReset(ContextHubClientInterface client) {
    }

    /**
     * Callback invoked when a nanoapp aborts at the attached Context Hub.
     *
     * @param client    the client that is associated with this callback
     * @param nanoAppId the ID of the nanoapp that had aborted
     * @param abortCode the reason for nanoapp's abort, specific to each nanoapp
     */
    public void onNanoAppAborted(ContextHubClientInterface client, long nanoAppId, int abortCode) {
    }

    /**
     * Callback invoked when a nanoapp is dynamically loaded.
     *
     * <p>NOTE: This callback is <b>not</b> invoked for a nanoapp that is loaded internally by CHRE
     * (e.g. nanoapps that are preloaded by the system). To check the availability of these
     * nanoapps,
     * use the {@link ContextHubAPManager#queryNanoApps()} API.
     *
     * @param client    the client that is associated with this callback
     * @param nanoAppId the ID of the nanoapp that had been loaded
     */
    public void onNanoAppLoaded(ContextHubClientInterface client, long nanoAppId) {
    }

    /**
     * Callback invoked when a nanoapp is dynamically unloaded from the attached Context Hub through
     * the {@link ContextHubAPManager}.
     *
     * @param client    the client that is associated with this callback
     * @param nanoAppId the ID of the nanoapp that had been unloaded
     */
    public void onNanoAppUnloaded(ContextHubClientInterface client, long nanoAppId) {
    }

    /**
     * Callback invoked when a nanoapp is dynamically enabled at the attached Context Hub through
     * the {@link ContextHubAPManager}.
     *
     * @param client    the client that is associated with this callback
     * @param nanoAppId the ID of the nanoapp that had been enabled
     */
    public void onNanoAppEnabled(ContextHubClientInterface client, long nanoAppId) {
    }

    /**
     * Callback invoked when a nanoapp is dynamically disabled at the attached Context Hub through
     * the {@link ContextHubAPManager}.
     *
     * @param client    the client that is associated with this callback
     * @param nanoAppId the ID of the nanoapp that had been disabled
     */
    public void onNanoAppDisabled(ContextHubClientInterface client, long nanoAppId) {
    }
}
