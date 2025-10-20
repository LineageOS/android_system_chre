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

import static com.google.common.truth.Truth.assertThat;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.bluetooth.BluetoothSocketSettings;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.content.Context;
import android.os.ParcelUuid;
import android.util.Log;

import java.io.IOException;
import java.util.UUID;

/**
 * This class is used to connect to the Socket Client and exchange data with it.
 */
public class ContextHubBluetoothSocketServer extends ContextHubBluetoothSocket{
    private static final String TAG = "ContextHubBluetoothSocketServer";

    /** UUID of the advertisement */
    public static final UUID ADV_SOCKET_UUID =
            UUID.fromString("00004000-0000-1000-8000-00805f9b34fb");

    /** UUID of the GATT Read Characteristics for LE_PSM value. */
    public static final UUID LE_PSM_CHARACTERISTIC_UUID =
            UUID.fromString("2d410339-82b6-42aa-b34e-e2e01df8cc1a");

    /** UUID of GATT service */
    public static final UUID GATT_SERVICE_UUID =
            UUID.fromString("00009999-0000-1000-8000-00805f9b34fb");

    private Context mContext;
    private boolean mOffload = false;
    private long mHubId = 0;
    private long mOffloadEndpointId = 0;
    private BluetoothManager mBluetoothManager;
    private BluetoothGattServer mGattServer;
    private BluetoothLeAdvertiser mAdvertiser;
    private AcceptThread mAcceptThread = null;
    private BluetoothServerSocket mServerSocket;
    private int mPsm = -1;

    public ContextHubBluetoothSocketServer(
            Context context, boolean offload, long offloadEndpointId, long hubId) {
        mContext = context;
        mOffload = offload;
        mOffloadEndpointId = offloadEndpointId;
        mHubId = hubId;
        mBluetoothManager = context.getSystemService(BluetoothManager.class);
        assertThat(mBluetoothManager).isNotNull();
    }

    /**
     * Start the server to listen for incoming connections. This function will start the accept
     * thread and start advertising the Socket UUID.
     *
     * @return true if the server was started successfully.
     */
    public boolean startServer() {
        if (!mBluetoothManager.getAdapter().isEnabled()) {
            Log.e(TAG, "Bluetooth is not enabled");
            return false;
        }
        mAdvertiser = mBluetoothManager.getAdapter().getBluetoothLeAdvertiser();
        if (mAdvertiser == null) {
            Log.e(TAG, "Advertising not supported");
            return false;
        }
        mGattServer = mBluetoothManager.openGattServer(mContext, mGattServerCallbacks);
        if (mGattServer == null) {
            Log.e(TAG, "Unable to open GATT server");
            return false;
        }
        if (!mGattServer.addService(createGattPsmService())) {
            Log.e(TAG, "Failed to add GATT service");
            return false;
        }
        startSocketAcceptThread();
        startAdvertising();
        return true;
    }

