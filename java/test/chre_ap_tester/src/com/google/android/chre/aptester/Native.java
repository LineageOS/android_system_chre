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


public class Native {
    static {
        // The runtime will add "lib" on the front and ".so" on the end of
        // the name supplied to loadLibrary.
        System.loadLibrary("chre_jni");
    }

    public static class NanoAppInfo {
        long mInstanceId;
        String mName;
    }

    static native int init();

    static native void destroy();

    static native boolean loadNanoAppFromFile(String filename);

    static native boolean unloadNanoApp(long nanoAppInstanceId);

    static native NanoAppInfo[] listNanoapps();

    static native boolean sendMessage(
            long nanoAppId, int messageType, byte[] message, int messageSize);

    /**
     * Called by native JNI library.
     */
    public static void onMessageReceived(long nanoAppId, int messageType, byte[] messageBody) {
        ContextHubAPManager.getInstance().onMessageFromNanoApp(nanoAppId, messageType, messageBody);
    }
}
