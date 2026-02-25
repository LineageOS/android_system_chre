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
package com.google.android.chre.test.endpoint;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.content.Context;
import android.hardware.contexthub.DataFlowCallback;
import android.hardware.contexthub.DataFlowData;
import android.hardware.contexthub.DataFlowDataConfig;
import android.hardware.contexthub.DataFlowNewDataAlertPolicy;
import android.hardware.contexthub.DataFlowSink;
import android.hardware.contexthub.DataFlowSource;
import android.hardware.contexthub.HubDiscoveryInfo;
import android.hardware.contexthub.HubEndpoint;
import android.hardware.contexthub.HubEndpointInfo;
import android.hardware.contexthub.HubEndpointInfo.HubEndpointIdentifier;
import android.hardware.contexthub.HubEndpointLifecycleCallback;
import android.hardware.contexthub.HubEndpointMessageCallback;
import android.hardware.contexthub.HubEndpointSession;
import android.hardware.contexthub.HubEndpointSessionResult;
import android.hardware.contexthub.HubMessage;
import android.hardware.contexthub.HubServiceInfo;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.ContextHubTransaction;
import android.hardware.location.HubInfo;
import android.hardware.location.NanoAppBinary;
import android.util.Log;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Assume;

import java.nio.ByteBuffer;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.function.Consumer;

public class ContextHubEndpointDataFlowExecutor {
    private static final String TAG = "ContextHubEndpointDataFlowExecutor";

    /** The service descriptor for an echo service. */
    private static final String ECHO_SERVICE_DESCRIPTOR =
            "android.hardware.contexthub.test.EchoService";

    private static final int MIN_CAPACITY_BYTES = 1024;
    private static final int MAX_CAPACITY_BYTES = 16 * 1024;

    private static final int DATA_FLOW_ELEMENT_SIZE = 1;
    private static final int DATA_FLOW_ELEMENT_ALIGNMENT = 1;

    private static final int ECHO_DATA_SIZE_BYTES = 10;
    private static final int ECHO_DATA_NUM_ITERATIONS = 10;

    private static final int TIMEOUT_SECONDS = 5;

    @NonNull private final ContextHubManager mContextHubManager;
    @Nullable private final ContextHubInfo mContextHubInfo;
    @Nullable private final NanoAppBinary mNanoAppBinary;

    private HubEndpoint mHubEndpoint = null;
    private final TestLifecycleCallback mLifecycleCallback = new TestLifecycleCallback();
    private final TestDataFlowCallback mDataFlowCallback = new TestDataFlowCallback();

    static class TestLifecycleCallback implements HubEndpointLifecycleCallback {
        private static final int TIMEOUT_SESSION_OPEN_SECONDS = 5;

        TestLifecycleCallback() {
            this(/* acceptSession= */ false);
        }

        TestLifecycleCallback(boolean acceptSession) {
            mAcceptSession = acceptSession;
        }

        @Override
        public HubEndpointSessionResult onSessionOpenRequest(
                HubEndpointInfo requester, String serviceDescriptor) {
            Log.d(TAG, "onSessionOpenRequest");
            HubEndpointSessionResult result =
                    mAcceptSession
                            ? HubEndpointSessionResult.accept()
                            : HubEndpointSessionResult.reject("Unexpected request");
            mSessionRequestQueue.add(result);
            return result;
        }

        @Override
        public void onSessionOpened(HubEndpointSession session) {
            Log.d(TAG, "onSessionOpened: session=" + session);
            mSessionQueue.add(session);
        }

        @Override
        public void onSessionClosed(HubEndpointSession session, int reason) {
            Log.d(TAG, "onSessionClosed: session=" + session);
            mSessionCloseQueue.add(Pair.create(session, reason));
        }

