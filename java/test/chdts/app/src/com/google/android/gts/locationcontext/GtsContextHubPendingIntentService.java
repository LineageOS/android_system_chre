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

package com.google.android.gts.locationcontext;

import android.app.IntentService;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.hardware.location.ContextHubClient;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.ContextHubTransaction;
import android.hardware.location.NanoAppMessage;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import com.google.android.chre.test.chqts.ContextHubTestConstants;

import org.junit.Assert;

/**
 * A service that will create a PendingIntent-based ContextHubClient when started, and sends
 * an empty message to a specified nanoapp. To start the service, the ContextHubInfo and nanoapp ID
 * must be specified through the EXTRA_NANOAPP_ID and EXTRA_CONTEXT_HUB_INFO extras. This class does
 * not close the generated ContextHubClient, so it must be done outside of this class.
 */
public class GtsContextHubPendingIntentService extends IntentService {
    public static final String EXTRA_NANOAPP_ID =
            "com.google.android.gts.locationcontext.extra.EXTRA_NANOAPP_ID";

    public static final String EXTRA_CONTEXT_HUB_INFO =
            "com.google.android.gts.locationcontext.extra.EXTRA_CONTEXT_HUB_INFO";

    public static final String EXTRA_PENDING_INTENT =
            "com.google.android.gts.locationcontext.extra.EXTRA_PENDING_INTENT";

    private static final String TAG = "GtsContextHubPendingIntentService";

    private static final int MESSAGE_TYPE =
            ContextHubTestConstants.MessageType.SERVICE_MESSAGE.asInt();

    private ContextHubManager mContextHubManager;

    private ContextHubClient mContextHubClient;

    private ServiceProvider mServiceProvider = new ServiceProvider();

    public GtsContextHubPendingIntentService() {
        super("GtsContextHubPendingIntentService");
    }

    private static class ServiceProvider extends IGtsContextHubPendingIntentService.Stub {
        private IGtsContextHubPendingIntentServiceCallback mCallback;

        @Override
        public void registerCallback(IGtsContextHubPendingIntentServiceCallback callback) {
            mCallback = callback;
        }

        public void notifyAssert(String error) {
            if (mCallback != null) {
                try {
                    mCallback.onAssert(error);
                } catch (RemoteException e) {
                    Log.e(TAG, "Failed to post assert to main test: " + error);
                }
            }
        }

        public void notifyComplete() {
            if (mCallback != null) {
                try {
                    mCallback.onComplete();
                } catch (RemoteException e) {
                    Log.e(TAG, "Failed to post complete to main test");
                }
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mServiceProvider;
    }

    /**
     * The IntentService calls this method from the default worker thread with
     * the intent that started the service. When this method returns, IntentService
     * stops the service, as appropriate.
     */
    @Override
    public void onHandleIntent(Intent intent) {
        mContextHubManager = (ContextHubManager) getSystemService(Context.CONTEXTHUB_SERVICE);

        Assert.assertNotNull("Context Hub Manager not found", mContextHubManager);

        ContextHubInfo contextHubInfo = intent.getParcelableExtra(EXTRA_CONTEXT_HUB_INFO);
        if (contextHubInfo == null) {
            mServiceProvider.notifyAssert("ContextHubInfo extra not found");
            return;
        }
        long nanoAppId = intent.getLongExtra(EXTRA_NANOAPP_ID, -1);
        if (nanoAppId == -1) {
            mServiceProvider.notifyAssert("Nanoapp ID extra not found");
            return;
        }
        PendingIntent pendingIntent = intent.getParcelableExtra(EXTRA_PENDING_INTENT);
        if (pendingIntent == null) {
            mServiceProvider.notifyAssert("PendingIntent extra not found");
            return;
        }

        onInit(contextHubInfo, nanoAppId, pendingIntent);
    }

    private void onInit(
            ContextHubInfo contextHubInfo, long nanoAppId, PendingIntent pendingIntent) {
        try {
            mContextHubClient =
                    mContextHubManager.createClient(contextHubInfo, pendingIntent, nanoAppId);
        } catch (IllegalStateException e) {
            mServiceProvider.notifyAssert("Failed to create client: " + e.getMessage());
            return;
        }
        if (mContextHubClient == null) {
            mServiceProvider.notifyAssert("Failed to create client");
            return;
        }

        NanoAppMessage message = NanoAppMessage.createMessageToNanoApp(
                nanoAppId, MESSAGE_TYPE, new byte[0]);
        int result = mContextHubClient.sendMessageToNanoApp(message);
        if (result != ContextHubTransaction.RESULT_SUCCESS) {
            mServiceProvider.notifyAssert("Send message failed with error code " + result);
            return;
        }

        mServiceProvider.notifyComplete();
    }
}
