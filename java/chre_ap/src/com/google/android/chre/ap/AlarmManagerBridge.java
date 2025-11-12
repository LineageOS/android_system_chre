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

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;

public class AlarmManagerBridge {
    private static final String TAG = "AlarmManagerBridge";
    private static final String ACTION_ALARM_FIRED = "CHRE_AP_ALARM_FIRED";
    private static Context sContext;
    private static AlarmManager sAlarmManager;

    static void initialize(Context appContext) {
        sContext = appContext;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            sContext.registerReceiver(
                    new AlarmReceiver(),
                    new IntentFilter(ACTION_ALARM_FIRED),
                    Context.RECEIVER_NOT_EXPORTED);
        } else {
            sContext.registerReceiver(
                    new AlarmReceiver(),
                    new IntentFilter(ACTION_ALARM_FIRED));
        }
        sAlarmManager = sContext.getSystemService(AlarmManager.class);
        if (sAlarmManager == null) {
            Log.e(TAG, "Failed to get AlarmManager!");
        }
    }

    static void setAlarm(long timerId, long delayMillis) {
        if (sContext == null || sAlarmManager == null) {
            Log.e(TAG, "Bridge not initialized!");
            return;
        }
        if (delayMillis == 0) {
            delayMillis = 1;
        }

        long triggerAtMillis = SystemClock.elapsedRealtime() + delayMillis;

        Intent intent = new Intent(ACTION_ALARM_FIRED);
        intent.putExtra("timerId", timerId);
        intent.setPackage(sContext.getPackageName());

        PendingIntent pendingIntent =
                PendingIntent.getBroadcast(
                        sContext,
                        (int) timerId,
                        intent,
                        PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);

        sAlarmManager.set(AlarmManager.ELAPSED_REALTIME_WAKEUP, triggerAtMillis, pendingIntent);
    }

    static void cancelAlarm(long timerId) {
        if (sContext == null || sAlarmManager == null) {
            Log.e(TAG, "Bridge not initialized!");
            return;
        }

        Intent intent = new Intent(ACTION_ALARM_FIRED);
        intent.setPackage(sContext.getPackageName());

        PendingIntent pendingIntent =
                PendingIntent.getBroadcast(
                        sContext,
                        (int) timerId,
                        intent,
                        PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);

        sAlarmManager.cancel(pendingIntent);
    }

    public static class AlarmReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent != null && ACTION_ALARM_FIRED.equals(intent.getAction())) {
                long timerId = intent.getLongExtra("timerId", 0);
                if (timerId != 0) {
                    ContextHubAPNative.onAlarmFired(timerId);
                }
            }
        }
    }
}
