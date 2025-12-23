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

import android.hardware.location.ContextHubMessage;
import android.util.Log;

import com.google.android.chre.test.chqts.ContextHubTestConstants.MessageType;
import com.google.android.chre.test.chqts.ContextHubTestConstants.TestNames;

import org.junit.Assert;

import java.nio.charset.Charset;
import java.util.HashMap;
import java.util.Map.Entry;

/**
 * A base class for all Nanoapp tests which use the "general" test framework.
 *
 * This base class is intended to be used on the Java side for tests which
 * use the "general_test" nanoapp(s) from
 * system/chre/java/test/chdts/app/src/com/google/android/gts/locationcontext/
 * running on the Context Hub.
 *
 * While this class is rather complex, the hope is that classes implementing
 * an individual test can be rather simple.  (Also, as we fix the wide variety
 * of bugs that this infrastructure is working around, hopefully this code
 * gets simpler as well.)
 *
 * A General test is expected to use one (or more) instances of the
 * general_test nanoapp.  We use the ContextHubTestConstants.TestNames to
 * track those instances, and the test must implement getTestNames() to
 * let this framework know which to launch.
 *
 * It's expected that a test has a "protocol" defined between the Host Java code
 * and the Nanoapp code.  See the "Test Protocol" discussion below.
 *
 * The beginning of the protocol (Host sending the TestNames) is handled
 * by this shared infrastructure.  Also, any unambiguous responses from
 * the Nanoapp (SUCCESS, FAILURE, INTERNAL_FAILURE, and INVALID_MESSAGE_TYPE)
 * are handled by this shared infrastructure.  Other messages must be properly
 * handled with the test's implementation of handleMessageFromNanoApp().
 *
 * There are convenience functions to help tests, such as
 * sendMessageToNanoApp() for continuing the protocol, and others for
 * easier error reporting.
 *
 * A test successfully passes in one of two ways:
 * - MessageType.SUCCESS received from the Nanoapp by this infrastructure.
 * - A call to GtsContextHubGeneralNanoAppTestBase.pass() by the test code.
 *
 * A test fails in one of several ways:
 * - A failure in attempting to perform basic test infrastructure methods
 *   (loading a nanoapp, sending a message to a nanoapp, etc).
 * - A failing MessageType received from the Nanoapp by this infrastructure.
 * - A call to one of the Assert.* methods from the test code which fails.
 * - A timeout running the test, failing to make it to a pass or fail state
 *   before getTestTimeoutSeconds() time.
 *
 * Note that philosophically, the test infrastructure fails the test as
 * soon as an issue is encountered, instead of passing failures along
 * further up the stack.
 *
 * An implementor of an individual test should hopefully just be able to
 * focus on the (documented) protected methods of this class, and not need
 * to dive into any of the rest of this code.
 *
 *
 * <b>Test "Protocol".</b>
 *
 * Our tests have a Java/Host component, and a C++/Nanoapp component.  They
 * communicate via constants defined in ContextHubTestConstants (TestNames
 * and MessageType), which are also identically defined in enums on the
 * C++/Nanoapp side.
 *
 * A Protocol for a test will always begin with the Host sending the TestNames
 * for the nanoapp(s) being run for the test.  From there, it's up to the
 * test to define its protocol between the Host and the Nanoapp.  The protocol
 * consists of one side sending a MessageType, optionally followed by data.
 * For new tests requiring new MessageTypes with custom meanings, the
 * MessageType enum should be expanded (in the Java and C++ code).
 *
 * There is no requirement that the protocol be symmetric.  It can have the
 * Host send X number of messages without any responses from Nanoapp, or
 * vice versa.  There is also no requirement that the Nanoapp sends a
 * MessageType.SUCCESS to conclude a test.  (For example, a protocol may
 * involve the Nanoapp sending some data to the Host, and the host evaluating
 * that data to determine if the test passed or failed.)
 *
 * There are a couple of universally recognized transactions in the protocol:
 * - Nanoapp sends MessageType.SUCCESS: This test is complete and a success.
 *   No data is accepted with this.
 * - Nanoapp sends MessageType.FAILURE or MessageType.INTERNAL_FAILURE: This
 *   test has failed.  If there is data, it is ASCII text describing the
 *   failure.
 */
public abstract class GtsContextHubGeneralNanoAppTestBase extends GtsContextHubNanoAppTestBase {
    private static final String TAG = "GtsContextHubGeneralNanoAppTestBase";

