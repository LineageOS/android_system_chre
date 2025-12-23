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

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.ContextHubMessage;
import android.hardware.location.NanoApp;
import android.hardware.location.NanoAppFilter;
import android.hardware.location.NanoAppInstanceInfo;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.Assert;
import org.junit.Assume;

import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * TODO(b/30784270): This is an implementation of ContextHubManager designed
 * to workaround some of the bugs with the NYC-released version of the
 * ContextHubManager.
 *
 * This also rolls in some testing-specific checks, to consolidate those
 * since this is used across multiple tests.
 *
 * TODO(b/467212059): Separate the ContextHubManager workarounds from the testing-specific
 *     checks, so GMS Core can just use the ContextHubManager workarounds.
 */
public class GtsContextHubManagerWrapper {
    private static final String TAG = "GtsContextHubManagerWrapper";

    // This is a constant from the JNI layer.
    private static final int OS_APP_HANDLE = -1;

    // These definitions come from the Context Hub HAL, in
    // context_hub.h.
    private static final int MSG_ID_LOAD_APP = 3;
    private static final int MSG_ID_UNLOAD_APP = 4;
    private static final int MSG_ID_QUERY = 5;
    private static final int MSG_ID_REBOOT = 7;

    private static final int INT_BYTES = 4;  // Java language mandate.
    private static final long ALL_APPS = -1;  // Constant from JNI code
    private static final long DELAY_BEFORE_QUERY_MS = 250;


    private static final int HEADER_MAGIC =
                (((int) 'N' <<  0) | ((int) 'A' <<  8) | ((int) 'N' << 16) | ((int) 'O' << 24));
    private static final int HEADER_MAGIC_OFFSET = 4;
    private static final int HEADER_APP_ID_OFFSET = 8;

    /**
     * The Vendor ID we use for the our GTS testing nanoapps.
     *
     * The most significant five bytes of the App ID are the Vendor ID.
     * Standard Google nanoapps use "Googl".  We take "GoogT", with the
     * 'T' for testing, under the presumption we'll never give one of our
     * vendors an ID starting with "Goog".  Note that changing this ID
     * in the future shouldn't be a big problem, so we're not making any
     * long term decision here.
     */
    public static final long GTS_VENDOR_ID =
            (long) ('G') << 56
            | (long) ('o') << 48
            | (long) ('o') << 40
            | (long) ('g') << 32
            | (long) ('T') << 24;

    // This is how long we'll wait to get response to a load or unload
    // request.  Any longer than this, and we abort the test in failure.
    //
    // This is a generous amount of time.  We'd rather error on taking too
    // long to register a test failure, than having a test which is flaky
    // because sometimes it takes a little longer to run.
    private static final long LOAD_OR_UNLOAD_RESPONSE_MAX_WAIT_SECONDS = 50;

    // The maximum amount of time we'll wait for a query response before giving
    // up.
    private static final long QUERY_RESPONSE_MAX_WAIT_SECONDS = 5;

    private ContextHubManager.Callback mCallback = new ContextHubManager.Callback() {
        public void onMessageReceipt(int hubHandle, int nanoAppHandle,
                                     ContextHubMessage message) {
            Log.d(TAG, "On hub " + mHubHandle
                    + " we received a message for hub " + hubHandle + ": "
                    + GtsContextHubManagerWrapper.debugDump(message));

            // TODO(b/467212059): It's legit for there to be message traffic from other
            //     hubs and apps.  We want to make sure the CHRE which we are
            //     testing isn't sending out messages multiple times for some
            //     reason.  We'll fail if we never get the message we expect,
            //     but for right now we won't fail if the CHRE sends the message
            //     multiple times or sends other unexpected messages.
            if (hubHandle != mHubHandle) {
                Log.d(TAG, "Ignored message not meant for this hub");
                return;
            }

            if (nanoAppHandle == OS_APP_HANDLE) {
                try {
                    handleContextHubOsMessage(message);
                } catch (AssertionError e) {
                    // We presume this error is via JUnit.  JUnit only
                    // supports assertions from the main thread of the test.
                    // This callback is, by definition, invoked by a different
                    // thread.  So we pass this error to the main thread, so
                    // it can be thrown there.
                    mCallbackResult.add(new CallbackResult(e));
                }
            } else {
                if (mUserCallback != null) {
                    // TODO(b/80083382): Remove hackMessageFromNanoappToHost and
                    // use the messages directly.
                    ContextHubMessage hackedMessage = null;
                    try {
                        hackedMessage = hackMessageFromNanoappToHost(message);
                    } catch (BufferUnderflowException e) {
                        Log.w(TAG, "Received unexpected message from nanoapp");
                    }
                    if (hackedMessage != null) {
                        mUserCallback.onMessageReceipt(hubHandle, nanoAppHandle, hackedMessage);
                    }
                }
                // If there's no user callback setup, we just silently
                // drop the message, without even logging (since we presumably
                // don't care at all about these messages).
            }
        }
    };

