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

package com.google.android.chre.test.chqts.multidevice;

import android.hardware.location.NanoAppBinary;
import android.util.Log;

import com.google.android.chre.test.chqts.ContextHubBleTestExecutor;
import com.google.android.utils.chre.ChreApiTestUtil;
import com.google.android.utils.chre.ContextHubBluetoothSocketClient;
import com.google.android.utils.chre.ContextHubBluetoothSocketServer;
import com.google.protobuf.ByteString;

import java.util.List;

import dev.chre.rpc.proto.ChreApiTest;

public class ContextHubMultiDeviceBluetoothSocketTestExecutor extends ContextHubBleTestExecutor {
    private static final String TAG = "ContextHubMultiDeviceBluetoothSocketTestExecutor";

    private long mNanoappId;
    private ContextHubBluetoothSocketClient mSocketClient;
    private ContextHubBluetoothSocketServer mSocketServer;
    private long mSocketId;

    public ContextHubMultiDeviceBluetoothSocketTestExecutor(NanoAppBinary nanoapp) {
        super(nanoapp);
        Log.i(TAG, "Constructing ContextHubMultiDeviceBluetoothSocketTestExecutor");
        mNanoappId = nanoapp.getNanoAppId();
    }

    /**
     * Starts a socket server and waits for a client to connect.
     *
     * @param offload Whether the socket should be offloaded.
     * @return true if socket server was connected successfully.
     */
    public boolean startSocketServer(boolean offload, long hubId) {
        mSocketServer =
                new ContextHubBluetoothSocketServer(mContext, offload, mNanoappId, hubId);
        return mSocketServer.startServer();
    }

    /**
     * Starts a socket client and connects to the server socket.
     *
     * <p>NOTE: A second device must call startSocketServer for this call to succeed.
     *
     * @param offload Whether the socket should be offloaded.
     * @return true if socket client connected successfully.
     */
    public boolean startSocketClient(boolean offload, long hubId) {
        mSocketClient =
                new ContextHubBluetoothSocketClient(mContext, offload, mNanoappId, hubId);
        return mSocketClient.startClient();
    }

    /**
     * Verifies the nanoapp is notified when a BT socket is opened.
     *
     * @return true if nanoapp was notified
     * @throws Exception if unable to send RPC message
     */
    public boolean verifyNanoappNotifiedWhenSocketOpened() throws Exception {
        ChreApiTestUtil util = new ChreApiTestUtil();
        List<ChreApiTest.ChreBleSocketConnectionEvent> response =
                util.callServerStreamingRpcMethodSync(
                        getRpcClient(), "chre.rpc.ChreApiTestService.ChreBleSocketOpenedSync");
        if (response == null || response.size() != 1) {
            return false;
        }
        ChreApiTest.ChreBleSocketConnectionEvent event = response.get(0);
        mSocketId = event.getSocketId();
        return event.getStatus();
    }

    /**
     * Sends a socket packet from the nanoapp to the remote device and verifies that the send was
     * successful.
     *
     * @param packet The packet to send.
     * @return true if nanoapp sent the socket packet.
     * @throws Exception if unable to send RPC message.
     */
    public int sendSocketPacketFromChre(String packet) throws Exception {
        ChreApiTestUtil util = new ChreApiTestUtil();
        ChreApiTest.ChreBleSocketPacket request =
                ChreApiTest.ChreBleSocketPacket.newBuilder()
                        .setSocketId(mSocketId)
                        .setData(ByteString.copyFrom(packet.getBytes()))
                        .build();
        ChreApiTest.ChreBleSocketSendStatus response =
                util.callUnaryRpcMethodSync(
                        getRpcClient(),
                        "chre.rpc.ChreApiTestService.ChreBleSocketSend", request);
        return response.getStatus();
    }

    /**
     * Verifies that CHRE received a socket packet.
     *
     * @param packet The packet to verify.
     * @return true if CHRE received the packet.
     * @throws Exception if unable to send RPC message.
     */
    public boolean verifyChreReceivedSocketPacket(String packet) throws Exception {
        ChreApiTestUtil util = new ChreApiTestUtil();
        List<ChreApiTest.ChreBleSocketPacketEvent> response =
                util.callServerStreamingRpcMethodSync(
                        getRpcClient(),
                        "chre.rpc.ChreApiTestService.ChreBleSocketReceiveSync");
        if (response == null) {
            return false;
        }
        if (response.size() != 1) {
            return false;
        }
        ChreApiTest.ChreBleSocketPacketEvent event = response.get(0);
        if (!event.getStatus()) {
            return false;
        }
        if (!event.getPacket().getData().equals(ByteString.copyFrom(packet.getBytes()))) {
            return false;
        }
        return true;
    }

    /**
     * Sends a socket to the remote device from the host and verifies that the send was successful.
     *
     * @param packet The packet to send.
     * @return true if the packet was sent successfully.
     */
    public boolean sendSocketPacketFromHost(String packet) {
        if (mSocketClient != null) {
            return mSocketClient.write(packet.getBytes());
        }
        if (mSocketServer != null) {
            return mSocketServer.write(packet.getBytes());
        }
        return false;
    }

    /**
     * Verifies that the host received a socket packet.
     *
     * @param packet The packet to verify.
     * @return true if the host received the packet.
     * @throws InterruptedException if interrupted.
     */
    public boolean verifyHostReceivedSocketPacket(String packet) throws InterruptedException {
        if (mSocketClient != null) {
            return mSocketClient.verifyPacket(packet.getBytes());
        }
        if (mSocketServer != null) {
            return mSocketServer.verifyPacket(packet.getBytes());
        }
        return false;
    }

    /**
     * Close a BT socket if one exists.
     *
     * @return true if BT socket was closed successfully.
     */
    public boolean closeSocket() {
        if (mSocketClient != null) {
            return mSocketClient.stopSocket();
        }
        if (mSocketServer != null) {
            return mSocketServer.stopSocket();
        }
        return false;
    }

    /**
     * Verifies the nanoapp is notified when a BT socket is closed.
     *
     * @return true if nanoapp was notified
     * @throws Exception if unable to send RPC message
     */
    public boolean verifyNanoappNotifiedWhenSocketClosed() throws Exception {
        ChreApiTestUtil util = new ChreApiTestUtil();
        List<ChreApiTest.ChreBleSocketDisconnectionEvent> response =
                util.callServerStreamingRpcMethodSync(
                        getRpcClient(), "chre.rpc.ChreApiTestService.ChreBleSocketClosedSync");
        if (response == null || response.size() != 1) {
            return false;
        }
        ChreApiTest.ChreBleSocketDisconnectionEvent event = response.get(0);
        return event.getStatus() && event.getSocketId() == mSocketId;
    }
}