        public HubEndpointSession waitForEndpointSession() {
            try {
                return mSessionQueue.poll(TIMEOUT_SESSION_OPEN_SECONDS, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Assert.fail("InterruptedException in waitForEndpointSession: " + e.getMessage());
                return null;
            }
        }

        public HubEndpointSessionResult waitForOpenSessionRequest() {
            try {
                return mSessionRequestQueue.poll(TIMEOUT_SESSION_OPEN_SECONDS, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Assert.fail("InterruptedException in waitForOpenSessionRequest: " + e.getMessage());
                return null;
            }
        }

        public Pair<HubEndpointSession, Integer> waitForCloseSession() {
            try {
                return mSessionCloseQueue.poll(TIMEOUT_SESSION_OPEN_SECONDS, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Assert.fail("InterruptedException in waitForCloseSession: " + e.getMessage());
                return null;
            }
        }

        /** If true, accepts incoming sessions */
        private final boolean mAcceptSession;

        private final BlockingQueue<HubEndpointSession> mSessionQueue = new ArrayBlockingQueue<>(1);
        private final BlockingQueue<HubEndpointSessionResult> mSessionRequestQueue =
                new ArrayBlockingQueue<>(1);
        private final BlockingQueue<Pair<HubEndpointSession, Integer>> mSessionCloseQueue =
                new ArrayBlockingQueue<>(1);
    }

    static class TestDataFlowCallback implements DataFlowCallback {
        private final ArrayBlockingQueue<DataFlowData> mData = new ArrayBlockingQueue<>(1);
        private final ArrayBlockingQueue<SinkAndSourceInfo> mSinkQueue =
                new ArrayBlockingQueue<>(1);
        private HubEndpointSession mSession = null;
        private HubMessage mMessage = null;

        static class SinkAndSourceInfo {
            public DataFlowSink mSink;
            public HubEndpointInfo mSourceInfo;
            public HubEndpointSession mSession;
            public HubMessage mMessage;

            SinkAndSourceInfo(
                    DataFlowSink sink,
                    HubEndpointInfo sourceInfo,
                    HubEndpointSession session,
                    HubMessage message) {
                mSession = session;
                mMessage = message;
                mSink = sink;
                mSourceInfo = sourceInfo;
            }
        }

        @Override
        public void onReceivedDataFlowSink(
                DataFlowSink sink,
                HubEndpointInfo source,
                HubEndpointSession session,
                HubMessage msg) {
            Log.i(TAG, "onReceivedDataFlowSink: Started: sink=" + sink + " source=" + source
                    + " session=" + session + " msg=" + msg);
            mSinkQueue.add(new SinkAndSourceInfo(sink, source, session, msg));
            Log.i(TAG, "onReceivedDataFlowSink: Finished");
        }

        @Override
        public void onDataFlowSourceEvent(DataFlowSource source, int event, SourceEventData data) {
            Log.i(TAG, "onDataFlowSourceEvent");
        }

        @Override
        public void onDataFlowSinkEvent(DataFlowSink sink, int event) {
            Log.i(TAG, "onDataFlowSinkEvent");

            if (event == DataFlowCallback.SINK_EVENT_READABLE) {
                try {
                    DataFlowData dataFlowData =
                            sink.awaitData(
                                    ECHO_DATA_SIZE_BYTES, Duration.ofSeconds(TIMEOUT_SECONDS));
                    assertThat(dataFlowData).isNotNull();
                    mData.add(dataFlowData);
                } catch (TimeoutException e) {
                    Assert.fail("Timed out waiting for data flow sink" + e.getMessage());
                }
            } else if (event == DataFlowCallback.SINK_EVENT_STOPPED) {
                Assert.fail("Sink stopped unexpectedly");
            } else {
                Assert.fail("Unexpected sink event: " + event);
            }
        }

        DataFlowSink waitForNewSink(HubEndpointInfo sourceInfo) {
            try {
                SinkAndSourceInfo sinkAndSourceInfo =
                        mSinkQueue.poll(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                assertThat(sinkAndSourceInfo).isNotNull();
                assertThat(sinkAndSourceInfo.mSink).isNotNull();
                assertThat(sinkAndSourceInfo.mSourceInfo).isNotNull();
                assertThat(sinkAndSourceInfo.mSourceInfo).isEqualTo(sourceInfo);
                assertThat(sinkAndSourceInfo.mSession == null)
                        .isEqualTo(sinkAndSourceInfo.mMessage == null);
                if (sinkAndSourceInfo.mSession != null) {
                    assertThat(sinkAndSourceInfo.mSession).isEqualTo(mSession);
                    assertThat(sinkAndSourceInfo.mMessage.getMessageBody())
                            .isEqualTo(mMessage.getMessageBody());
                }
                return sinkAndSourceInfo.mSink;
            } catch (InterruptedException e) {
                Assert.fail("InterruptedException in waitForNewSink: " + e.getMessage());
                return null;
            }
        }

        DataFlowData waitForNewData(DataFlowSink sink) {
            try {
                DataFlowData data = mData.poll(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                assertThat(data).isNotNull();
                return data;
            } catch (InterruptedException e) {
                Assert.fail("InterruptedException in waitForNewData: " + e.getMessage());
                return null;
            }
        }

        void setSession(HubEndpointSession session) {
            mSession = session;
        }

        void setMessage(HubMessage message) {
            mMessage = message;
        }

        void reset() {
            mSession = null;
            mMessage = null;
            mData.clear();
            mSinkQueue.clear();
        }
    }

    public ContextHubEndpointDataFlowExecutor(ContextHubManager manager) {
        this(manager, /* info= */ null, /* nanoAppBinary= */ null);
    }

    public ContextHubEndpointDataFlowExecutor(
            ContextHubManager manager, ContextHubInfo info, NanoAppBinary nanoAppBinary) {
        mContextHubManager = manager;
        mContextHubInfo = info;
        // TODO(b/468415989): Update to use nanoapp when available
        mNanoAppBinary = nanoAppBinary;
    }

    /** Initialization code that should be called in e.g. @Before. */
    public void init() {
        if (mHubEndpoint == null) {
            // For this test, we use a non-default executor, since the TestDataFlowCallback
            // will perform a blocking read on callback execution, which can block the dataflow
            // Looper if they are both running on the same thread.
            ScheduledThreadPoolExecutor executor =
                    new ScheduledThreadPoolExecutor(/* corePoolSize= */ 1);

            HubEndpoint endpoint =
                    registerDefaultEndpoint(
                            mLifecycleCallback,
                            /* messageCallback= */ null,
                            mDataFlowCallback,
                            /* executor= */ executor,
                            Collections.emptyList());
            assertThat(endpoint).isNotNull();
            mHubEndpoint = endpoint;
        }

        // TODO(b/468415989): Add nanoapp loading when ready
    }

    /** Deinitialization code that should be called in e.g. @After. */
    public void deinit() {
        if (mHubEndpoint != null) {
            checkApiSupport((manager) -> manager.unregisterEndpoint(mHubEndpoint));
            mHubEndpoint = null;
        }

        // TODO(b/468415989): Add nanoapp unloading when ready
    }

    /**
     * Tests the data flow functionality by creating a data flow source, sharing it with an offload
     * endpoint, and then pushing data to the sink and verifying that it is echoed back. This test
     * assumes the existence of an offload endpoint that supports data flows and echoes data.
     *
     * <p>The sequence of the test is as follows:
     *
     * <ol>
     *   <li>The test attempts to find the Echo service in the system
     *   <li>If the echo service is found, and if the hub hosting this service supports data flows,
     *       the data flow test is started.
     *   <li>The test starts by creating a fixed-size data flow source, and shares the data flow
     *       with the offload endpoint hosting the echo service.
     *   <li>The test wait a new DataSourceSink to be created as a result of the above step, through
     *       the DataFlowCallback.
     *   <li>The test pushes binary data to the source, and waits for equivalent data to arrive at
     *       the sink.
     * </ol>
     *
     * @param overSession Whether to send a message to the offload endpoint when sharing the data
     *     flow.
     * @param useFixedSizeDataFlow Whether to use a fixed-size data flow source. If false, a
     *     variable-size data flow source will be used.
     */
    public void testDataFlow(boolean overSession, boolean useFixedSizeDataFlow) {
        List<HubDiscoveryInfo> infoList = new ArrayList<>();
        checkApiSupport(
                (manager) -> infoList.addAll(manager.findEndpoints(ECHO_SERVICE_DESCRIPTOR)));
        for (HubDiscoveryInfo info : infoList) {
            doTestDataFlow(
                    info,
                    overSession,
                    useFixedSizeDataFlow);
        }
    }

    private void doTestDataFlow(
            HubDiscoveryInfo info,
            boolean overSession,
            boolean useFixedSizeDataFlow) {
        mDataFlowCallback.reset();
        printHubDiscoveryInfo(info);

        // Only run this test if we find an Echo service hosted on a hub that supports
        // data flows.
        Optional<Boolean> dataFlowSupported = Optional.empty();
        HubEndpointIdentifier id = info.getHubEndpointInfo().getIdentifier();
        List<HubInfo> hubs = new ArrayList<>();
        checkApiSupport((manager) -> hubs.addAll(manager.getHubs()));
        for (HubInfo hub : hubs) {
            if (hub.getId() == id.getHub()) {
                dataFlowSupported = Optional.of(hub.areDataFlowsSupported());
                break;
            }
        }
        assertWithMessage("Hub 0x" + Long.toHexString(id.getHub()) + " not found in getHubs()")
                .that(dataFlowSupported)
                .isPresent();
        if (!dataFlowSupported.get()) {
            Log.d(TAG, "Data flow not supported on hub 0x" + Long.toHexString(id.getHub())
                    + ", skipping");
            return;
        }

        HubEndpointInfo offloadEndpointInfo = info.getHubEndpointInfo();
        assertThat(offloadEndpointInfo).isNotNull();

        HubEndpointSession session = null;
        if (overSession) {
            checkApiSupport(
                (manager) -> manager.openSession(mHubEndpoint, offloadEndpointInfo));
            session = mLifecycleCallback.waitForEndpointSession();
            mDataFlowCallback.setSession(session);
        }

        // Create the data flow and add the offload endpoint as a sink
        DataFlowDataConfig dataConfig =
                useFixedSizeDataFlow
                        ? DataFlowDataConfig.createFixedSize(
                                DATA_FLOW_ELEMENT_SIZE, DATA_FLOW_ELEMENT_ALIGNMENT)
                        : DataFlowDataConfig.createVariableSizeAligned(
                                DATA_FLOW_ELEMENT_ALIGNMENT);
        DataFlowSource source =
                mHubEndpoint.createDataFlowSource(
                        Collections.singleton(offloadEndpointInfo.getIdentifier().getHub()),
                        dataConfig,
                        MIN_CAPACITY_BYTES,
                        MAX_CAPACITY_BYTES);
        assertThat(source).isNotNull();

        if (overSession) {
            HubMessage message = new HubMessage.Builder(1234, new byte[] {1, 2, 3, 4, 5})
                    .setResponseRequired(true)
                    .build();
            mDataFlowCallback.setMessage(message);

            ContextHubTransaction<Void> txn = source.shareDataFlowOverSession(
                    offloadEndpointInfo,
                    DataFlowNewDataAlertPolicy.createStreamingPolicy(),
                    /* canOverwrite= */ false, session, message);
            Assert.assertNotNull(txn);

            try {
                ContextHubTransaction.Response<Void> txnResponse =
                        txn.waitForResponse(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                assertThat(txnResponse).isNotNull();
                assertThat(txnResponse.getResult()).isEqualTo(ContextHubTransaction.RESULT_SUCCESS);
            } catch (InterruptedException | TimeoutException e) {
                Assert.fail("InterruptedException or TimeoutException in waitForResponse: "
                            + e.getMessage());
                return;
            }
        } else {
            source.shareDataFlow(
                    offloadEndpointInfo,
                    DataFlowNewDataAlertPolicy.createStreamingPolicy(),
                    /* canOverwrite= */ false);
        }

        // Wait for the sink to be created for us
        DataFlowSink sink = mDataFlowCallback.waitForNewSink(offloadEndpointInfo);
        assertThat(sink).isNotNull();

        // Send data to the sink and confirm it echos back to us
        ByteBuffer dataBuffer = ByteBuffer.allocate(ECHO_DATA_SIZE_BYTES);
        for (int i = 0; i < ECHO_DATA_SIZE_BYTES; ++i) {
            dataBuffer.put((byte) i);
        }
        dataBuffer.rewind();
        DataFlowData data = new DataFlowData(dataBuffer, dataConfig);

        DataFlowData data2 = null;
        ByteBuffer dataBuffer2 = null;
        if (!useFixedSizeDataFlow) {
            dataBuffer2 = ByteBuffer.allocate(ECHO_DATA_SIZE_BYTES * 2);
            for (int i = 0; i < ECHO_DATA_SIZE_BYTES * 2; ++i) {
                dataBuffer2.put((byte) i);
            }
            dataBuffer2.rewind();
            data2 = new DataFlowData(dataBuffer2, dataConfig);
        }

        for (int i = 0; i < ECHO_DATA_NUM_ITERATIONS; ++i) {
            Log.i(TAG, "Pushing data to source: " + i);
            if (useFixedSizeDataFlow) {
                source.push(data, /* canOverwrite= */ false);
            } else {
                source.push(i % 2 == 0 ? data : data2, /* canOverwrite= */ false);
            }

            DataFlowData echo = mDataFlowCallback.waitForNewData(sink);
            assertThat(echo).isNotNull();
            List<ByteBuffer> buffers = echo.getBuffers();
            assertThat(buffers).isNotEmpty();
            ByteBuffer echoBuffer = buffers.get(0);
            assertThat(echoBuffer).isNotNull();
            if (useFixedSizeDataFlow || i % 2 == 0) {
                assertThat(echoBuffer.capacity()).isEqualTo(dataBuffer.capacity());
                assertThat(echoBuffer.capacity()).isEqualTo(ECHO_DATA_SIZE_BYTES);
                for (int j = 0; j < ECHO_DATA_SIZE_BYTES; ++j) {
                    assertThat(echoBuffer.get(i)).isEqualTo(dataBuffer.get(i));
                }
            } else {
                assertThat(echoBuffer.capacity()).isEqualTo(dataBuffer2.capacity());
                assertThat(echoBuffer.capacity()).isEqualTo(ECHO_DATA_SIZE_BYTES * 2);
                for (int j = 0; j < ECHO_DATA_SIZE_BYTES * 2; ++j) {
                    assertThat(echoBuffer.get(i)).isEqualTo(dataBuffer2.get(i));
                }
            }
        }

        if (overSession) {
            assertThat(session).isNotNull();
            session.close();
            mLifecycleCallback.waitForCloseSession();
        }
    }

    private void checkApiSupport(Consumer<ContextHubManager> consumer) {
        try {
            consumer.accept(mContextHubManager);
        } catch (UnsupportedOperationException e) {
            // Forced assumption
            Assume.assumeTrue("Skipping endpoint test on unsupported device", false);
        }
    }

    private void printHubDiscoveryInfo(HubDiscoveryInfo info) {
        Log.d(TAG, "Found hub: ");
        Log.d(TAG, " - Endpoint info: " + info.getHubEndpointInfo());
        Log.d(TAG, " - Service info: " + info.getHubServiceInfo());
    }

    private HubEndpoint registerDefaultEndpoint(
            HubEndpointLifecycleCallback callback,
            HubEndpointMessageCallback messageCallback,
            DataFlowCallback dataCallback,
            Executor executor,
            Collection<HubServiceInfo> serviceList) {
        Assert.assertNotNull(serviceList);
        Context context = InstrumentationRegistry.getTargetContext();
        HubEndpoint.Builder builder = new HubEndpoint.Builder(context);
        builder.setTag(TAG);
        if (callback != null) {
            if (executor != null) {
                builder.setLifecycleCallback(executor, callback);
            } else {
                builder.setLifecycleCallback(callback);
            }
        }
        if (messageCallback != null) {
            if (executor != null) {
                builder.setMessageCallback(executor, messageCallback);
            } else {
                builder.setMessageCallback(messageCallback);
            }
        }
        if (dataCallback != null) {
            if (executor != null) {
                builder.setDataFlowCallback(executor, dataCallback);
            } else {
                builder.setDataFlowCallback(context.getMainExecutor(), dataCallback);
            }
        }
        builder.setServiceInfoCollection(serviceList);
        HubEndpoint endpoint = builder.build();
        assertThat(endpoint).isNotNull();
        assertThat(endpoint.getTag()).isEqualTo(TAG);
        assertThat(endpoint.getLifecycleCallback()).isEqualTo(callback);
        assertThat(endpoint.getMessageCallback()).isEqualTo(messageCallback);
        assertThat(endpoint.getDataFlowCallback()).isEqualTo(dataCallback);
        assertThat(endpoint.getServiceInfoCollection().size()).isEqualTo(serviceList.size());

        checkApiSupport((manager) -> manager.registerEndpoint(endpoint));
        return endpoint;
    }
}