    private static class CallbackResult {
        // If false, check if 'error != null' to see if this was an assertion.
        public final boolean isSuccessful;
        public final AssertionError assertion;
        // Note that jniId is only valid for result from Load calls.
        public final int jniId;

        private CallbackResult(boolean isSuccessful, AssertionError assertion,
                int jniId) {
            this.isSuccessful = isSuccessful;
            this.assertion = assertion;
            this.jniId = jniId;
        }
        CallbackResult(boolean isSuccessful, int jniId) {
            this(isSuccessful, null, jniId);
        }
        CallbackResult(boolean isSuccessful) {
            this(isSuccessful, -1);
        }
        CallbackResult(AssertionError assertion) {
            this(false, assertion, -1);
        }
    }

    // Note that Java does not allow scope an enum within an inner class.
    private enum RequestDataState {
        NOT_WAITING,
        AWAITING_LOAD_RESPONSE,
        AWAITING_UNLOAD_RESPONSE,
        AWAITING_QUERY_RESPONSE,
    }

    private static class RequestData {
        private RequestDataState mState;
        private long mData;

        RequestData() {
            reset();
        }

        public void reset() {
            mState = RequestDataState.NOT_WAITING;
        }

        public void prepareForLoad(long appId) {
            assertCurrentState(RequestDataState.NOT_WAITING);
            mState = RequestDataState.AWAITING_LOAD_RESPONSE;
            mData = appId;
        }

        public void prepareForQuery() {
            assertCurrentState(RequestDataState.NOT_WAITING);
            mState = RequestDataState.AWAITING_QUERY_RESPONSE;
        }

        public void prepareForUnload(int jniId) {
            assertCurrentState(RequestDataState.NOT_WAITING);
            mState = RequestDataState.AWAITING_UNLOAD_RESPONSE;
            mData = jniId;
        }

        public void doneWaiting() {
            mState = RequestDataState.NOT_WAITING;
        }

        public boolean isWaitingForLoad() {
            return mState == RequestDataState.AWAITING_LOAD_RESPONSE;
        }

        public boolean isWaitingForQuery() {
            return mState == RequestDataState.AWAITING_QUERY_RESPONSE;
        }

        public boolean isWaitingForUnload() {
            return mState == RequestDataState.AWAITING_UNLOAD_RESPONSE;
        }

        // Illegal to call unless isWaitingForLoad()
        public long getAppId() {
            assertCurrentState(RequestDataState.AWAITING_LOAD_RESPONSE);
            return mData;
        }

        // Illegal to call unless isWaitingForUnload()
        public int getJniId() {
            assertCurrentState(RequestDataState.AWAITING_UNLOAD_RESPONSE);
            return (int) mData;
        }

        private void assertCurrentState(RequestDataState expectedState) {
            assertThat(mState).isEqualTo(expectedState);
        }
    }


    private ContextHubManager mManager;
    private int mHubHandle;
    private ContextHubManager.Callback mUserCallback;

    // Data associate with our load and unload requests.
    private RequestData mRequestData = new RequestData();
    // Results from the callback for load/unload requests.
    private ArrayBlockingQueue<CallbackResult> mCallbackResult =
            new ArrayBlockingQueue<CallbackResult>(1);

