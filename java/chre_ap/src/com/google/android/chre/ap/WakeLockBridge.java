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
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.util.Log;

import java.util.HashMap;
import java.util.Map;

public class WakeLockBridge {
    private static final String TAG = "WakeLockBridge";

    // Manage multiple locks identified by name
    private static final Map<String, WakeLock> sLocks = new HashMap<>();
    private static LockFactory sLockFactory = null;

    public interface LockFactory {
        // Returns a new wake lock with the given name.

        /**
         * Returns a new wake lock with the given name.
         * @param lockName Name of the lock
         * @return New wake lock
         */
        WakeLock newWakeClock(String lockName);
    }

    public static class DefaultLockFactory implements LockFactory {
        private final Context mContext;

        public DefaultLockFactory(Context ctx) {
            mContext = ctx;
        }

        /**
         * Returns a new wake lock with the given name from PowerManager.
         * @param lockName Name of the lock
         * @return New wake lock
         */
        public WakeLock newWakeClock(String lockName) {
            PowerManager pm = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
            if (pm == null) {
                Log.e(TAG, "PowerManager service not found");
                return null;
            }
            return pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, lockName);
        }
    }

    /**
     * set LockFactory to use, must be called in during initialization.
     *
     * @param factory LockFactory to use.
     */
    public static void setLockFactory(LockFactory factory) {
        sLockFactory = factory;
    }

    /**
     * Called by Native/JNI to acquire a wake lock.
     *
     * @param lockName      Unique identifier for the lock.
     * @param timeoutMillis Duration in milliseconds to hold the lock.
     */
    public static void acquireWakeLock(String lockName, long timeoutMillis) {
        if (sLockFactory == null) {
            Log.e(TAG, "LockFactory not set!");
            return;
        }

        synchronized (sLocks) {
            PowerManager.WakeLock wl = sLocks.get(lockName);

            // Create a new lock if it doesn't exist
            if (wl == null) {
                String tag = "CHRE_AP:" + lockName;
                wl = sLockFactory.newWakeClock(tag);
                wl.setReferenceCounted(true);
                sLocks.put(lockName, wl);
            }

            // Acquire the lock with a timeout safety net
            if (timeoutMillis == 0) {
                timeoutMillis = 1000; // 1s by default
            }
            wl.acquire(timeoutMillis);
            Log.d(TAG, "Acquired lock: " + lockName + " for " + timeoutMillis + "ms");
        }
    }

    /**
     * Called by Native/JNI to release a wake lock.
     *
     * @param lockName Unique identifier for the lock.
     */
    public static void releaseWakeLock(String lockName) {
        synchronized (sLocks) {
            WakeLock wl = sLocks.get(lockName);

            if (wl != null) {
                try {
                    // Only release if it is actually held
                    if (wl.isHeld()) {
                        wl.release();
                        Log.d(TAG, "Released lock: " + lockName);
                    }
                    if (!wl.isHeld()) {
                        sLocks.remove(lockName);
                    }
                } catch (RuntimeException e) {
                    Log.e(TAG, "Error releasing lock " + lockName + ": " + e.getMessage());
                }
            } else {
                Log.w(TAG, "Attempted to release non-existent lock: " + lockName);
            }
        }
    }
}