    @Override
    protected void messageHandler(int hubHandle, int nanoAppHandle,
                                  ContextHubMessage message) {
        if (!mNanoAppHandles.containsKey(nanoAppHandle)) {
            Log.d(TAG, "While testing with apps " + mNanoAppHandles.toString()
                    + " we received a message for app " + nanoAppHandle + ": "
                    + GtsContextHubManagerWrapper.debugDump(message));
            // Ignore message.
            return;
        }

        int messageType = message.getMsgType();
        MessageType messageEnum = MessageType.fromInt(messageType,
                "message to app " + nanoAppHandle);
        TestNames testName = mNanoAppHandles.get(nanoAppHandle);
        byte[] data = message.getData();

        switch (messageEnum) {
            case INVALID_MESSAGE_TYPE:  // fall-through
            case FAILURE:  // fall-through
            case INTERNAL_FAILURE:
                // These are univeral failure conditions for all tests.
                // If they have data, it's expected to be an ASCII string.
                String errorString = new String(data, Charset.forName("US-ASCII"));
                Assert.fail("Test " + testName + " got " + messageEnum
                            + ": " + errorString);
                // While we should never get past fail(), we put this
                // 'break' for clarity.
                break;

            case SKIPPED:
                // TODO(b/28386054): The GTS infrastructure doesn't support
                // SKIPPED.  So we'll "pass" this, even though that's highly
                // misleading.  We'll at least log this, but no one is ever
                // going to notice that.  There doesn't appear to be a way
                // to "pass" with a message, either.
                String reason = new String(data, Charset.forName("US-ASCII"));
                Log.w(TAG, "SKIPPED " + testName + ": " + reason);
                pass();
                break;

            case SUCCESS:
                // This is a universal success for the test.  We ignore
                // 'data'.
                pass();
                break;

            default:
                // We don't know how to react to this universally,
                // and need to have the individual test process this.
                handleMessageFromNanoApp(testName, messageEnum, data);
        }
    }

    private static final String NANO_APP_FILENAME_BASE = "general_test";
    private static final String NANO_APP_FILENAME_EXTENSION = ".napp";
    private static final String NANO_APP_NAME_BASE = "GTS General Test:";

    // This is a mapping of the JNI provided handle for our nanoapps to/from
    // our TestNames.
    // TODO(b/30810861): Hopefully we can switch this to the more meaningful
    //     mapping of our TestNames to/from the Nanoapp ID.
    private HashMap<Integer, TestNames> mNanoAppHandles =
            new HashMap<Integer, TestNames>();

    // TODO(b/31727154): When we're able to load multiple instances of a
    // single nanoapp, this can go away and we'll use the same app for
    // all.
    private String getNanoAppFilename(int index) {
        // Our naming scheme is: foo, foo2, foo3, etc.
        String num = "";
        if (index != 0) {
            num = Integer.toString(index + 1);
        }
        return NANO_APP_FILENAME_BASE + num + NANO_APP_FILENAME_EXTENSION;
    }

    private void loadNanoApps() {
        TestNames[] testNames = getTestNames();
        int nanoAppCount = testNames.length;

        // This indicates an error in this specific test.
        Assert.assertFalse("Internal Error: Test provided empty getTestNames()",
                           nanoAppCount == 0);

        int index = 0;
        for (TestNames testName : testNames) {
            loadSingleNanoApp(testName, index);
            index++;
        }

        // We shouldn't ever get this assert.  It indicates an error in this
        // framework code, failing to load a nanoapp but not immediately
        // stopping the test in failure.
        Assert.assertEquals("Internal Error: Incorrect number of nanoapps loaded",
                            nanoAppCount, mNanoAppHandles.size());
    }

    private void launchTest() {
        byte[] data = new byte[0];
        for (TestNames testName : getTestNames()) {
            sendMessageToNanoApp(testName, testName.asInt(), data);
        }
    }

    @Override
    protected void loadAndStart() {
        loadNanoApps();
        launchTest();
    }

    @Override
    protected void unload() {
        for (Integer nanoAppHandle : mNanoAppHandles.keySet()) {
            String appName =
                    NANO_APP_NAME_BASE + mNanoAppHandles.get(nanoAppHandle).name();
            unloadNanoApp(appName, nanoAppHandle);
        }
    }

    /**
     * START HELPER METHODS DESIGNED FOR USE BY CHILDREN TEST CLASSES.
     */

    /**
     * Send message to a NanoApp/TestName
     *
     * This should be used only in rare cases where a MessageType is not
     * sufficient. For example, starting a NanoApp
     *
     * @param testName  The enum of the NanoApp.
     * @param messageType  The message type.
     * @param  data  The message data. This is allowed to be an empty array.
     */
    protected void sendMessageToNanoApp(TestNames testName,
                                        int messageType, byte[] data) {
        int result =
                getContextHubManager().sendMessage(getNanoAppHandle(testName), messageType, data);
        Assert.assertEquals("Failed to send message " + messageType
                + " to nanoapp test " + testName, 0, result);
    }

    /**
     * Expose the nanoapp handle of a TestName
     *
     * @param testName  The enum of the NanoApp.
     *
     * @return The Nanoapp handle
     */
    protected int getNanoAppHandle(TestNames testName) {
        for (Entry<Integer, TestNames> entry :
                 mNanoAppHandles.entrySet()) {
            if (testName.asInt() == entry.getValue().asInt()) {
                return entry.getKey();
            }
        }
        Assert.fail("Internal error: Could not find nanoapp handle for " + testName);
        // Never get here.
        return -1;
    }

