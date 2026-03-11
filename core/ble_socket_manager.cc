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

#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED

#include "chre/core/ble_socket_manager.h"

#include "chre/core/event.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/context.h"
#include "chre/platform/log.h"

namespace chre {

namespace {

struct socketEventData {
  uint64_t socketId;
  SocketEvent event;
};

struct socketPacketData {
  uint64_t appId;
  void *data;
  uint16_t length;
  chreBleSocketPacketFreeFunction *freeCallback;
};

/**
 * Handles a socket open request from the host. This function is a core
 * implementation shared by the L2CAP and RFCOMM socket open handlers. It
 * performs the following tasks:
 * 1. Checks if the socket is valid and initialized.
 * 2. Checks if the nanoapp that opened the socket is loaded.
 * 3. Distributes the CHRE_EVENT_BLE_SOCKET_CONNECTION event to the nanoapp.
 * 4. If the nanoapp does not accept the socket, returns false to deallocate the
 *    socket.
 * 5. Sends the BT socket open response to the host.
 *
 * @param btSocket The platform socket to use for the connection.
 * @param endpointId The endpoint ID of the nanoapp that opened the socket.
 * @param socketId The ID of the socket.
 * @param txMtu The MTU of the socket.
 * @param rxMtu The MTU of the socket.
 * @return false if the socket should be deallocated, true otherwise.
 */
bool handleSocketOpenedByHostSyncCore(PlatformBtSocket *btSocket,
                                      uint64_t endpointId, uint64_t socketId,
                                      uint16_t txMtu, uint16_t rxMtu) {
  const char *errorReason = nullptr;
  uint16_t targetInstanceId;

  if (btSocket == nullptr) {
    errorReason = "no available sockets";
  } else if (!btSocket->isInitialized()) {
    errorReason = "failed to initialize socket";
  } else if (!EventLoopManagerSingleton::get()
                  ->getEventLoop()
                  .findNanoappInstanceIdByAppId(endpointId,
                                                &targetInstanceId)) {
    errorReason = "failed to find nanoapp";
  } else {
    btSocket->setNanoappInstanceId(targetInstanceId);
    btSocket->setNanoappAppId(endpointId);
    // TODO(b/425747779): Populate BT socket name
    chreBleSocketConnectionEvent event = {.socketId = socketId,
                                          .socketName = nullptr,
                                          .maxTxPacketLength = txMtu,
                                          .maxRxPacketLength = rxMtu};
    EventLoopManagerSingleton::get()->getEventLoop().distributeEventSync(
        CHRE_EVENT_BLE_SOCKET_CONNECTION, &event, targetInstanceId);
    if (!btSocket->getSocketAccepted()) {
      errorReason = "nanoapp did not accept socket";
    }
  }

  bool success = (errorReason == nullptr);
  const char *reason = success ? "success" : errorReason;
  if (!success) {
    LOGE("Failed to open BT socketId=%" PRIu64 " for endpointId=%" PRIx64
         ": %s",
         socketId, endpointId, errorReason);
  }
  EventLoopManagerSingleton::get()
      ->getHostCommsManager()
      .sendBtSocketOpenResponse(socketId, success, reason);

  return success;
}

}  // namespace

template <typename SocketDataType>
void BleSocketManager::handleSocketOpenedByHost(
    const SocketDataType &socketData) {
  LOGI("handleSocketOpenedByHost request for endpointId: %" PRIx64
       " socketId: %" PRIu64,
       socketData.endpointId, socketData.socketId);
  auto cbData = MakeUnique<SocketDataType>(socketData);
  if (cbData.isNull()) {
    LOG_OOM();
    EventLoopManagerSingleton::get()
        ->getHostCommsManager()
        .sendBtSocketOpenResponse(socketData.socketId, /*success=*/false,
                                  /*reason=*/"out of memory");
    return;
  }
  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::BleSocketConnected, std::move(cbData),
      [](SystemCallbackType, UniquePtr<SocketDataType> &&data)
          CHRE_REQUIRES(getMultiThreadingApiMutex()) {
            EventLoopManagerSingleton::get()
                ->getBleSocketManager()
                .handleSocketOpenedByHostSync(*data);
          });
}

void BleSocketManager::handleSocketCapabilitiesRequestByHost() {
  EventLoopManagerSingleton::get()
      ->getHostCommsManager()
      .sendBtSocketGetCapabilitiesResponse(
          /*leCocNumberOfSupportedSockets=*/kMaxNumLeCocSockets,
          /*leCocMtu=*/mPlatformBtSocketResources.getLeCocMtu(),
          /*rfcommNumberOfSupportedSockets=*/kMaxNumRfcommSockets,
          /*rfcommMaxFrameSize=*/
          mPlatformBtSocketResources.getRfcommMaxFrameSize());
}