    private void startAdvertising() {
        Log.d(TAG, "startAdvertise");
        AdvertiseData data =
                new AdvertiseData.Builder()
                        .addServiceUuid(new ParcelUuid(ADV_SOCKET_UUID))
                        .build();
        AdvertiseSettings setting =
                new AdvertiseSettings.Builder()
                        .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                        .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM)
                        .setConnectable(true)
                        .build();
        mAdvertiser.startAdvertising(setting, data, mAdvertiseCallback);
    }

    private void stopAdvertising() {
        Log.d(TAG, "stopAdvertise");
        if (mAdvertiser != null) {
            mAdvertiser.stopAdvertising(mAdvertiseCallback);
        }
    }

    private final AdvertiseCallback mAdvertiseCallback =
            new AdvertiseCallback() {
                @Override
                public void onStartFailure(int errorCode) {
                    Log.e(TAG, "onStartFailure error:" + errorCode);
                }

                @Override
                public void onStartSuccess(AdvertiseSettings settingsInEffect) {
                    super.onStartSuccess(settingsInEffect);
                }
            };

    private final BluetoothGattServerCallback mGattServerCallbacks =
            new BluetoothGattServerCallback() {
                @Override
                public void onConnectionStateChange(
                        BluetoothDevice device, int status, int newState) {
                    Log.i(
                            TAG,
                            "onConnectionStateChange: newState="
                                    + newState
                                    + " status="
                                    + status
                                    + " device="
                                    + device.getAnonymizedAddress());
                    if (newState == BluetoothProfile.STATE_CONNECTED) {
                        stopAdvertising();
                    }
                }

                @Override
                public void onCharacteristicReadRequest(
                        BluetoothDevice device,
                        int requestId,
                        int offset,
                        BluetoothGattCharacteristic characteristic) {
                    if (mGattServer == null) {
                        Log.e(TAG, "onCharacteristicReadRequest: GattServer is null, return");
                        return;
                    }

                    UUID uid = characteristic.getUuid();
                    if (!uid.equals(LE_PSM_CHARACTERISTIC_UUID)) {
                        Log.e(TAG, "onCharacteristicReadRequest: Note: unknown uuid=" + uid);
                        return;
                    }
                    Log.d(TAG, "onCharacteristicReadRequest: reading PSM");
                    byte[] value = new byte[1];
                    value[0] = (byte) (mPsm & 0xFF);
                    mGattServer.sendResponse(
                            device, requestId, BluetoothGatt.GATT_SUCCESS, offset, value);
                }
            };

    private BluetoothGattService createGattPsmService() {
        BluetoothGattService service =
                new BluetoothGattService(
                        GATT_SERVICE_UUID, BluetoothGattService.SERVICE_TYPE_PRIMARY);

        BluetoothGattCharacteristic lePsmCharacteristic =
                new BluetoothGattCharacteristic(
                        LE_PSM_CHARACTERISTIC_UUID,
                        BluetoothGattCharacteristic.PROPERTY_READ,
                        BluetoothGattCharacteristic.PERMISSION_READ);
        service.addCharacteristic(lePsmCharacteristic);

        return service;
    }

    /* Start the Bluetooth Socket Server */
    private void startSocketAcceptThread() {
        if (mAcceptThread == null) {
            mAcceptThread = new AcceptThread();
            mAcceptThread.start();
        }
        Log.d(TAG, "startSocketAcceptThread: assigned PSM=" + mPsm);
    }

    /**
     * This thread runs while listening for incoming connections. It behaves like a server-side
     * client. It runs until a connection is accepted (or until cancelled).
     */
    private class AcceptThread extends Thread {
        AcceptThread() {
            try {
                BluetoothSocketSettings.Builder builder =
                        new BluetoothSocketSettings.Builder()
                                .setSocketType(BluetoothSocket.TYPE_LE)
                                .setEncryptionRequired(false)
                                .setAuthenticationRequired(false);
                if (mOffload) {
                    builder.setDataPath(BluetoothSocketSettings.DATA_PATH_HARDWARE_OFFLOAD)
                            .setSocketName("ContextHubSocket_L2CAP_COC_Socket")
                            .setHubId(mHubId)
                            .setEndpointId(mOffloadEndpointId)
                            .setRequestedMaximumPacketSize(2048);
                }
                BluetoothSocketSettings settings = builder.build();
                mServerSocket = mBluetoothManager.getAdapter().listenUsingSocketSettings(settings);
                Log.d(TAG, "Started listening for L2CAP COC using BluetoothSocketSettings API");
                mPsm = mServerSocket.getPsm();
            } catch (IOException e) {
                Log.e(TAG, "socket listen() failed", e);
            }
        }

        public void run() {
            Log.i(TAG, "BEGIN mAcceptThread, offload=" + mOffload);
            setName("AcceptThread");

            try {
                // This is a blocking call and will only return on a
                // successful connection or an exception
                mSocket = mServerSocket.accept();
            } catch (IOException e) {
                Log.e(TAG, "socket accept() failed", e);
            }

            if (mSocket == null) {
                Log.e(TAG, "Got null socket");
            } else {
                handleSocketConnected();
            }
            Log.i(TAG, "END mAcceptThread, offload=" + mOffload);
        }
    }

    private void handleSocketConnected() {
        mAcceptThread = null;
        startSocket();
    }
}