    /**
     * Tests should use this method to send a message to the NanoApp.
     *
     * @param testName  The enum name of the NanoApp.  Most tests will only
     *     have a single testName, but some tests involve multiple nanoapps
     *     communicating, which is why we have this argument.
     * @param messageType  The message type.
     * @param data  The message data.  This is allowed to be an empty array.
     *
     * @return Nothing.  If we fail to send the message, this method will
     *     assert, failing the test and never returning.
     */
    protected void sendMessageToNanoApp(TestNames testName,
                                        MessageType messageType, byte[] data) {
        sendMessageToNanoApp(testName, messageType.asInt(), data);
    }


    /**
     * Load a single NanoApp
     *
     * @param testName  The enum name of the NanoApp.
     * @param index  The number of times the NanoApp has been loaded prior.
     */
    protected void loadSingleNanoApp(TestNames testName, int index) {
        String appName = NANO_APP_NAME_BASE + testName.name();
        Integer handle = loadNanoApp(appName, getNanoAppFilename(index));
        mNanoAppHandles.put(handle, testName);
    }


    /**
     * Unload a single NanoApp
     *
     * @param testName  The enum name of the NanoApp.
     */
    protected void unloadSingleNanoApp(TestNames testName) {
        Assert.assertTrue("Internal Error: Provided empty testName",
                testName != null);

        unloadNanoApp(NANO_APP_NAME_BASE + testName.name(),
                getNanoAppHandle(testName));
        mNanoAppHandles.remove(getNanoAppHandle(testName));
    }

    /**
     * Report a test-failing unexpected message.
     *
     * @param testName  The enum name of the NanoApp which received this
     *     unexpected message.
     * @param type  The 'type' of the message.
     * @param data  The message data.  This is allowed to be an empty array.
     *
     * @return Never.  This triggers a test-ending failure.
     */
    protected void unexpectedMessageFailure(TestNames testName,
                                            MessageType type, byte[] data) {
        // Put this back in Message form for better logging.
        ContextHubMessage message = new ContextHubMessage(type.asInt(), 0, data);
        Assert.fail("Unexpected message received in test " + testName
                + ": " + GtsContextHubManagerWrapper.debugDump(message));
    }

    /**
     * Complete a test as passing.
     *
     * For a test which expects to receive SUCCESS from the nanoapp, there's
     * no need to call this.  This method is intended for tests where the
     * "protocol" (see near the top of this file) has the Java Host code
     * evaluating messages and deciding whether or not the test passes.
     *
     * @return  This method does return, but no further work should be done
     *     by the caller.
     */
    @Override
    protected void pass() {
        super.pass();
    }


    /**
     * START ABSTRACT METHODS REQUIRED TO BE IMPLEMENTED BY CHILDREN TEST CLASSES.
     */

    /**
     * Method to handle "protocol" message.
     *
     * Note that methods with certain messageTypes (SUCCESS, FAILURE,
     * INTERNAL_FAILURE, and INVALID_MESSAGE_TYPE) are handled by
     * the infrastructure.  This method is invoked for other messageTypes.
     *
     * A robust test should call unexpectedMessageFailure() for unexpected
     * messages, instead of ignoring unexpected messages.
     */
    protected abstract void handleMessageFromNanoApp(
            TestNames testName, MessageType type, byte[] data);

    /**
     * Get the enum names of the nanoapp(s) used by this test.
     *
     * Most tests only use one nanoapp instance, so this method will just
     * return an array with a single TestNames.  But tests which involve
     * communication between multiple nanoapps will have multiple items
     * in this array.
     *
     * @return The array of TestNames.
     */
    protected abstract TestNames[] getTestNames();

    /**
     * Get the number of seconds after which a test should be considered failed.
     *
     * This is the amount of time after the nanoapps have loaded, and before
     * they have been unloaded.  Thus, this just covers the time spent running
     * the actual test.
     *
     * This is intended to keep tests from running "forever".  This is not
     * intended to be used to perform specific timing checks for a test (note
     * that our precision here is just in seconds).  Tests which require/expect
     * precise timings should internally check for that, using this mechanism
     * only as a fallback for things falling apart overall.
     *
     * Error on the side of making this value too large.  If this value is
     * too small, we risk failures due to flakiness (a test that would have
     * passed if it had been given longer).  If this value is too large, we
     * end up waiting longer in a failure case where expected communications
     * which didn't come across.  We'd much rather wait a little longer to
     * report an overall failure than risk a flaky test.
     *
     * @return The number of seconds after which to give up on a test as
     *     not responding properly.
     */
    @Override
    protected abstract long getTestTimeoutSeconds();
}
