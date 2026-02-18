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

#pragma once

#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED

#include "chre/core/ble_l2cap_coc_socket_data.h"
#include "chre/core/multi_threading_api_mutex.h"
#include "chre/platform/platform_bt_socket.h"
#include "chre/platform/platform_bt_socket_resources.h"
#include "chre/util/memory_pool.h"
#include "chre_api/chre.h"

namespace chre {

/**
 * Manages offloaded BLE sockets. Handles sending packets between nanoapps and
 * BLE sockets.
 */
class BleSocketManager : public NonCopyable {
 public:
  // Public for testing purposes.
  static constexpr uint8_t kMaxNumSockets = 2;

  // Forward all arguments passed to the BleSocketManager constructor to the
  // PlatformBtSocketResources constructor
  template <typename... Args>
  BleSocketManager(Args &&...args)
      : mPlatformBtSocketResources(std::forward<Args>(args)...) {}

  /**
   * Handles a request from the Host to get CHRE's BLE socket capabilities.
   */
  void handleSocketCapabilitiesRequestByHost();

  /**
   * Handles a socket open request from the host. Switches the context to the
   * event loop thread before processing the socket open request with
   * handleSocketOpenedByHostSync.
   *
   * @param socketData Metadata for the BLE socket.
   */
  static void handleSocketOpenedByHost(const BleL2capCocSocketData &socketData);

  /**
   * Validates if a socket ID is currently managed by CHRE. This is used by
   * nanoapps to accept an incoming socket connection.
   *
   * @param socketId The ID of the socket to find.
   * @return true if the socket exists, false otherwise.
   */
  bool acceptBleSocket(uint64_t socketId) {
    PlatformBtSocket *btSocket = findPlatformBtSocket(socketId);
    if (btSocket != nullptr) {
      btSocket->setSocketAccepted(true);
    }
    return btSocket != nullptr;
  }

  /**
   * Sends a packet to the socket.
   *
   * @see chreBleSocketSend
   */
  int32_t sendBleSocketPacket(uint64_t appId, uint64_t socketId,
                              const void *data, uint16_t length,
                              chreBleSocketPacketFreeFunction *freeCallback);

  /**
   * Handles a request to free the socket packet from the platform. Switches the
   * context to the event loop thread before freeing the socket packet.
   *
   * @param appId ID of the nanoapp that owns the socket packet.
   * @param data Socket packet to be freed.
   * @param length Length of socket packet.
   * @param freeCallback @see chreBleSocketPacketFreeFunction
   */
  static void freeSocketPacket(uint64_t appId, void *data, uint16_t length,
                               chreBleSocketPacketFreeFunction *freeCallback);

  /**
   * Handles a socket event originating from the platform. Switches the context
   * to the event loop thread before processing the event with
   * handlePlatformSocketEventSync.
   *
   * @param socketId Identifies socket which the event is for.
   * @param socketEvent Socket event to be processed.
   */
  static void handlePlatformSocketEvent(uint64_t socketId,
                                        SocketEvent socketEvent);

  /**
   * Handles a socket packet from the platform. Switches the context to the
   * event loop thread before processing the event with
   * handlePlatformSocketPacketSync.
   *
   * @param socketId ID of the socket sending the packet.
   * @param data Socket packet data.
   * @param length Socket packet data length.
   */
  static void handlePlatformSocketPacket(uint64_t socketId, const uint8_t *data,
                                         uint16_t length);

  /**
   * Closes the sockets belonging to a nanoapp when it is unloaded.
   *
   * @param nanoappInstanceId Nanoapp being unloaded.
   * @return number of sockets closed.
   */
  uint32_t closeSocketsOnNanoappUnload(uint16_t nanoappInstanceId);

  /**
   * Handles the host closing the socket. Switches the context to the event loop
   * thread before processing the socket close request with
   * handleSocketClosedByHostSync.
   *
   * @param socketId Socket ID to be closed.
   */
  static void handleSocketClosedByHost(uint64_t socketId);

 private:
  /**
   * @see handleSocketOpenedByHost
   */
  void handleSocketOpenedByHostSync(const BleL2capCocSocketData &socketData)
      CHRE_REQUIRES(getMultiThreadingApiMutex());

  /**
   * @see handlePlatformSocketEvent
   */
  void handlePlatformSocketEventSync(uint64_t socketId, SocketEvent socketEvent)
      CHRE_REQUIRES(getMultiThreadingApiMutex());

  /**
   * @see handlePlatformSocketPacket
   */
  void handlePlatformSocketPacketSync(chreBleSocketPacketEvent *event)
      CHRE_REQUIRES(getMultiThreadingApiMutex());

  /**
   * @see handleSocketClosedByHost
   */
  void handleSocketClosedByHostSync(uint64_t socketId)
      CHRE_REQUIRES(getMultiThreadingApiMutex());

  /**
   * Tracks BT sockets and their corresponding nanoapp.
   *
   * TODO(b/418832158): We can't use a CHRE FixedSizeVector here because some
   * PlatformBtSocket implementations have dependencies which delete the copy
   * and move assignment operators. Look into adding move assignment operators
   * to those dependencies and refactor this code when finished.
   */
  MemoryPool<PlatformBtSocket, kMaxNumSockets> mBtSockets;

  /**
   * Platform resources used for creating a new BT socket.
   */
  PlatformBtSocketResources mPlatformBtSocketResources;

  PlatformBtSocket *findPlatformBtSocket(uint64_t socketId) {
    return mBtSockets.find(
        [](PlatformBtSocket *btSocket, void *targetSocketId) {
          return btSocket->getId() == *static_cast<uint64_t *>(targetSocketId);
        },
        &socketId);
  }
};

}  // namespace chre

#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED
