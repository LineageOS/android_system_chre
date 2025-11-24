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

import android.content.Context;
import android.telephony.CellInfo;
import android.telephony.TelephonyManager;
import android.util.Log;

import java.util.List;

public class ContextHubAPNative {
    private static final String TAG = "ContextHubAPNative";

    private static Context sContext;

    public static void setContext(Context context) {
        sContext = context.getApplicationContext();
    }

    static {
        // The runtime will add "lib" on the front and ".so" on the end of
        // the name supplied to loadLibrary.
        System.loadLibrary("chre_jni");
    }

    static native int init();

    static native void destroy();

    static native void runEventLoop(boolean useNativeThread);

    static native void nativeRegister(ContextHubAPManager instance);

    static native boolean loadNanoAppFromFile(String filename);

    static native boolean unloadNanoApp(long nanoAppInstanceId);

    static native NanoAppState[] listNanoapps();

    static native boolean isInitialized();

    static native void stopEventLoop();

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

    // Called when an alarm is fired.
    static native void onAlarmFired(long timerId);

    /**
     * Called by native code to get WWAN capabilities.
     */
    static int getWwanCapabilities() {
        TelephonyManager tm = sContext.getSystemService(TelephonyManager.class);
        if (tm == null) {
            return 0; // CHRE_WWAN_CAPABILITIES_NONE
        }
        // Assume basic cell info support if TelephonyManager is present.
        // CHRE_WWAN_GET_CELL_INFO = 1
        return 1;
    }

    /**
     * Called by native code to request cell info.
     * According to requirements: purely cache based, no new scan, no new threads.
     */
    static boolean requestWwanCellInfo() {
        TelephonyManager tm = sContext.getSystemService(TelephonyManager.class);

        if (tm == null) {
            Log.e(TAG, "TelephonyManager not found");
            return false;
        }

        try {
            // getAllCellInfo returns cached data and does not trigger a scan.
            // Requires ACCESS_FINE_LOCATION permission in the Manifest.
            List<CellInfo> cellInfoList = tm.getAllCellInfo();

            if (cellInfoList == null) {
                Log.w(TAG, "TelephonyManager returned null cell info list");
                // Even if null, we should respond to CHRE to complete the async request
                onCellInfoReceived(new Object[0]);
            } else {
                onCellInfoReceived(cellInfoList.toArray());
            }
            return true;
        } catch (SecurityException e) {
            Log.e(TAG, "Security exception getting cell info", e);
            return false;
        } catch (Exception e) {
            Log.e(TAG, "Error getting cell info", e);
            return false;
        }
    }

    /**
     * Native method to pass CellInfo data back to CHRE.
     * using Object[] to avoid generic array creation issues in JNI signatures.
     */
    static native void onCellInfoReceived(Object[] cellInfoList);
}
