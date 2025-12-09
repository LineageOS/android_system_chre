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

#include "chre/target_platform/platform_bt_socket_base.h"

#include <cinttypes>
#include <cstdint>

#include "chre/platform/platform_bt_socket_resources.h"
#include "chre_api/chre.h"

namespace chre {

enum class SocketEvent : uint8_t {
  SEND_AVAILABLE,
  SOCKET_CLOSURE_REQUEST,
};

/**
 * Defines the common interface to BT socket functionality that is implemented
 * in a platform-specific way, and must be supported on every platform.
 */
class PlatformBtSocket : public PlatformBtSocketBase {
 public:
  /**
   * Constructs a new Platform Bt Socket object and initializes the socket.
   * Callers can verify the socket has been initialized successfully by calling
   * isInitialized().
   *
   * @param socketData Socket data for initializing the socket.
   * @param platformBtSocketResources Platform resources for initializing the
   *        socket.
   */
  PlatformBtSocket(const BleL2capCocSocketData &socketData,
                   PlatformBtSocketResources &platformBtSocketResources)
      : PlatformBtSocketBase(socketData, platformBtSocketResources) {}

  ~PlatformBtSocket();

  // Delete the copy constructor
  PlatformBtSocket(const PlatformBtSocket &) = delete;
  // Disable copy assignment constructor
  PlatformBtSocket &operator=(const PlatformBtSocket &other) = delete;

  void setSocketAccepted(bool accepted) {
    mSocketAccepted = accepted;
  }

  bool getSocketAccepted() {
    return mSocketAccepted;
  }

  uint64_t getId();

  uint16_t getNanoappInstanceId() {
    return mInstanceId;
  }

  void setNanoappInstanceId(uint16_t instanceId) {
    mInstanceId = instanceId;
  }

  /**
   * Checks whether the socket has been initialized successfully by the
   * constructor.
   *
   * @return true if socket was initialized successfully.
   */
  bool isInitialized();

  /**
   * Sends a packet to the socket.
   *
   * @see chreBleSocketSend
   */
  int32_t sendSocketPacket(const void *data, uint16_t length,
                           chreBleSocketPacketFreeFunction *freeCallback);

  // Frees a socket packet after it has been received by the nanoapp.
  void freeReceivedSocketPacket();

 private:
  // Nanoapp instance ID.
  uint16_t mInstanceId = 0;

  // Whether the nanoapp accepted the socket.
  bool mSocketAccepted = false;
};

}  // namespace chre
