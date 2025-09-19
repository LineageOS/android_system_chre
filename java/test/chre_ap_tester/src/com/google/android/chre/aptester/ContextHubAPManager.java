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
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;

/**
 * ContextHubAPManager: The managing API for simulated Context Hub functionality. This class aims to
 * provide an interface to the CHRE AP environment for devices that lack hardware CHRE support. Its
 * implementation runs entirely within the client application process and communicates with the
 * native environment via JNI. It follows the Singleton pattern but is a local manager within the
 * app process, not a system service.
 */
public final class ContextHubAPManager {

    private static final String TAG = "ContextHubAPManager";

    // Used to ensure callbacks are executed on the main thread
    private final Handler mMainHandler;

    private static volatile ContextHubAPManager sInstance;

    // One nano app should have only one client created.
    private final ConcurrentHashMap<Long, ContextHubAPClient> mClientMap =
            new ConcurrentHashMap<Long, ContextHubAPClient>();

    private ContextHubAPManager() {
        // Init the CHRE AP environment
        int initRes = Native.init();

        if (initRes != 0) {
            Log.e(TAG, "Failed to initialize native CHRE AP environment: " + initRes);
            throw new RuntimeException("CHRE AP simulator initialization failed.");
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
     * @param nanoAppId The ID of the target nanoapp.
     * @param executor The executor used to invoke callbacks (typically the main thread executor).
     * @param callback The callback to receive messages and events from the nanoapp.
     * @return The ContextHubClient instance.
     */
    public synchronized ContextHubAPClient createClient(
            long nanoAppId,
            @NonNull Executor executor,
            @NonNull ContextHubAPClient.Callback callback) {
        ContextHubAPClient client = mClientMap.get(nanoAppId);
        if (client != null) {
            Log.d(TAG, "Client already registered for NanoApp ID: " + nanoAppId);
            return null;
        }
        int loadRes = Native.loadNanoApp(nanoAppId);
        if (loadRes != 0) {
            Log.d(TAG, "Load nano app failed: " + loadRes);
            return null;
        }

        client = new ContextHubAPClient(nanoAppId, executor, callback);
        mClientMap.put(nanoAppId, client);
        Log.d(TAG, "Client registered for NanoApp ID: " + nanoAppId);
        return client;
    }

    /**
     * Unregister a client from manager.
     *
     * @param nanoAppId
     */
    public void unregisterClient(long nanoAppId) {
        mClientMap.remove(nanoAppId);
        int unloadRes = Native.unloadNanoApp(nanoAppId);
        if (unloadRes != 0) {
            Log.d(TAG, "Unload nano app failed: " + unloadRes);
        }
    }

    /**
     * JNI callback method called by the C/C++ simulator This is invoked when a nanoapp sends a
     * message and is usually executed on a JNI thread.
     *
     * @param nanoAppId
     * @param messageData
     */
    public void onMessageFromNanoApp(long nanoAppId, byte[] messageData) {
        mMainHandler.post(
                () -> {
                    ContextHubAPClient client = mClientMap.get(nanoAppId);
                    if (client == null) {
                        Log.d(TAG, "No client for nanoapp id: " + nanoAppId);
                        return;
                    }
                    Log.d(TAG, "Message received for NanoApp ID: " + nanoAppId);
                    Executor exec = client.getExecutor();
                    if (exec != null) {
                        exec.execute(
                                new Runnable() {
                                    @Override
                                    public void run() {
                                        client.getCallback()
                                                .onMessageFromNanoApp(client, messageData);
                                    }
                                });
                    } else {
                        client.getCallback().onMessageFromNanoApp(client, messageData);
                    }
                });
    }
}
