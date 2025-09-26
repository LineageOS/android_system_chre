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

import android.annotation.CallbackExecutor;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.hardware.contexthub.HubDiscoveryInfo;
import android.hardware.contexthub.HubEndpoint;
import android.hardware.contexthub.HubEndpointDiscoveryCallback;
import android.hardware.contexthub.HubEndpointInfo;
import android.hardware.location.ContextHubClientCallback;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubTransaction;
import android.hardware.location.HubInfo;
import android.hardware.location.NanoAppMessage;
import android.hardware.location.NanoAppState;
import android.os.Handler;
import android.os.HandlerExecutor;
import android.os.Looper;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * ContextHubAPManager: The managing API for simulated Context Hub functionality. This class aims to
 * provide an interface to the CHRE AP environment for devices that lack hardware CHRE support. Its
 * implementation runs entirely within the client application process and communicates with the
 * native environment via JNI. It follows the Singleton pattern but is a local manager within the
 * app process, not a system service.
 */
public final class ContextHubAPManager implements ContextHubManagerInterface {

    private static final String TAG = "ContextHubAPManager";

    // Used to ensure callbacks are executed on the main thread
    private final Handler mMainHandler;

    private static volatile ContextHubAPManager sInstance;

    private final AtomicInteger mClientIdCounter = new AtomicInteger(0);

    // One nano app should have only one client created.
    private final ConcurrentHashMap<Integer, ContextHubAPClient> mClientMap =
            new ConcurrentHashMap<Integer, ContextHubAPClient>();

    private ContextHubAPManager() {
        // Init the CHRE AP environment
        int initRes = Native.init();

        if (initRes != 0) {
            Log.e(TAG, "Failed to initialize native CHRE AP environment: " + initRes);
            throw new RuntimeException("CHRE AP environment initialization failed.");
        }

        mMainHandler = new Handler(Looper.getMainLooper());
        Log.i(TAG, "ContextHubAPManager initialized successfully.");
    }

    /**
     * Retrieves the singleton instance of ContextHubAPManager.
     *
     * @return The instance of ContextHubAPManager.
     */
    @NonNull
    public static ContextHubAPManager getInstance() {
        if (sInstance == null) {
            synchronized (ContextHubAPManager.class) {
                if (sInstance == null) {
                    sInstance = new ContextHubAPManager();
                }
            }
        }
        return sInstance;
    }

    /**
     * Creates and registers a client to communicate with a simulated nanoapp.
     *
     * @param context The context of caller.
     * @param executor The executor used to invoke callbacks (typically the main thread executor).
     * @param callback The callback to receive messages and events from the nanoapp.
     * @return The ContextHubClient instance.
     */
    @NonNull
    @Override
    public ContextHubClientInterface createClient(
            @Nullable Context context,
            @NonNull ContextHubInfo hubInfo,
            @NonNull @CallbackExecutor Executor executor,
            @NonNull ContextHubClientCallback callback) {
        Objects.requireNonNull(callback, "Callback cannot be null");
        Objects.requireNonNull(hubInfo, "ContextHubInfo cannot be null");
        Objects.requireNonNull(executor, "Executor cannot be null");
        var clientId = mClientIdCounter.incrementAndGet();
        var client = new ContextHubAPClient(clientId, executor, callback);
        mClientMap.put(clientId, client);
        return client;
    }

    @NonNull
    @Override
    public ContextHubClientInterface createClient(
            @NonNull ContextHubInfo hubInfo,
            @NonNull ContextHubClientCallback callback,
            @NonNull @CallbackExecutor Executor executor) {
        return createClient(null /* context */, hubInfo, executor, callback);
    }

    @NonNull
    @Override
    public ContextHubClientInterface createClient(
            @NonNull ContextHubInfo hubInfo, @NonNull ContextHubClientCallback callback) {
        return createClient(
                null /* context */, hubInfo, new HandlerExecutor(mMainHandler), callback);
    }

    /**
     * Unregister a client from manager.
     *
     * @param client The client to unregister.
     */
    public void unregisterClient(ContextHubAPClient client) {
        mClientMap.remove(client.getId(), client);
    }

    /**
     * Loads a nanoapp from a file into the simulated CHRE environment.
     *
     * @param filename The path to the nanoapp shared object (.so) file.
     * @return {@code true} if the nanoapp was loaded successfully, {@code false} otherwise.
     */
    public boolean loadNanoApp(String filename) {
        boolean success = Native.loadNanoAppFromFile(filename);
        if (!success) {
            Log.d(TAG, "Load nano app failed for " + filename);
        }
        return success;
    }

