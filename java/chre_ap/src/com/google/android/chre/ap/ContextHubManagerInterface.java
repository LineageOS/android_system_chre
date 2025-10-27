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
import android.annotation.Nullable;
import android.content.Context;

import java.util.List;
import java.util.concurrent.Executor;

/** An interface that exposes the Context hubs on a device to applications. */
public interface ContextHubManagerInterface {

    /** Creates a client to communicate with a specific Context Hub. */
    @NonNull
    ContextHubClientInterface createClient(
            @Nullable Context context,
            @NonNull Executor executor,
            @NonNull ContextHubClientCallback callback);

    /** Creates a client with a callback that uses the main thread's Looper. */
    @NonNull
    ContextHubClientInterface createClient(@NonNull ContextHubClientCallback callback);

    /** Creates a client for a Context Hub with a specified callback and executor. */
    @NonNull
    ContextHubClientInterface createClient(
            @NonNull ContextHubClientCallback callback,
            @NonNull Executor executor);

    /** Queries for the list of preloaded nanoapp IDs on the system, for testing. */
    @NonNull
    long[] getPreloadedNanoAppIds();

    /** Requests a query for nanoapps loaded at the specified Context Hub. */
    @NonNull
    ContextHubTransaction<List<NanoAppState>> queryNanoApps();

    /** Unloads a nanoapp at the specified Context Hub. */
    @NonNull
    ContextHubTransaction<Void> unloadNanoApp(long nanoAppId);
}
