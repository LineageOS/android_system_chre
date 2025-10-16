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

import android.annotation.CallbackExecutor;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.TestApi;
import android.content.Context;
import android.hardware.contexthub.HubDiscoveryInfo;
import android.hardware.contexthub.HubEndpoint;
import android.hardware.contexthub.HubEndpointDiscoveryCallback;
import android.hardware.contexthub.HubEndpointInfo;
import android.hardware.location.ContextHubClientCallback;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubTransaction;
import android.hardware.location.HubInfo;
import android.hardware.location.NanoAppState;

import java.util.List;
import java.util.concurrent.Executor;

/** An interface that exposes the Context hubs on a device to applications. */
public interface ContextHubManagerInterface {

    /** Creates a client to communicate with a specific Context Hub. */
    @NonNull
    ContextHubClientInterface createClient(
            @Nullable Context context,
            @NonNull ContextHubInfo hubInfo,
            @NonNull @CallbackExecutor Executor executor,
            @NonNull ContextHubClientCallback callback);

    /** Creates a client with a callback that uses the main thread's Looper. */
    @NonNull
    ContextHubClientInterface createClient(
            @NonNull ContextHubInfo hubInfo, @NonNull ContextHubClientCallback callback);

    /** Creates a client for a Context Hub with a specified callback and executor. */
    @NonNull
    ContextHubClientInterface createClient(
            @NonNull ContextHubInfo hubInfo,
            @NonNull ContextHubClientCallback callback,
            @NonNull @CallbackExecutor Executor executor);

    /** Finds a list of endpoints that match a specific ID. */
    @NonNull
    List<HubDiscoveryInfo> findEndpoints(long endpointId);

    /** Finds a list of endpoints that provide a specific service. */
    @NonNull
    List<HubDiscoveryInfo> findEndpoints(@NonNull String serviceDescriptor);

    /** Returns the list of available Context Hubs. */
    @NonNull
    List<ContextHubInfo> getContextHubs();

    /** Returns the list of all available hubs, including Context Hubs and Vendor Hubs. */
    @NonNull
    List<HubInfo> getHubs();

    /** Queries for the list of preloaded nanoapp IDs on the system. */
    @TestApi
    @NonNull
    long[] getPreloadedNanoAppIds(@NonNull ContextHubInfo hubInfo);

    /** Opens a session from a registered endpoint to another endpoint. */
    void openSession(@NonNull HubEndpoint hubEndpoint, @NonNull HubEndpointInfo destination);

    /** Opens a session from a registered endpoint to another for a specific service. */
    void openSession(
            @NonNull HubEndpoint hubEndpoint,
            @NonNull HubEndpointInfo destination,
            @NonNull String serviceDescriptor);

    /** Requests a query for nanoapps loaded at the specified Context Hub. */
    @NonNull
    ContextHubTransaction<List<NanoAppState>> queryNanoApps(@NonNull ContextHubInfo hubInfo);

    /** Unloads a nanoapp at the specified Context Hub. */
    @NonNull
    ContextHubTransaction<Void> unloadNanoApp(@NonNull ContextHubInfo hubInfo, long nanoAppId);

    /** Registers an endpoint and its callback with the Context Hub Service. */
    void registerEndpoint(@NonNull HubEndpoint hubEndpoint);

    /** Registers a callback to be notified when a hub endpoint has started or stopped. */
    void registerEndpointDiscoveryCallback(
            @NonNull HubEndpointDiscoveryCallback callback, long endpointId);

    /**
     * Registers a callback with a specified executor to be notified of endpoint lifecycle events.
     */
    void registerEndpointDiscoveryCallback(
            @NonNull Executor executor,
            @NonNull HubEndpointDiscoveryCallback callback,
            long endpointId);

    /** Registers a callback for an endpoint identified by a service descriptor. */
    void registerEndpointDiscoveryCallback(
            @NonNull HubEndpointDiscoveryCallback callback, @NonNull String serviceDescriptor);

    /**
     * Registers a callback with a specified executor for an endpoint identified by a service
     * descriptor.
     */
    void registerEndpointDiscoveryCallback(
            @NonNull Executor executor,
            @NonNull HubEndpointDiscoveryCallback callback,
            @NonNull String serviceDescriptor);

    /** Unregisters an endpoint from the Context Hub Service. */
    void unregisterEndpoint(@NonNull HubEndpoint hubEndpoint);

    /** Unregisters a previously registered endpoint discovery callback. */
    void unregisterEndpointDiscoveryCallback(@NonNull HubEndpointDiscoveryCallback callback);
}