template <typename SocketDataType>
void BleSocketManager::handleSocketOpenedByHostSync(
    const SocketDataType &socketData) {
  PlatformBtSocket *btSocket =
      mBtSockets.allocate(socketData, mPlatformBtSocketResources);
  if (!handleSocketOpenedByHostSyncCore(
          btSocket, socketData.endpointId, socketData.socketId,
          socketData.txConfig.mtu, socketData.rxConfig.mtu)) {
    if (btSocket != nullptr) {
      mBtSockets.deallocate(btSocket);
    }
  }
}

template void BleSocketManager::handleSocketOpenedByHost(
    const BleL2capCocSocketData &socketData);
template void BleSocketManager::handleSocketOpenedByHostSync(
    const BleL2capCocSocketData &socketData);

#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
template void BleSocketManager::handleSocketOpenedByHost(
    const BtRfcommChannelSocketData &socketData);
template void BleSocketManager::handleSocketOpenedByHostSync(
    const BtRfcommChannelSocketData &socketData);
#else
template <>
void BleSocketManager::handleSocketOpenedByHost(
    const BtRfcommChannelSocketData & /*socketData*/) {}
template <>
void BleSocketManager::handleSocketOpenedByHostSync(
    const BtRfcommChannelSocketData & /*socketData*/) {}
#endif  // CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED

int32_t BleSocketManager::sendBleSocketPacket(
    uint64_t appId, uint64_t socketId, const void *data, uint16_t length,
    chreBleSocketPacketFreeFunction *freeCallback) {
  PlatformBtSocket *btSocket = findPlatformBtSocket(socketId);
  if (btSocket == nullptr) {
    LOGE("BT socketId %" PRIu64 " not found. NanoappId: %" PRIu64, socketId,
         appId);
    if (freeCallback != nullptr) {
      freeSocketPacket(appId, const_cast<void *>(data), length, freeCallback);
    }
    return CHRE_BLE_SOCKET_SEND_STATUS_FAILURE;
  }
  return btSocket->sendSocketPacket(data, length, freeCallback);
}

void BleSocketManager::freeSocketPacket(
    uint64_t appId, void *data, uint16_t length,
    chreBleSocketPacketFreeFunction *freeCallback) {
  auto packetData = MakeUnique<socketPacketData>();
  if (packetData.isNull()) {
    LOG_OOM();
    return;
  }
  packetData->appId = appId;
  packetData->data = data;
  packetData->length = length;
  packetData->freeCallback = freeCallback;

  // TODO(b/475537998): This callback is scheduled by deferCallback() later
  //  as a system callback, meaning a global lock will be held by its
  //  caller, EventLoop::freeEvent(). But if packetData->freeCallback wants to
  //  hold a lock again it will be deadlocked. Wrapping
  //  invokeMessageFreeFunction() with lock.unlock() and lock.lock() for now as
  //  a workaround which will be replaced by a perm fix soon.
  auto callback = [](SystemCallbackType,
                     UniquePtr<socketPacketData> &&packetData) {
    MultiThreadingApiMutex *lock = getMultiThreadingApiMutex();
    lock->unlock();
    getCurrentEventLoop()->invokeMessageFreeFunction(
        packetData->appId,
        reinterpret_cast<chreMessageFreeFunction *>(packetData->freeCallback),
        packetData->data, packetData->length);
    lock->lock();
  };

  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::BleSocketFreePacketEvent, std::move(packetData),
      callback, EventLoopManagerSingleton::get()->getEventLoopByAppId(appId));
}

void BleSocketManager::handlePlatformSocketEvent(uint64_t socketId,
                                                 SocketEvent event) {
  auto socketEvent = MakeUnique<socketEventData>();

  if (socketEvent.isNull()) {
    LOG_OOM();
    CHRE_ASSERT(false);
    return;
  }
  socketEvent->socketId = socketId;
  socketEvent->event = event;

  auto callback =
      [](SystemCallbackType, UniquePtr<socketEventData> &&socketEvent)
          CHRE_REQUIRES(getMultiThreadingApiMutex()) {
            EventLoopManagerSingleton::get()
                ->getBleSocketManager()
                .handlePlatformSocketEventSync(socketEvent->socketId,
                                               socketEvent->event);
          };

  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::BleSocketEvent, std::move(socketEvent), callback);
}

