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

public class ContextHubAPNative {
    static {
        // The runtime will add "lib" on the front and ".so" on the end of
        // the name supplied to loadLibrary.
        System.loadLibrary("chre_jni");
    }

    static native int init();

    static native void destroy();

    static native void nativeRegister(ContextHubAPManager instance);

    static native boolean loadNanoAppFromFile(String filename);

    static native boolean unloadNanoApp(long nanoAppInstanceId);

    /**
     * List running nanoapps on CHRE AP.
     *
     * @return NanoAppInfo array
     */
    public static native NanoAppState[] listNanoapps();

    /**
     * Send message to nanoapp.
     *
     * @param nanoAppId   NanoApp ID
     * @param messageType Message type
     * @param message     Message body
     * @param messageSize Message size
     * @return {@code true} if the message was sent successfully, {@code false} otherwise.
     */
    static native boolean sendMessage(
            long nanoAppId, int messageType, byte[] message, int messageSize);

    // Called by native JNI library.
    static void onMessageReceived(long nanoAppId, int messageType, byte[] messageBody) {
        ContextHubAPManager.getInstance().onMessageFromNanoApp(nanoAppId, messageType, messageBody);
    }
}