    /**
     * Creates and wraps a ContextHubManager.
     *
     * Note that this is only testing across one Context Hub.
     *
     * The user must assure that the close() method is called on this
     * class when the test is done (best done via a test method marked as
     * @After ).
     *
     * @param callback  Method to call with non-OS messages.  This is allowed
     *     to be null.
     */
    public GtsContextHubManagerWrapper(ContextHubManager.Callback callback) {
        Context mContext =
                InstrumentationRegistry.getInstrumentation().getTargetContext();
        mManager = (ContextHubManager) mContext.getSystemService(Context.CONTEXTHUB_SERVICE);

        ContextHubHostTestUtil.checkDeviceShouldRunTest(mContext, mManager);

        int[] handles = mManager.getContextHubHandles();
        Assert.assertTrue(handles != null);
        Assume.assumeTrue("Skipping CHQTS for device with no Context Hub", handles.length != 0);

        // TODO(b/467212059): We need to support testing multiple hubs.  For now
        // we just assume CHRE at index 0 is the hub of interest and
        // has everything we want.
        mHubHandle = handles[0];

        mUserCallback = callback;
        int result = mManager.registerCallback(mCallback);
        Assert.assertEquals("Unable to register callback",
                0, result);
    }

    /**
     * This method must be called at the conclusion of the test.
     *
     * If tests start failing with:
     *     java.lang.AssertionError: Unable to register callback expected:<0> but was:<-1>
     * then one of them is probably failing to call this method.
     */
    public void close() {
        mManager.unregisterCallback(mCallback);
    }


    private class MessageResult {
        public final boolean isSuccessful;
        public final byte[] remainingMessage;
        MessageResult(boolean isSuccessful, byte[] remainingMessage) {
            this.isSuccessful = isSuccessful;
            this.remainingMessage = remainingMessage;
        }
    }

    private MessageResult getMessageResult(byte[] messageData,
                                           String commandType) {
        int dataLength = messageData.length;
        final int resultLength = 1;

        Assert.assertTrue("Undersized response for " + commandType
                + ".  Expected at least " + resultLength + " byte(s),"
                + " but got " + dataLength, dataLength >= resultLength);

        int result = messageData[0];

        // Always log this so we can figure it out later.
        Log.d(TAG, "Got " + result + " result for " + commandType);

        return new MessageResult(result == 0,
                                 Arrays.copyOfRange(messageData, resultLength,
                                                    dataLength));
    }

    private CallbackResult handleLoadAppMessage(ContextHubMessage message) {
        // NOTE: (b/31105001): This is subject to being flaky, as we can
        //     "fail" this just because some other process is loading/unloading
        //     nanoapp at this same time.
        MessageResult result = getMessageResult(message.getData(),
                                                "Loading Nanoapp");
        byte[] data = result.remainingMessage;

        // The bytes after the 'response' code are the app ID which the JNI
        // code has given (that is, _not_ the app ID which we provided for
        // loading (b/30810861)).  This is in the host endian (b/30807327).
        int dataLength = data.length;
        Assert.assertTrue("LoadAppMessage after response is expected to be"
                + " 4 bytes.  It was " + dataLength, (dataLength == 4));

        int jniAppId = ByteBuffer.wrap(data)
                .order(ByteOrder.nativeOrder())
                .getInt();

        // TODO(b/30835598): We don't know if this message was for the
        //     nanoapp we were trying to load.  As an indirect check,
        //     we at least see if there's an app with our ID loaded.

        if (!result.isSuccessful) {
            // If this load failed, there's no means to check it was a
            // failure for our app.  We assume this was for our app, and hope
            // for the best.
            return new CallbackResult(false);
        }

        NanoAppInstanceInfo info = getNanoAppInstanceInfo(jniAppId);
        if (info == null) {
            Log.d(TAG, "No NanoAppInstanceInfo for " + jniAppId);
        } else if (info.getAppId() != mRequestData.getAppId()) {
            Log.d(TAG, "Expected App ID " + mRequestData.getAppId()
                    + ", but load response is for App ID " + info.getAppId());
        } else {
            // It's possible that this app was already loaded, but this is
            // the best we can check for now.
            return new CallbackResult(true, jniAppId);
        }
        // Either we got the message for a different app being loaded, or
        // ended up with a buggy JNI App Id.  We'll return that we skipped
        // processing this message and see how things turn out.
        return null;
    }

    private CallbackResult handleQueryMessage(ContextHubMessage message) {
        MessageResult result = getMessageResult(message.getData(), "Query");

        // Note that we currently don't do anything with the query response, we
        // just send the message to force the Java and JNI caches to be
        // refreshed.

        return new CallbackResult(result.isSuccessful);
    }