void BleSocketManager::handlePlatformSocketEventSync(uint64_t socketId,
                                                     SocketEvent event) {
  PlatformBtSocket *btSocket = findPlatformBtSocket(socketId);
  if (btSocket == nullptr) {
    LOGW("Received event %" PRIu8
         " for disconnected/unknown BT socketId %" PRIu64,
         event, socketId);
    return;
  }
  switch (event) {
    case SocketEvent::SEND_AVAILABLE:
      EventLoopManagerSingleton::get()->getEventLoop().distributeEventSync(
          CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE, nullptr,
          btSocket->getNanoappInstanceId());
      break;
    case SocketEvent::SOCKET_CLOSURE_REQUEST:
      LOGI(
          "The platform encountered an unrecoverable error and is requesting "
          "closure of socketId=%" PRIu64,
          btSocket->getId());
      EventLoopManagerSingleton::get()->getHostCommsManager().sendBtSocketClose(
          btSocket->getId(), "offload stack requests socket closure");
      break;
    default:
      LOGE("Received unknown event %" PRIu8 " for socketId=%" PRIu64,
           static_cast<uint8_t>(event), btSocket->getId());
      break;
  }
}

void BleSocketManager::handlePlatformSocketPacket(uint64_t socketId,
                                                  const uint8_t *data,
                                                  uint16_t length) {
  auto packetEvent = MakeUnique<chreBleSocketPacketEvent>();
  if (packetEvent.isNull()) {
    LOG_OOM();
    return;
  }
  packetEvent->socketId = socketId;
  packetEvent->data = data;
  packetEvent->length = length;

  auto callback =
      [](SystemCallbackType, UniquePtr<chreBleSocketPacketEvent> &&packetEvent)
          CHRE_REQUIRES(getMultiThreadingApiMutex()) {
            EventLoopManagerSingleton::get()
                ->getBleSocketManager()
                .handlePlatformSocketPacketSync(packetEvent.get());
          };
  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::BleSocketPacketEvent, std::move(packetEvent),
      callback);
}

void BleSocketManager::handlePlatformSocketPacketSync(
    chreBleSocketPacketEvent *event) {
  PlatformBtSocket *btSocket = findPlatformBtSocket(event->socketId);
  if (btSocket == nullptr) {
    LOGW("Received packet for disconnected/unknown BT socketId %" PRIu64,
         event->socketId);
    return;
  }
  EventLoopManagerSingleton::get()->getEventLoop().distributeEventSync(
      CHRE_EVENT_BLE_SOCKET_PACKET, event, btSocket->getNanoappInstanceId());
  btSocket->freeReceivedSocketPacket();
}

uint32_t BleSocketManager::closeSocketsOnNanoappUnload(
    uint16_t nanoappInstanceId) {
  return mBtSockets.forEach(
      [](PlatformBtSocket *btSocket, void *data) {
        uint64_t nanoappInstanceId = *static_cast<uint64_t *>(data);
        if (btSocket->getNanoappInstanceId() == nanoappInstanceId) {
          EventLoopManagerSingleton::get()
              ->getHostCommsManager()
              .sendBtSocketClose(btSocket->getId(), "Nanoapp unloaded");
          return true;
        }
        return false;
      },
      &nanoappInstanceId);
}

void BleSocketManager::handleSocketClosedByHost(uint64_t socketId) {
  auto cbData = MakeUnique<uint64_t>(socketId);
  if (cbData == nullptr) {
    LOG_OOM();
    return;
  }
  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::BleSocketClosed, std::move(cbData),
      [](SystemCallbackType /*type*/, UniquePtr<uint64_t> &&data)
          CHRE_REQUIRES(getMultiThreadingApiMutex()) {
            EventLoopManagerSingleton::get()
                ->getBleSocketManager()
                .handleSocketClosedByHostSync(*data);
          });
}

void BleSocketManager::handleSocketClosedByHostSync(uint64_t socketId) {
  PlatformBtSocket *btSocket = findPlatformBtSocket(socketId);
  if (btSocket == nullptr) {
    LOGE("Received notification that host closed socketId=%" PRIu64
         " but socket does not exist.",
         socketId);
    return;
  }
  LOGI("Host closed socketId=%" PRIu64 " notifying nanoapp instanceId=%" PRIu16,
       socketId, btSocket->getNanoappInstanceId());
  chreBleSocketDisconnectionEvent event = {.socketId = btSocket->getId()};
  EventLoopManagerSingleton::get()->getEventLoop().distributeEventSync(
      CHRE_EVENT_BLE_SOCKET_DISCONNECTION, &event,
      btSocket->getNanoappInstanceId());
  mBtSockets.deallocate(btSocket);
}

}  // namespace chre

#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED
