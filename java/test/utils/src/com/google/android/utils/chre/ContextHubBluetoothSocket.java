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

package com.google.android.utils.chre;

import android.bluetooth.BluetoothSocket;
import android.util.Log;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;


/**
 * Creates the Socket Thread with a connected socket and uses it to exchange data with a
 * remote device.
 */
public class ContextHubBluetoothSocket {
    private static final String TAG = "ContextHubBluetoothSocket";

    /** Call back timeout. */
    protected static final int CALLBACK_TIMEOUT_SEC = 5;

    protected BluetoothSocket mSocket;

    private SocketThread mSocketThread;

    private final BlockingQueue<byte[]> mReceivedPackets = new ArrayBlockingQueue<>(1);

    /** Starts a new Socket session. */
    public void startSocket() {
        mSocketThread = new SocketThread();
        mSocketThread.start();
    }

    /** Stops the Socket and closes the socket. */
    public boolean stopSocket() {
        boolean success = true;
        try {
            mSocket.close();
            if (mSocketThread != null) {
                mSocketThread = null;
            }
        } catch (IOException e) {
            Log.e(TAG, "socket create() failed", e);
            success = false;
        }
        return success;
    }

    /** Stop the SocketThread. */
    public void stop() {
        if (mSocketThread != null) {
            try {
                mSocket.close();
            } catch (IOException e) {
                Log.e(TAG, "close() of socket failed", e);
            }
            mSocketThread = null;
        }
    }

    /**
     * Writes to the socket.
     *
     * @param buffer The bytes to write
     * @return true if write was successful.
     */
    public boolean write(byte[] buffer) {
        return mSocketThread.write(buffer);
    }

    /**
     * Verifies that a packet was received.
     *
     * @param packet The packet to verify.
     * @return true if the packet was received.
     */
    public boolean verifyPacket(byte[] packet) throws InterruptedException {
        byte[] received = mReceivedPackets.poll(CALLBACK_TIMEOUT_SEC, TimeUnit.SECONDS);
        if (received == null) {
            return false;
        }
        return java.util.Arrays.equals(packet, received);
    }

    /**
     * This thread runs during a connection with a remote device. It handles all incoming and
     * outgoing transmissions.
     */
    private class SocketThread extends Thread {
        private final InputStream mInStream;
        private final OutputStream mOutStream;

        SocketThread() {
            Log.d(TAG, "create SocketThread");
            InputStream tmpIn = null;
            OutputStream tmpOut = null;

            try {
                tmpIn = mSocket.getInputStream();
                tmpOut = mSocket.getOutputStream();
            } catch (IOException e) {
                Log.e(TAG, "input/output stream(s) not created", e);
            }
            mInStream = tmpIn;
            mOutStream = tmpOut;
        }

        /**
         * This thread runs while listening for incoming data. It runs until the connection is lost.
         */
        public void run() {
            Log.i(TAG, "BEGIN mSocketThread");

            // Keep listening to the InputStream while connected
            while (true) {
                try {
                    byte[] buffer = new byte[2048];
                    int bytesRead = mInStream.read(buffer, 0, buffer.length);
                    if (bytesRead == -1) {
                        Log.i(TAG, "Socket reached EOF");
                        return;
                    }
                    byte[] received = new byte[bytesRead];
                    System.arraycopy(buffer, 0, received, 0, bytesRead);
                    Log.i(TAG, "Read " + bytesRead + " bytes");
                    mReceivedPackets.add(received);
                } catch (IOException e) {
                    Log.e(TAG, "disconnected", e);
                    break;
                }
            }

            Log.i(TAG, "END mSocketThread");
        }

        /**
         * Write to the connected OutStream.
         *
         * @param buffer The bytes to write
         */
        private synchronized boolean write(byte[] buffer) {
            boolean success = true;
            try {
                mOutStream.write(buffer, 0, buffer.length);
                mOutStream.flush();
            } catch (IOException e) {
                success = false;
                Log.e(TAG, "Exception during write", e);
            }
            return success;
        }
    }
}