    private CallbackResult handleUnloadAppMessage(ContextHubMessage message) {
        // NOTE: (b/31105001): This is subject to being flaky, as we can
        //     "fail" this just because some other process is loading/unloading
        //     nanoapp at this same time.
        MessageResult result = getMessageResult(message.getData(),
                                                "Unloading Nanoapp");
        byte[] data = result.remainingMessage;

        Assert.assertTrue("Unexpected extra data in response to unloading nanoapp:"
                          + Arrays.toString(data),
                          data.length == 0);

        // TODO(b/30835598): We don't know if this message was for the
        //     nanoapp we were trying to unload.  As an indirect check,
        //     we at least confirm there's no longer a NanoApp with our
        //     ID loaded.

        if (!result.isSuccessful) {
            // If this unload failed, there's no means to check it was a
            // failure for our app.  We assume this was for our app, and hope
            // for the best.
            return new CallbackResult(false);
        }

        int jniHandle = mRequestData.getJniId();
        NanoAppInstanceInfo info = getNanoAppInstanceInfo(jniHandle);
        if (info == null) {
            // It's possible that this app was already unloaded, but this is
            // the best we can check for now.
            return new CallbackResult(true);
        }
        // Either we got the message for a different app being unloaded, or
        // we're dealing with a buggy JNI App Id.  We'll return that we skipped
        // processing this message and see how things turn out.
        Log.d(TAG, "There is still nanoapp info for handle " + jniHandle
                + ", which has App ID " + info.getAppId());
        return null;
    }

    private void handleContextHubOsMessage(ContextHubMessage message) {
        int messageType = message.getMsgType();

        // TODO(b/30838000): Unfortunately, the C++ code discards any reboot
        //     details, so there's nothing useful we can log here.  If/when
        //     the C++ code is fixed, we should log more.
        Assert.assertTrue("While testing Context Hub " + mHubHandle
                + ", it rebooted", messageType != MSG_ID_REBOOT);

        // We only care about certain messages from the "OS" when we're
        // in specific states.  Otherwise we can ignore them.
        // This would be subject to race conditions, except that we require
        // only one loader/unloader at a time for users of our API.

        CallbackResult callbackResult = null;
        if ((messageType == MSG_ID_LOAD_APP) && mRequestData.isWaitingForLoad()) {
            callbackResult = handleLoadAppMessage(message);
        } else if ((messageType == MSG_ID_UNLOAD_APP) && mRequestData.isWaitingForUnload()) {
            callbackResult = handleUnloadAppMessage(message);
        } else if ((messageType == MSG_ID_QUERY) && mRequestData.isWaitingForQuery()) {
            callbackResult = handleQueryMessage(message);
        }

        if (callbackResult != null) {
            mRequestData.doneWaiting();
            mCallbackResult.add(callbackResult);
        } else {
            Log.d(TAG, "While testing Context Hub " + mHubHandle
                    + " we received an OS message we ignored: "
                    + debugDump(message));
        }
    }

    // TODO(b/32114261): Remove this method.
    protected ContextHubMessage hackMessageFromNanoappToHost(
            ContextHubMessage origMessage) {
        // For now, our nanohub HAL and JNI code end up not sending across the
        // message type of the user correctly.  So our testing protocol hacks
        // around this by putting the message type in the first four bytes of
        // the data payload, in little endian.
        ByteBuffer origData = ByteBuffer.wrap(origMessage.getData());
        origData.order(ByteOrder.LITTLE_ENDIAN);
        int newMessageType = origData.getInt();
        // The new data is the remainder of this array (which could be empty).
        byte[] newData = new byte[origData.remaining()];
        origData.get(newData);
        return new ContextHubMessage(newMessageType, origMessage.getVersion(),
                                     newData);
    }

