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
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

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
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());

    private static volatile ContextHubAPManager sInstance;

    private final AtomicInteger mClientIdCounter = new AtomicInteger(0);

    private Thread mEventLoopThread = null;
    private boolean mEventLoopRunning = false;
    private ThreadFactory mThreadFactory = new DefaultThreadFactory();

    // One nano app should have only one client created.
    private final ConcurrentHashMap<Integer, ContextHubAPClient> mClientMap =
            new ConcurrentHashMap<Integer, ContextHubAPClient>();

    private ContextHubAPManager() {
    }

    /**
     * Retrieves the singleton instance of ContextHubAPManager.
     *
     * @return The instance of ContextHubAPManager.
     */
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
     * Initializes the CHRE AP environment. Repeated calls will be no-op.
     *
     * @throws RuntimeException if the initialization fails.
     */
    public void init(Context appContext, @Nullable WakeLockBridge.LockFactory lockFactory) {
        if (isInitialized()) {
            return;
        }
        // Init the CHRE AP environment
        ContextHubAPNative.setContext(appContext);
        int initRes = ContextHubAPNative.init();
        if (initRes != 0) {
            Log.e(TAG, "Failed to initialize native CHRE AP environment: " + initRes);
            throw new RuntimeException("CHRE AP environment initialization failed.");
        }
        ContextHubAPNative.nativeRegister(this);
        AlarmManagerBridge.initialize(appContext);
        WakeLockBridge.setLockFactory(Objects.requireNonNullElseGet(lockFactory,
                () -> new WakeLockBridge.DefaultLockFactory(appContext)));
        Log.i(TAG, "ContextHubAPManager initialized successfully.");
    }

    public boolean isInitialized() {
        return ContextHubAPNative.isInitialized();
    }

    /**
     * ThreadFactory to use for creating event loop.
     */
    public interface ThreadFactory {
        /**
         * Creates a new thread.
         * @param runnable
         * @return Created thread with the runnable.
         */
        Thread newThread(Runnable runnable);
    }

    /**
     * Default implementation of ThreadFactory.
     */
    public static class DefaultThreadFactory implements ThreadFactory {
        @Override
        public Thread newThread(Runnable runnable) {
            return new Thread(runnable);
        }
    }

    /**
     * Sets the thread factory used to create event loop thread. Must be set before event loop is
     * created.
     * @param factory The thread factory to use.
     */
    public void setThreadFactory(ThreadFactory factory) {
        if (mEventLoopRunning) {
            Log.w(TAG, "Cannot set ThreadFactory while event loop is running.");
            return;
        }
        mThreadFactory = Objects.requireNonNull(factory, "ThreadFactory cannot be null");
    }

    /**
     * Specify the running mode for CHRE event loop.
     */
    public enum EventLoopMode {
        // A new thread will be created in native library.
        NATIVE,
        // The event loop will be run on a newly created thread, owned and managed by the manager.
        OWNED,
        // The event loop will run on the calling thread. Caller is also expected to call
        // setEventLoopThread to make sure the thread is joined upon destroy.
        PROVIDED,
    }

    /**
     * Runs the event loop of the CHRE AP environment. Repeated calls will be no-op.
     *
     * @param mode The running mode for the event loop.
     */
    public void runEventLoop(EventLoopMode mode) {
        if (mEventLoopRunning) {
            Log.e(TAG, "EventLoop is already running, nothing to do.");
            return;
        }
        mEventLoopRunning = true;
        switch (mode) {
            case NATIVE:
                ContextHubAPNative.runEventLoop(true /*useNativeThread*/);
                break;
            case PROVIDED:
                ContextHubAPNative.runEventLoop(false /*useNativeThread*/);
                break;
            case OWNED:
                mEventLoopThread = mThreadFactory.newThread(() -> {
                    ContextHubAPNative.runEventLoop(false /*useNativeThread*/);
                });
                mEventLoopThread.start();
                break;
            default:
                Log.e(TAG, "Unknown event loop mode");
        }
    }

    /** Destroys the CHRE AP environment. Repeated calls will be no-op. */
    public void destroy() {
        if (!isInitialized()) {
            return;
        }
        ContextHubAPNative.stopEventLoop();
        if (mEventLoopThread != null) {
            mEventLoopThread.interrupt();
            try {
                mEventLoopThread.join();
            } catch (InterruptedException e) {
                Log.e(TAG, "Failed to join event loop thread: " + e);
            }
            mEventLoopThread = null;
        }
        mEventLoopRunning = false;
        ContextHubAPNative.destroy();

        mClientMap.clear();
    }

    /**
     * If set, indicate the event loop is run on this thread. ContextHubAPManager will join the
     * thread up on destroy.
     */
    public void setEventLoopThread(@Nullable Thread eventLoopThread) {
        mEventLoopThread = eventLoopThread;
    }

    /**
     * Creates and registers a client to communicate with a simulated nanoapp.
     *
     * @param context  The context of caller.
     * @param executor The executor used to invoke callbacks (typically the main thread executor).
     * @param callback The callback to receive messages and events from the nanoapp.
     * @return The ContextHubClient instance.
     */
    @Override
    public ContextHubClientInterface createClient(
            @Nullable Context context,
            Executor executor,
            ContextHubClientCallback callback) {
        if (!isInitialized()) {
            throw new IllegalStateException("CHRE AP environment not initialized.");
        }
        Objects.requireNonNull(callback, "Callback cannot be null");
        Objects.requireNonNull(executor, "Executor cannot be null");
        var clientId = mClientIdCounter.incrementAndGet();
        var client = new ContextHubAPClient(clientId, executor, callback);
        mClientMap.put(clientId, client);
        return client;
    }

    @Override
    public ContextHubClientInterface createClient(
            ContextHubClientCallback callback, Executor executor) {
        return createClient(null /* context */, executor, callback);
    }

    @Override
    public ContextHubClientInterface createClient(ContextHubClientCallback callback) {
        return createClient(null /* context */, new HandlerExecutor(mMainHandler), callback);
    }

    /**
     * Unregister a client from manager. Does nothing if the client is already unregistered.
     *
     * @param client The client to unregister.
     */
    public void unregisterClient(ContextHubClientInterface client) {
        mClientMap.remove(client.getId());
    }

    /**
     * Loads a nanoapp from a file into the simulated CHRE environment. Loading a file multiple
     * times will result in duplicated nanoapp instances, which may not be desired.
     *
     * @param filename The path to the nanoapp shared object (.so) file.
     * @return {@code true} if the nanoapp was loaded successfully, {@code false} otherwise.
     */
    public boolean loadNanoApp(String filename) {
        if (!isInitialized()) {
            throw new IllegalStateException("CHRE AP environment not initialized.");
        }
        boolean success = ContextHubAPNative.loadNanoAppFromFile(filename);
        if (!success) {
            Log.d(TAG, "Load nano app failed for " + filename);
        }
        return success;
    }

    /**
     * Unload a nanoapp from CHRE AP environment.
     *
     * @param nanoAppInstanceId The ID of the nanoapp to unload.
     * @return A ContextHubTransaction containing the result of the unload operation.
     */
    @Override
    public ContextHubTransaction<Void> unloadNanoApp(long nanoAppInstanceId) {
        if (!isInitialized()) {
            throw new IllegalStateException("CHRE AP environment not initialized.");
        }
        boolean unloadRes = ContextHubAPNative.unloadNanoApp(nanoAppInstanceId);
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

    /**
     * Queries the current running nanoapps.
     *
     * @return A ContextHubTransaction containing the list of nanoapps.
     */
    @Override
    public ContextHubTransaction<List<NanoAppState>> queryNanoApps() {
        if (!isInitialized()) {
            throw new IllegalStateException("CHRE AP environment not initialized.");
        }
        NanoAppState[] nanoAppInfos = ContextHubAPNative.listNanoapps();
        ContextHubTransaction<List<NanoAppState>> transaction =
                new ContextHubTransaction<>(ContextHubTransaction.TYPE_QUERY_NANOAPPS);
        transaction.setResponse(
                new ContextHubTransaction.Response<>(
                        ContextHubTransaction.RESULT_SUCCESS, List.of(nanoAppInfos)));
        return transaction;
    }

    /**
     * JNI callback method called by the C/C++ simulator This is invoked when a nanoapp sends a
     * message and is usually executed on a JNI thread.
     *
     * @param nanoAppId   The ID of the nanoapp that sent the message.
     * @param messageType The type of the message.
     * @param messageBody The message data sent by the nanoapp.
     */
    public void onMessageFromNanoApp(long nanoAppId, int messageType, byte[] messageBody) {
        var message =
                NanoAppMessage.createMessageFromNanoApp(nanoAppId, messageType, messageBody, false);
        Log.d(TAG, "Message received for NanoApp ID: " + nanoAppId);
        for (ContextHubAPClient client : mClientMap.values()) {
            client.getExecutor()
                    .execute(
                            new Runnable() {
                                @Override
                                public void run() {
                                    client.getCallback()
                                            .onMessageFromNanoApp(null, message);
                                }
                            });
        }
    }

    // Not implemented for AP environment
    @Override
    public long[] getPreloadedNanoAppIds() {
        return new long[0];
    }
}
