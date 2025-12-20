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
import android.hardware.contexthub.HubMessage;
import android.hardware.contexthub.HubServiceInfo;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.HubInfo;
import android.hardware.location.NanoAppBinary;
import android.util.Log;

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

    private static final int TIMEOUT_SECONDS = 5;

    @NonNull private final ContextHubManager mContextHubManager;
    @Nullable private final ContextHubInfo mContextHubInfo;
    @Nullable private final NanoAppBinary mNanoAppBinary;

    static class TestDataFlowCallback implements DataFlowCallback {
        private final ArrayBlockingQueue<DataFlowData> mData = new ArrayBlockingQueue<>(1);
        private final ArrayBlockingQueue<DataFlowSink> mSinkQueue = new ArrayBlockingQueue<>(1);
        private HubEndpointInfo mSourceInfo = null;

        @Override
        public void onReceivedDataFlowSink(
                DataFlowSink sink,
                HubEndpointInfo source,
                HubEndpointSession session,
                HubMessage msg) {
            Log.i(TAG, "onReceivedDataFlowSink");
            if (mSourceInfo != null && mSourceInfo.getIdentifier().equals(source.getIdentifier())) {
                mSinkQueue.add(sink);
            } else {
                Log.w(
                        TAG,
                        "Received data flow sink for unexpected source: "
                                + source
                                + " expected: "
                                + mSourceInfo);
            }
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
                DataFlowSink sink = mSinkQueue.poll(TIMEOUT_SECONDS, TimeUnit.SECONDS);
                assertThat(sink).isNotNull();
                return sink;
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

        void setSourceInfo(HubEndpointInfo sourceInfo) {
            mSourceInfo = sourceInfo;
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
        // TODO(b/468415989): Add nanoapp loading when ready
    }

    /** Deinitialization code that should be called in e.g. @After. */
    public void deinit() {
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
     */
    public void testDataFlow() {
        // Register the endpoint
        TestDataFlowCallback callback = new TestDataFlowCallback();

        // For this test, we use a non-default executor, since the TestDataFlowCallback
        // will perform a blocking read on callback execution, which can block the dataflow Looper
        // if they are both running on the same thread.
        ScheduledThreadPoolExecutor executor =
                new ScheduledThreadPoolExecutor(/* corePoolSize= */ 1);
        HubEndpoint endpoint =
                registerDefaultEndpoint(
                        /* callback= */ null,
                        /* messageCallback= */ null,
                        callback,
                        /* executor= */ executor,
                        Collections.emptyList());
        assertThat(endpoint).isNotNull();

        List<HubDiscoveryInfo> infoList = new ArrayList<>();
        checkApiSupport(
                (manager) -> infoList.addAll(manager.findEndpoints(ECHO_SERVICE_DESCRIPTOR)));
        for (HubDiscoveryInfo info : infoList) {
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
            assertWithMessage("Hub 0x" + id.getHub() + " not found in getHubs()")
                    .that(dataFlowSupported)
                    .isPresent();
            if (!dataFlowSupported.get()) {
                Log.d(TAG, "Data flow not supported on hub 0x" + id.getHub() + ", skipping");
                continue;
            }

            HubEndpointInfo offloadEndpointInfo = info.getHubEndpointInfo();
            assertThat(offloadEndpointInfo).isNotNull();
            callback.setSourceInfo(offloadEndpointInfo);

            // Create the data flow and add the offload endpoint as a sink
            DataFlowDataConfig dataConfig =
                    DataFlowDataConfig.createFixedSize(
                            DATA_FLOW_ELEMENT_SIZE, DATA_FLOW_ELEMENT_ALIGNMENT);
            DataFlowSource source =
                    endpoint.createDataFlowSource(
                            Collections.singleton(offloadEndpointInfo.getIdentifier().getHub()),
                            dataConfig,
                            MIN_CAPACITY_BYTES,
                            MAX_CAPACITY_BYTES);
            assertThat(source).isNotNull();
            source.shareDataFlow(
                    offloadEndpointInfo,
                    DataFlowNewDataAlertPolicy.createStreamingPolicy(),
                    /* canOverwrite= */ false);

            // Wait for the sink to be created for us
            DataFlowSink sink = callback.waitForNewSink(offloadEndpointInfo);
            assertThat(sink).isNotNull();

            // Send data to the sink and confirm it echos back to us
            ByteBuffer dataBuffer = ByteBuffer.allocate(ECHO_DATA_SIZE_BYTES);
            for (int i = 0; i < ECHO_DATA_SIZE_BYTES; ++i) {
                dataBuffer.put((byte) i);
            }
            dataBuffer.rewind();
            DataFlowData data = new DataFlowData(dataBuffer, dataConfig);

            for (int i = 0; i < ECHO_DATA_SIZE_BYTES; ++i) {
                source.push(data, /* canOverwrite= */ false);

                DataFlowData echo = callback.waitForNewData(sink);
                assertThat(echo).isNotNull();
                List<ByteBuffer> buffers = echo.getBuffers();
                assertThat(buffers).isNotEmpty();
                ByteBuffer echoBuffer = buffers.get(0);
                assertThat(echoBuffer).isNotNull();
                assertThat(echoBuffer.capacity()).isEqualTo(dataBuffer.capacity());
                assertThat(echoBuffer).isEqualTo(dataBuffer);
            }
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