    // TODO(b/30808791): Remove this when NanoApp's API is correctly treating
    // app IDs as 64-bits.
    // NOTE: This is basically a copy of ContextHubService.parseAppId().
    private static long getAppId(NanoApp app) {
        // NOTE: If this shifting seems odd (since it's actually "ONAN"), note
        //     that it matches how this is defined in context_hub.h.

        byte[] appBinary = app.getAppBinary();
        if (appBinary != null) {
            ByteBuffer header = ByteBuffer.wrap(appBinary)
                    .order(ByteOrder.LITTLE_ENDIAN);

            try {
                if (header.getInt(HEADER_MAGIC_OFFSET) == HEADER_MAGIC) {
                    // This is a legitimate nanoapp header.  Let's grab
                    // the app ID.
                    return header.getLong(HEADER_APP_ID_OFFSET);
                }
            } catch (IndexOutOfBoundsException e) {
                // The header is undersized.  We'll fall through to our code
                // path below, which handles being unable to parse the header.
            }
        }
        // We failed to parse the header.  Even through it's probably wrong,
        // let's give NanoApp's idea of our ID.  This is at least consistent.
        return app.getAppId();
    }

    /**
     * Load a nanoapp.
     *
     * This is a blocking load, which doesn't return until we have the
     * final result of loading.  It will abort the test in failure if it
     * takes too long to get the result.
     *
     * Note this, and unloadNanoApp(), are "synchronized".  There may not be
     * another load or unload taking place at the time of this call.
     *
     * This code will abort the test with failure under some conditions.
     * However, unsuccessful load of the nanoapp is not one of those
     * conditions.
     *
     * @param app  The NanoApp to load.
     * @return  The "handle" to the NanoApp on success; -1 on load failure.
     */
    public synchronized int loadNanoApp(NanoApp app) {
        // We reset, because a previous call could have thrown an exception,
        // and left this in a bad state.
        mRequestData.reset();
        mRequestData.prepareForLoad(getAppId(app));

        // TODO(b/31105001): This is a potential source of test flakiness,
        // if other processes are also loading nanoapps.
        if (mManager.loadNanoApp(mHubHandle, app) == -1) {
            // This failed to load.  This could be expected, so we pass this
            // failure back to the caller.  But we need to clean up our
            // state.
            Log.d(TAG, "loadNanoApp for app " + app.getName()
                    + " failed to send request to Context Hub");
            mRequestData.doneWaiting();
            return -1;
        }

        // Block until the callback has gotten our result.
        CallbackResult result = null;
        try {
            result =
                mCallbackResult.poll(LOAD_OR_UNLOAD_RESPONSE_MAX_WAIT_SECONDS,
                                     TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail("Unexpected Interrupt waiting for load result for "
                    + app.getName() + ": " + e.toString());
        }
        if (result == null) {
            // We will only get null in the case that our poll() call timed
            // out.  The user should never expect this to time out.  This is a
            // fatal test failure.
            Assert.fail("Timed out waiting for load response for "
                    + app.getName());
        }

        if (!result.isSuccessful) {
            if (result.assertion != null) {
                throw result.assertion;
            }
            // This load failed.  The user might have expected that, so we
            // just return here.
            return -1;
        }
        int nanoAppHandle = result.jniId;

        // The load succeeded.  Perform a couple consistency checks.
        NanoAppInstanceInfo info = getNanoAppInstanceInfo(nanoAppHandle);
        Assert.assertTrue("No NanoAppInstanceInfo available for " + app.getName(),
                          info != null);
        // TODO(b/30943489): Reinstate these checks.
        // Assert.assertTrue("Expected app name (" + appName + "), but got ("
        //         + info.getName() + ")",
        //         appName.equals(info.getName()));
        // Assert.assertTrue("Expected publisher name (" + kNanoAppPublisher
        //         + "), but got (" + info.getPublisher() + ")",
        //         kNanoAppPublisher.equals(info.getPublisher()));

        return nanoAppHandle;
    }

    /**
     * Send a QUERY request to force refresh of the ContextHubService Java and
     * JNI caches. Errors during this process are only logged (they do not
     * affect the test outcome), but the caller can use the return value to
     * decide whether to fail the test if this fails.
     *
     * @return true on success, false on failure
     */
    private synchronized boolean forceCacheRefresh() {
        boolean success = false;
        ByteBuffer bb = ByteBuffer.allocate(Long.BYTES)
                .order(ByteOrder.LITTLE_ENDIAN)
                .putLong(ALL_APPS);
        ContextHubMessage message = new ContextHubMessage(
                MSG_ID_QUERY, 0, bb.array());

        // It's possible/likely that the HAL is still completing the QUERY
        // sent as a result of our prior LOAD by the time we get here. We
        // want to force a new query, so delay - we don't have a way to tell
        // whether a query response was due to our request or a previous one,
        // so just delay here for a suitable amount of time.
        try {
            Thread.sleep(DELAY_BEFORE_QUERY_MS);
        } catch (InterruptedException e) {
            Log.w(TAG, "Interrupted from sleep prior to query");
        }

        mRequestData.prepareForQuery();
        int ret = mManager.sendMessage(mHubHandle, OS_APP_HANDLE, message);
        if (ret == 0) {
            try {
                // Ignore the result; we just want to wait until the query
                // response is processed by ContextHubService
                mCallbackResult.poll(QUERY_RESPONSE_MAX_WAIT_SECONDS,
                        TimeUnit.SECONDS);
                success = true;
            } catch (InterruptedException e) {
                Log.e(TAG, "Unexpected Interrupt while waiting for query "
                        + "response: " + e);
            }
        } else {
            Log.e(TAG, "Failed to send query message: " + ret);
        }

        // Reset our waiting state if the request failed
        if (!success) {
            mRequestData.reset();
        }

        return success;
    }

    /**
     * Unload all nanoapps on hub
     */
    private void unloadAllNanoApps() {
        NanoAppFilter filter = new NanoAppFilter(NanoAppFilter.APP_ANY,
                /* appVersion */ NanoAppFilter.FLAGS_VERSION_ANY,
                /* versionMask */ NanoAppFilter.FLAGS_VERSION_ANY,
                NanoAppFilter.VENDOR_ANY);

        boolean successful = true;
        for (int nanoAppHandle : mManager.findNanoAppOnHub(mHubHandle, filter)) {
            successful &= (unloadNanoApp(nanoAppHandle) == 0);
        }

        if (!successful) {
            Log.w(TAG, "Unable to unload all nanoapps");
        }
    }

    /**
     * Unload a nanoapp.
     *
     * This is a blocking unload, which doesn't return until we have the
     * final result of unloading.  It will abort the test in failure if it
     * takes too long to get the result.
     *
     * Note this, and loadNanoApp(), are "synchronized".  There may not be
     * another load or unload taking place at the time of this call.
     *
     * This code will abort the test with failure under some conditions.
     * However, unsuccessful unload of the nanoapp is not one of those
     * conditions.
     *
     * @param nanoAppHandle  The "handle" to the NanoApp to unload.
     * @return  0 on success; -1 on failure.
     */
    public synchronized int unloadNanoApp(int nanoAppHandle) {
        // We reset, because a previous call could have thrown an exception,
        // and left this in a bad state.
        mRequestData.reset();

        // Perform a query to make sure the Java/JNI caches are not out of sync
        // with the state of the hub. This could happen, for example, if the
        // test nanoapp called chreAbort() and was unloaded - currently there
        // is no push notification of this behavior.
        // TODO(b/30835981): Remove this once underlying cache synchronization
        // issues are resolved
        forceCacheRefresh();

        NanoAppInstanceInfo info = getNanoAppInstanceInfo(nanoAppHandle);
        // It's possible people may attempt this method on nanoapps which no
        // longer exist.  And we want to support that.  But if a nanoapp
        // didn't exist prior to unload, and then the hub claims to have
        // successfully unloaded it, we know that's a bug under any circumstances.
        boolean unloadMustFail = (info == null);

        mRequestData.prepareForUnload(nanoAppHandle);

        if (mManager.unloadNanoApp(nanoAppHandle) == -1) {
            // This failed to unload.  This could be expected, so we pass this
            // failure back to the caller.  But we need to clean up our
            // state.
            Log.d(TAG, "unloadNanoApp for app handle " + nanoAppHandle
                    + " failed to send request to Context Hub");
            mRequestData.doneWaiting();
            return -1;
        }

        // Block until the callback has gotten our result.
        CallbackResult result = null;
        try {
            result =
                mCallbackResult.poll(LOAD_OR_UNLOAD_RESPONSE_MAX_WAIT_SECONDS,
                                     TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Assert.fail("Unexpected Interrupt waiting for unload result for "
                    + nanoAppHandle + ": " + e.toString());
        }
        if (result == null) {
            // We will only get null in the case that our poll() call timed
            // out.  The user should never expect this to time out.  This is a
            // fatal test failure.
            Assert.fail("Timed out waiting for unload response for " + nanoAppHandle);
        }

        if (result.isSuccessful) {
            Assert.assertFalse("Could not find NanoAppInstanceInfo for handle "
                               + nanoAppHandle + " prior to unload, yet unload"
                               + " claimed success.",
                               unloadMustFail);
            info = getNanoAppInstanceInfo(nanoAppHandle);
            if (info != null) {
                // While it's better form to use
                // Assert.assertTrue("...", info == null);
                // the text message gets generated unconditionally, and
                // we want info.getAppId() in the failure case, which is
                // referencing a null pointer in the success case.
                Assert.fail("We still have NanoAppInstanceInfo for appId "
                            + info.getAppId() + ", handle " + nanoAppHandle
                            + " after a successful unload.");
            }
            return 0;
        }
        if (result.assertion != null) {
            throw result.assertion;
        }
        return -1;
    }

    // TODO(b/32114261): Remove this method.
    protected ContextHubMessage hackMessageFromHostToNanoapp(int messageType,
                                                             byte[] origData) {
        // For NYC, we are not able to assume that the messageType correctly
        // makes it to the nanoapp.  So we put it, in little endian, as the
        // first four bytes of the message.
        ByteBuffer newData = ByteBuffer.allocate(INT_BYTES + origData.length);
        newData.order(ByteOrder.LITTLE_ENDIAN);
        newData.putInt(messageType);
        newData.put(origData);
        return new ContextHubMessage(messageType, 0, newData.array());
    }

    /**
     * Sends a message to a nanoapp.
     *
     * @param nanoAppHandle  The "handle" to the nanoapp
     * @param messageType  The "type" of the message
     * @param data  The data from the message (can be empty array)
     *
     * @return 0 on success; -1 on failure
     */
    public int sendMessage(int nanoAppHandle, int messageType, byte[] data) {
        ContextHubMessage message = hackMessageFromHostToNanoapp(messageType,
                                                                 data);

        return mManager.sendMessage(mHubHandle, nanoAppHandle, message);
    }

    /**
     * Get the ContextHubInfo for the hub we're managing.
     *
     * @return The appropriate ContextHubInfo
     */
    public ContextHubInfo getContextHubInfo() {
        return mManager.getContextHubInfo(mHubHandle);
    }

    /**
     * Get the NanoAppInstanceInfo for a nanoapp
     *
     * @return The appropriate NanoAppInstanceInfo
     */
    public NanoAppInstanceInfo getNanoAppInstanceInfo(int nanoAppHandle) {
        return mManager.getNanoAppInstanceInfo(nanoAppHandle);
    }

    /**
     * Helper method designed to give better logging output.
     *
     * TODO(b/31069172): Move this method to be ContextHubMessage.toString().
     *
     * @param message  The message we want a string representation of.
     * @return  A string representation of the message.
     */
    public static String debugDump(ContextHubMessage message) {
        if (message == null) {
            return "ContextHubMessage[NULL]";
        }
        byte[] data = message.getData();
        int length = data.length;
        String ret = "ContextHubMessage[type=" + message.getMsgType()
                + ",length=" + length + "](";
        if (length > 0) {
            ret += "0x";
        }
        final int maxOutputLength = 16;
        final char[] hexArray = "0123456789ABCDEF".toCharArray();
        for (int i = 0; (i < data.length) && (i < maxOutputLength); i++) {
            // Java lacks an unsigned type.  Furthermore, given the code:
            //   byte b = [...];
            //   [...] array[b >>> 4];
            // it would appear that Java promotes the byte to an int prior
            // to performing the shift.  So, when we have a negative value
            // for our signed type, this means that we don't get the result
            // we want.  Thus, we perform some gymnastics.
            byte b = data[i];
            int realByteValue = ((int) b) & 0xFF;
            ret += Character.toString(hexArray[realByteValue >>> 4]);
            ret += Character.toString(hexArray[realByteValue & 0x0F]);
        }
        if (length > 16) {
            ret += "...";
        }
        ret += ")";

        return ret;
    }
}
