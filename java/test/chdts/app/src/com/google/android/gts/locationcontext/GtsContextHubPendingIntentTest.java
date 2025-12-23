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

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.google.android.utils.chre.ContextHubHostTestUtil.createNanoAppBinary;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.hardware.location.ContextHubClient;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.NanoAppBinary;
import android.os.Build;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.google.android.chre.test.chqts.ContextHubPendingIntentTestExecutor;
import com.google.android.utils.chre.ContextHubServiceTestHelper;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** A basic test for PendingIntent APIs of Context Hub Service. */
@RunWith(AndroidJUnit4.class)
public class GtsContextHubPendingIntentTest extends GtsContextHubTestBase {
    @Rule public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    public static final String ACTION = ContextHubPendingIntentTestExecutor.ACTION;

    private final Context mContext = getInstrumentation().getTargetContext();
    private final ContextHubPendingIntentTestExecutor mExecutor;
    private final long mNanoAppId;
    private final NanoAppBinary mNanoAppBinary;
    private final ContextHubServiceTestHelper mTestHelper;

    public GtsContextHubPendingIntentTest() {
        ContextHubInfo contextHubInfo = getContextHubInfo();
        mNanoAppBinary = createNanoAppBinary(contextHubInfo, "who_am_i.napp");
        mNanoAppId = mNanoAppBinary.getNanoAppId();
        mExecutor = new ContextHubPendingIntentTestExecutor(getContextHubManager(), contextHubInfo,
                mNanoAppBinary);
        mTestHelper = new ContextHubServiceTestHelper(contextHubInfo, getContextHubManager());
    }

    @Before
    public void setUp() throws InterruptedException, TimeoutException {
        mExecutor.init();
    }

    @Test
    public void basicPendingIntentTest() {
        long dummyNanoAppId = createNanoAppBinary(getContextHubInfo(),
                "do_nothing.napp").getNanoAppId();
        mExecutor.basicPendingIntentTest(dummyNanoAppId);
    }

    /**
     * This test does the following things:
     * - Loads a nanoapp, then starts a service in a different process, which will create a
     * ContextHubClient and asks the nanoapp the host endpoint ID. -
     * - Polls for the message in the test thread, and gets the host endpoint ID of the client
     * generated in the service.
     * - Regenerates the same ContextHubClient in the test thread, and verifies that the host
     * endpoint ID remains consistent. We do this in the test thread to ensure that different
     * processes can create the same ContextHubClient with the same host endpoint ID (since
     * restarting the service may result in using a cached process).
     * - Stops the service, unloads the nanoapp, and closes the ContextHubClient.
     */
    // TODO(b/294222151): Remove this test from GTS in favor of PTS
    @Ignore
    public void pendingIntentServiceTest() throws InterruptedException {
        mTestHelper.loadNanoAppAssertSuccess(mNanoAppBinary);

        Intent intent = new Intent(GtsContextHubPendingIntentTest.ACTION)
                .setPackage(mContext.getPackageName());
        PendingIntent pendingIntent = PendingIntent.getBroadcast(mContext, /* requestCode= */ 0,
                intent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_MUTABLE);

        // Start a service that will send a message to the nanoapp
        // We use getContext() here rather than getTargetContext() to get the right component of
        // the service.
        Intent serviceIntent = new Intent(getInstrumentation().getContext(),
                GtsContextHubPendingIntentService.class)
                .putExtra(GtsContextHubPendingIntentService.EXTRA_NANOAPP_ID, mNanoAppId)
                .putExtra(GtsContextHubPendingIntentService.EXTRA_CONTEXT_HUB_INFO,
                        getContextHubInfo())
                .putExtra(GtsContextHubPendingIntentService.EXTRA_PENDING_INTENT, pendingIntent);

        IntentServiceConnection mConnection = new IntentServiceConnection();
        mContext.bindService(serviceIntent, mConnection, Context.BIND_AUTO_CREATE);
        mContext.startService(serviceIntent);
        mConnection.waitForComplete();
        mContext.unbindService(mConnection);
        mContext.stopService(serviceIntent);

        short hostEndpointId = mExecutor.waitForIdFromNanoApp();
        Log.d("ContextHubPendingIntentTest", "My host endpoint ID is " + hostEndpointId);

        ContextHubClient mContextHubClient = mExecutor.createClient(mNanoAppId);
        Assert.assertNotNull("Failed to regenerate PendingIntent client", mContextHubClient);
        Assert.assertEquals(mExecutor.getIdFromNanoApp(), hostEndpointId);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            Assert.assertEquals(mContextHubClient.getId(), hostEndpointId);
        }
        mContextHubClient.close();

        mContextHubClient = null;
        mTestHelper.unloadNanoAppAssertSuccess(mNanoAppId);
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }

    /**
     * A class used to connect to the GtsContextHubPendingIntentService used in the
     * pendingIntentServiceTest to receive notifications from the service.
     */
    private static class IntentServiceConnection implements ServiceConnection {
        private static final long TIMEOUT_SECONDS = 5;

        private final CountDownLatch mDoneLatch = new CountDownLatch(1);

        private String mAssertString = null;

        private final IGtsContextHubPendingIntentServiceCallback mCallback =
                new IGtsContextHubPendingIntentServiceCallback.Stub() {
                    @Override
                    public void onComplete() {
                        mDoneLatch.countDown();
                    }

                    @Override
                    public void onAssert(String error) {
                        mAssertString = error;
                    }
                };

        @Override
        public void onServiceConnected(ComponentName className, IBinder binderService) {
            IGtsContextHubPendingIntentService service =
                    IGtsContextHubPendingIntentService.Stub.asInterface(binderService);
            try {
                service.registerCallback(mCallback);
            } catch (RemoteException e) {
                Assert.fail("Failed to register assert callback");
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName className) {
        }

        /*
         * Waits for the service to indicate completion, and asserts if the service has timed
         * out or asserted.
         */
        private void waitForComplete() throws InterruptedException {
            boolean done = mDoneLatch.await(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            if (!done) {
                Assert.fail("Timed out while waiting for service to finish");
            }

            if (mAssertString != null) {
                Assert.fail(mAssertString);
            }
        }
    }
}