    @NonNull
    @Override
    public ContextHubTransaction<Void> unloadNanoApp(
            @NonNull ContextHubInfo hubInfo, long nanoAppInstanceId) {
        boolean unloadRes = Native.unloadNanoApp(nanoAppInstanceId);
        if (!unloadRes) {
            Log.d(TAG, "Unload nano app failed for instance id: " + nanoAppInstanceId);
        }
        ContextHubTransaction<Void> transaction =
                new ContextHubTransaction<>(ContextHubTransaction.TYPE_UNLOAD_NANOAPP);
        transaction.setResponse(
                new ContextHubTransaction.Response<>(
                        unloadRes
                                ? ContextHubTransaction.RESULT_SUCCESS
                                : ContextHubTransaction.RESULT_FAILED_UNKNOWN,
                        null));
        return transaction;
    }

    @NonNull
    @Override
    public ContextHubTransaction<List<NanoAppState>> queryNanoApps(
            @NonNull ContextHubInfo hubInfo) {
        // Not implemented for AP environment
        ContextHubTransaction<List<NanoAppState>> transaction =
                new ContextHubTransaction<>(ContextHubTransaction.TYPE_QUERY_NANOAPPS);
        transaction.setResponse(
                new ContextHubTransaction.Response<>(
                        ContextHubTransaction.RESULT_SUCCESS, new ArrayList<NanoAppState>()));
        return transaction;
    }

    /**
     * JNI callback method called by the C/C++ simulator This is invoked when a nanoapp sends a
     * message and is usually executed on a JNI thread.
     *
     * @param nanoAppId The ID of the nanoapp that sent the message.
     * @param messageData The message data sent by the nanoapp.
     */
    public void onMessageFromNanoApp(long nanoAppId, int messageType, byte[] messageBody) {
        var message =
                NanoAppMessage.createMessageFromNanoApp(
                        nanoAppId, messageType, messageBody, false, false, 0);
        Log.d(TAG, "Message received for NanoApp ID: " + nanoAppId);
        mMainHandler.post(
                () -> {
                    for (ContextHubAPClient client : mClientMap.values()) {
                        Executor exec = client.getExecutor();
                        if (exec != null) {
                            exec.execute(
                                    new Runnable() {
                                        @Override
                                        public void run() {
                                            client.getCallback()
                                                    .onMessageFromNanoApp(null, message);
                                        }
                                    });
                        } else {
                            client.getCallback().onMessageFromNanoApp(null, message);
                        }
                    }
                });
    }

    @NonNull
    @Override
    public List<HubDiscoveryInfo> findEndpoints(long endpointId) {
        // Not implemented for AP environment
        return new ArrayList<>();
    }

    @NonNull
    @Override
    public List<HubDiscoveryInfo> findEndpoints(@NonNull String serviceDescriptor) {
        // Not implemented for AP environment
        return new ArrayList<>();
    }

    @NonNull
    @Override
    public List<ContextHubInfo> getContextHubs() {
        // Not implemented for AP environment
        return new ArrayList<>();
    }

    @NonNull
    @Override
    public List<HubInfo> getHubs() {
        // Not implemented for AP environment
        return new ArrayList<>();
    }

    @NonNull
    @Override
    public long[] getPreloadedNanoAppIds(@NonNull ContextHubInfo hubInfo) {
        // Not implemented for AP environment
        return new long[0];
    }

    @Override
    public void openSession(
            @NonNull HubEndpoint hubEndpoint, @NonNull HubEndpointInfo destination) {
        // Not implemented for AP environment
    }

    @Override
    public void openSession(
            @NonNull HubEndpoint hubEndpoint,
            @NonNull HubEndpointInfo destination,
            @NonNull String serviceDescriptor) {
        // Not implemented for AP environment
    }

    @Override
    public void registerEndpoint(@NonNull HubEndpoint hubEndpoint) {
        // Not implemented for AP environment
    }

    @Override
    public void registerEndpointDiscoveryCallback(
            @NonNull HubEndpointDiscoveryCallback callback, long endpointId) {
        // Not implemented for AP environment
    }

    @Override
    public void registerEndpointDiscoveryCallback(
            @NonNull Executor executor,
            @NonNull HubEndpointDiscoveryCallback callback,
            long endpointId) {
        // Not implemented for AP environment

    }

    /** Registers a callback for an endpoint identified by a service descriptor. */
    @Override
    public void registerEndpointDiscoveryCallback(
            @NonNull HubEndpointDiscoveryCallback callback, @NonNull String serviceDescriptor) {
        // Not implemented for AP environment

    }

    @Override
    public void registerEndpointDiscoveryCallback(
            @NonNull Executor executor,
            @NonNull HubEndpointDiscoveryCallback callback,
            @NonNull String serviceDescriptor) {
        // Not implemented for AP environment
    }

    @Override
    public void unregisterEndpoint(@NonNull HubEndpoint hubEndpoint) {
        // Not implemented for AP environment
    }

    @Override
    public void unregisterEndpointDiscoveryCallback(
            @NonNull HubEndpointDiscoveryCallback callback) {
        // Not implemented for AP environment
    }
}
