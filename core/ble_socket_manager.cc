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
#include "chre/platform/log.h"
#include "chre_api/chre.h"

namespace chre {

namespace {

struct socketEventData {
  uint64_t socketId;
  SocketEvent event;
};

struct socketPacketData {
  void *data;
  uint16_t length;
  chreBleSocketPacketFreeFunction *freeCallback;
};

}  // namespace

void BleSocketManager::handleSocketCapabilitiesRequestByHost() {
  EventLoopManagerSingleton::get()
      ->getHostCommsManager()
      .sendBtSocketGetCapabilitiesResponse(
          /*leCocNumberOfSupportedSockets=*/kMaxNumSockets,
          /*leCocMtu=*/mPlatformBtSocketResources.getLeCocMtu(),
          /*rfcommNumberOfSupportedSockets=*/0,
          /*rfcommMaxFrameSize=*/0);
}

void BleSocketManager::handleSocketOpenedByHost(
    const BleL2capCocSocketData &socketData) {
  LOGI("handleSocketOpenedByHost request for endpointId: %" PRIx64
       " socketId: %" PRIu64,
       socketData.endpointId, socketData.socketId);
  auto cbData = MakeUnique<BleL2capCocSocketData>(socketData);
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
      [](SystemCallbackType, UniquePtr<BleL2capCocSocketData> &&data) {
        EventLoopManagerSingleton::get()
            ->getBleSocketManager()
            .handleSocketOpenedByHostSync(*(data.get()));
      });
}

void BleSocketManager::handleSocketOpenedByHostSync(
    const BleL2capCocSocketData &socketData) {
  const char *errorReason = nullptr;
  uint16_t targetInstanceId;

  PlatformBtSocket *btSocket =
      mBtSockets.allocate(socketData, mPlatformBtSocketResources);

  if (btSocket == nullptr) {
    errorReason = "no available sockets";
  } else if (!btSocket->isInitialized()) {
    errorReason = "failed to initialize socket";
  } else if (!EventLoopManagerSingleton::get()
                  ->getEventLoop()
                  .findNanoappInstanceIdByAppId(socketData.endpointId,
                                                &targetInstanceId)) {
    errorReason = "failed to find nanoapp";
  } else {
    btSocket->setNanoappInstanceId(targetInstanceId);
    // TODO(b/425747779): Populate BT socket name
    chreBleSocketConnectionEvent event = {
        .socketId = socketData.socketId,
        .socketName = nullptr,
        .maxTxPacketLength = socketData.txConfig.mtu,
        .maxRxPacketLength = socketData.rxConfig.mtu};
    EventLoopManagerSingleton::get()->getEventLoop().distributeEventSync(
        CHRE_EVENT_BLE_SOCKET_CONNECTION, &event, targetInstanceId);
    if (!btSocket->getSocketAccepted()) {
      errorReason = "nanoapp did not accept socket";
    }
  }

  if (errorReason != nullptr) {
    LOGE("Failed to open BT socketId=%" PRIu64 " for endpointId=%" PRIx64
         ": %s",
         socketData.socketId, socketData.endpointId, errorReason);
    if (btSocket != nullptr) {
      mBtSockets.deallocate(btSocket);
    }
  }
  bool success = (errorReason == nullptr);
  const char *reason = success ? "success" : errorReason;
  EventLoopManagerSingleton::get()
      ->getHostCommsManager()
      .sendBtSocketOpenResponse(socketData.socketId, success, reason);
}

bool BleSocketManager::acceptBleSocket(uint64_t socketId) {
  PlatformBtSocket *btSocket = mBtSockets.find(
      [](PlatformBtSocket *btSocket, void *data) {
        uint64_t socketId = *(static_cast<uint64_t *>(data));
        if (btSocket->getId() == socketId) {
          btSocket->setSocketAccepted(true);
          return true;
        }
        return false;
      },
      &socketId);
  return btSocket != nullptr;
}

int32_t BleSocketManager::sendBleSocketPacket(
    uint64_t socketId, const void *data, uint16_t length,
    chreBleSocketPacketFreeFunction *freeCallback) {
  PlatformBtSocket *btSocket = findPlatformBtSocket(socketId);
  if (btSocket == nullptr) {
    LOGE("BT socketId %" PRIu64 " not found", socketId);
    freeCallback(const_cast<void *>(data), length);
    return CHRE_BLE_SOCKET_SEND_STATUS_FAILURE;
  }
  return btSocket->sendSocketPacket(data, length, freeCallback);
}

void BleSocketManager::freeSocketPacket(
    void *data, uint16_t length,
    chreBleSocketPacketFreeFunction *freeCallback) {
  auto packetData = MakeUnique<socketPacketData>();
  packetData->data = data;
  packetData->length = length;
  packetData->freeCallback = freeCallback;

  auto callback = [](SystemCallbackType,
                     UniquePtr<socketPacketData> &&packetData) {
    packetData->freeCallback(packetData->data, packetData->length);
  };

  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::BleSocketFreePacketEvent, std::move(packetData),
      callback);
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

  auto callback = [](SystemCallbackType,
                     UniquePtr<socketEventData> &&socketEvent) {
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
    LOGW("Received event %" PRIu32
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
  packetEvent->socketId = socketId;
  packetEvent->data = data;
  packetEvent->length = length;

  auto callback = [](SystemCallbackType,
                     UniquePtr<chreBleSocketPacketEvent> &&packetEvent) {
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
        uint64_t nanoappInstanceId = *(static_cast<uint64_t *>(data));
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
      [](SystemCallbackType /*type*/, UniquePtr<uint64_t> &&data) {
        EventLoopManagerSingleton::get()
            ->getBleSocketManager()
            .handleSocketClosedByHostSync(*data.get());
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
