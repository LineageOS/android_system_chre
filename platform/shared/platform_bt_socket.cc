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

#include "chre/platform/platform_bt_socket.h"
#include "chre/target_platform/platform_bt_socket_base.h"

#include "chre/core/event_loop_manager.h"
#include "chre/platform/log.h"
#include "chre/platform/shared/memory.h"
#include "pw_multibuf/from_span.h"
#include "pw_status/status.h"

namespace chre {

PlatformBtSocketBase::PlatformBtSocketBase(
    const BleL2capCocSocketData &socketData,
    PlatformBtSocketResources &platformBtSocketResources)
    : mId(socketData.socketId), mSocketType(SocketType::L2CAP_LE_COC) {
  if (socketData.rxConfig.mps == 0) {
    LOGE("Rx MPS cannot be 0");
    return;
  }
  uint16_t rxCredits = kRxMultiBufAreaSize / socketData.rxConfig.mps;
  if (rxCredits < socketData.rxConfig.credits) {
    LOGE(
        "Socket allocated more Rx credits to the remote device than CHRE is "
        "capable of supporting");
    return;
  }
  pw::bluetooth::proxy::L2capCoc::CocConfig pwRxConfig = {
      .cid = socketData.rxConfig.cid,
      .mtu = socketData.rxConfig.mtu,
      .mps = socketData.rxConfig.mps,
      .credits = socketData.rxConfig.credits,
  };
  pw::bluetooth::proxy::L2capCoc::CocConfig pwTxConfig = {
      .cid = socketData.txConfig.cid,
      .mtu = socketData.txConfig.mtu,
      .mps = socketData.txConfig.mps,
      .credits = socketData.txConfig.credits,
  };

  pw::Result<pw::bluetooth::proxy::L2capCoc> result =
      platformBtSocketResources.getProxyHost().AcquireL2capCoc(
          mRxSimpleAllocator, socketData.connectionHandle, pwRxConfig,
          pwTxConfig,
          pw::bind_member<&PlatformBtSocketBase::handleRxSocketPacket>(this),
          pw::bind_member<&PlatformBtSocketBase::handleL2capSocketEvent>(this));
  if (!result.ok()) {
    LOGE("AcquireL2capCoc failed: %s", result.status().str());
    return;
  }
  mL2capCoc.emplace(std::move(result.value()));
  // CHRE expects the socket has not allocated Rx credits to the remote device
  // prior to being offloaded to CHRE. If CHRE receives a socket open request
  // with the Rx credits value populated, it assumes these have already been
  // allocated to the remote device.
  if (socketData.rxConfig.credits > 0) {
    LOGW("Assuming host allocated %" PRIu16
         " Rx credits to remote device prior to offloading the socket to CHRE",
         socketData.rxConfig.credits);
    rxCredits -= socketData.rxConfig.credits;
  }
  if (rxCredits > 0) {
    pw::Status status = mL2capCoc->SendAdditionalRxCredits(rxCredits);
    if (!status.ok()) {
      LOGE("SendAdditionalRxCredits failed: %s", status.str());
      mL2capCoc.reset();
    }
  }
}

#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
PlatformBtSocketBase::PlatformBtSocketBase(
    const BtRfcommChannelSocketData &socketData,
    PlatformBtSocketResources &platformBtSocketResources)
    : mId(socketData.socketId), mSocketType(SocketType::RFCOMM_CHANNEL) {
  if (socketData.rxConfig.maxFrameSize == 0) {
    LOGE("Rx Max Frame Size cannot be 0");
    return;
  }
  pw::bluetooth::proxy::rfcomm::RfcommChannelConfig pwRxConfig = {
      .cid = socketData.rxConfig.cid,
      .max_frame_size = socketData.rxConfig.maxFrameSize,
      .initial_credits = socketData.rxConfig.credits,
  };
  uint16_t rxCredits = kRxMultiBufAreaSize / socketData.rxConfig.maxFrameSize;
  if (rxCredits < socketData.rxConfig.credits) {
    LOGE(
        "Socket allocated more Rx credits to the remote device than CHRE is "
        "capable of supporting");
    return;
  }
  pw::bluetooth::proxy::rfcomm::RfcommChannelConfig pwTxConfig = {
      .cid = socketData.txConfig.cid,
      .max_frame_size = socketData.txConfig.maxFrameSize,
      .initial_credits = static_cast<uint8_t>(socketData.txConfig.credits),
  };

  pw::Result<pw::bluetooth::proxy::rfcomm::RfcommChannel> result =
      platformBtSocketResources.getRfcommProxyHost().AcquireRfcommChannel(
          mRxSimpleAllocator,
          static_cast<pw::bluetooth::proxy::ConnectionHandle>(
              socketData.connectionHandle),
          (socketData.dlci >> 1),
          (socketData.dlci & 1)
              ? pw::bluetooth::proxy::rfcomm::RfcommDirection::kInitiator
              : pw::bluetooth::proxy::rfcomm::RfcommDirection::kResponder,
          socketData.muxInitiator, pwRxConfig, pwTxConfig,
          pw::bind_member<&PlatformBtSocketBase::handleRxSocketPacket>(this),
          pw::bind_member<&PlatformBtSocketBase::handleRfcommSocketEvent>(
              this));
  if (!result.ok()) {
    LOGE("AcquireRfcommChannel failed: %s", result.status().str());
    return;
  }
  mRfcommChannel.emplace(std::move(result.value()));
}
#endif  // CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED

PlatformBtSocket::~PlatformBtSocket() {
  // The channel must be destroyed first to avoid the race condition in which
  // the channel receives data and attempts to use the receive callback from an
  // Rx thread while the socket is being destroyed by CHRE's event loop thread.
  // Pigweed's ChannelManager uses thread protection to ensure that data cannot
  // be sent via the receive callback after the channel has been destroyed.
  if (mSocketType == SocketType::L2CAP_LE_COC && mL2capCoc.has_value()) {
    mL2capCoc->Close();
    mL2capCoc.reset();
  }
#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
  else if (mSocketType == SocketType::RFCOMM_CHANNEL &&
             mRfcommChannel.has_value()) {
    mRfcommChannel.reset();
  }
#endif
}

void PlatformBtSocketBase::handleRxSocketPacket(
    pw::multibuf::MultiBuf &&payload) {
  std::optional<pw::ConstByteSpan> packet = payload.ContiguousSpan();
  CHRE_ASSERT(packet.has_value());
  {
    LockGuard<Mutex> lockGuard(mRxSocketPacketsMutex);
    CHRE_ASSERT(mRxSocketPackets.push(std::move(payload)));
  }

  /**
   * NOTE: handlePlatformSocketPacket() adds an event to CHRE's event
   * queue. We call forceDramAccess after adding this event to CHRE's event
   * queue to avoid the race condition in which forceDramAccess() is called and
   * CHRE's event queue empties, triggering a call to removeDramAccessVote()
   * right before this event is enqueued.
   *
   * TODO(b/429237573): Support enqueueing high power events on CHRE's event
   * queue and remove forceDramAccess call.
   */
  BleSocketManager::handlePlatformSocketPacket(
      mId, reinterpret_cast<const uint8_t *>(packet->data()),
      static_cast<uint16_t>(packet->size()));
  forceDramAccess();
}

void PlatformBtSocketBase::handleL2capSocketEvent(
    pw::bluetooth::proxy::L2capChannelEvent event) {
  SocketEvent platformEvent = SocketEvent::SEND_AVAILABLE;
  switch (event) {
    case pw::bluetooth::proxy::L2capChannelEvent::kWriteAvailable:
      break;
    case pw::bluetooth::proxy::L2capChannelEvent::kChannelClosedByOther:
      // Do not process event in CHRE
      LOGI("Host or remote device closed socket");
      return;
    case pw::bluetooth::proxy::L2capChannelEvent::kReset:
      // Do not process event in CHRE
      LOGI("BT reset closed socket");
      return;
    case pw::bluetooth::proxy::L2capChannelEvent::kRxInvalid:
      LOGE("Socket Rx packet invalid, requesting closure");
      platformEvent = SocketEvent::SOCKET_CLOSURE_REQUEST;
      break;
    case pw::bluetooth::proxy::L2capChannelEvent::kRxOutOfMemory:
      LOGE("OOM to receive Rx packet, requesting closure");
      platformEvent = SocketEvent::SOCKET_CLOSURE_REQUEST;
      break;
    case pw::bluetooth::proxy::L2capChannelEvent::kRxWhileStopped:
      // Do not process event in CHRE
      LOGW(
          "Received Rx packet while in `stopped` state. Waiting on channel "
          "closure");
      return;
    default:
      // Do not process event in CHRE
      LOGE("Received unexpected socket event %" PRIu32,
           static_cast<uint32_t>(event));
      return;
  }

  /**
   * NOTE: handlePlatformSocketEvent() adds an event to CHRE's event queue. We
   * call forceDramAccess after adding this event to CHRE's event queue to avoid
   * the race condition in which forceDramAccess() is called and CHRE's event
   * queue empties, triggering a call to removeDramAccessVote() right before
   * this event is enqueued.
   *
   * TODO(b/429237573): Support enqueueing high power events on CHRE's event
   * queue and remove forceDramAccess call.
   */
  BleSocketManager::handlePlatformSocketEvent(mId, platformEvent);
  forceDramAccess();
}

#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
void PlatformBtSocketBase::handleRfcommSocketEvent(
    pw::bluetooth::proxy::rfcomm::RfcommEvent event) {
  SocketEvent platformEvent = SocketEvent::SEND_AVAILABLE;
  switch (event) {
    case pw::bluetooth::proxy::rfcomm::RfcommEvent::kChannelReadyToSend:
      break;
    case pw::bluetooth::proxy::rfcomm::RfcommEvent::kChannelClosedByRemote:
      // Do not process event in CHRE
      LOGI("Remote device closed socket");
      return;
    case pw::bluetooth::proxy::rfcomm::RfcommEvent::kChannelClosedByOther:
      // Do not process event in CHRE
      LOGI("Host or remote device closed socket");
      return;
    case pw::bluetooth::proxy::rfcomm::RfcommEvent::kReset:
      // Do not process event in CHRE
      LOGI("BT reset closed socket");
      return;
    default:
      // Do not process event in CHRE
      LOGE("Received unexpected socket event %" PRIu32,
           static_cast<uint32_t>(event));
      return;
  }

  /**
   * NOTE: handlePlatformSocketEvent() adds an event to CHRE's event queue. We
   * call forceDramAccess after adding this event to CHRE's event queue to avoid
   * the race condition in which forceDramAccess() is called and CHRE's event
   * queue empties, triggering a call to removeDramAccessVote() right before
   * this event is enqueued.
   *
   * TODO(b/429237573): Support enqueueing high power events on CHRE's event
   * queue and remove forceDramAccess call.
   */
  BleSocketManager::handlePlatformSocketEvent(mId, platformEvent);
  forceDramAccess();
}
#endif  // CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED

void PlatformBtSocket::freeReceivedSocketPacket() {
  LockGuard<Mutex> lockGuard(mRxSocketPacketsMutex);
  mRxSocketPackets.pop();
}

uint64_t PlatformBtSocket::getId() {
  return mId;
}

bool PlatformBtSocket::isInitialized() {
  if (mSocketType == SocketType::L2CAP_LE_COC) {
    return mL2capCoc.has_value();
  } else {
#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
    return mRfcommChannel.has_value();
#else
    return false;
#endif  // CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
  }
}

struct SocketSendContext {
  uint64_t appId;
  chreBleSocketPacketFreeFunction *freeCallback;
};

int32_t PlatformBtSocket::sendSocketPacket(
    const void *data, uint16_t length,
    chreBleSocketPacketFreeFunction *freeCallback) {
  // Per CHRE API, if this call results in
  // CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL, do not use the free callback. In
  // this scenario, it is the responsibility of the nanoapp to free the data.
  // The nanoapp may choose to hold on to the data until it receives a
  // CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE event when it can re-attempt the send.
  if (mSocketType == SocketType::L2CAP_LE_COC) {
    if (mL2capCoc->IsWriteAvailable() == pw::Status::Unavailable()) {
      return CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL;
    }
  }

  auto nonConstData = const_cast<uint8_t *>(static_cast<const uint8_t *>(data));
  pw::ByteSpan byteSpan(reinterpret_cast<std::byte *>(nonConstData), length);

  /**
   * This deleter function can either be called from the CHRE thread, in which
   * case, this code is already running in DRAM, or from the BT Rx thread. If it
   * is called from the BT Rx thread, it is expected that the caller invokes
   * DramVoteClient::incrementDramVoteCount() and
   * DramVoteClient::decrementDramVoteCount() around use of this function.
   *
   * NOTE: freeSocketPacket() adds an event to CHRE's event queue. We call
   * forceDramAccess after adding this event to CHRE's event queue to avoid the
   * race condition in which forceDramAccess() is called and CHRE's event queue
   * empties, triggering a call to removeDramAccessVote() right before this
   * event is enqueued.
   */
  auto *context = memoryAlloc<SocketSendContext>();
  if (context == nullptr) {
    LOG_OOM();
    if (freeCallback != nullptr) {
      BleSocketManager::freeSocketPacket(mAppId, const_cast<void *>(data),
                                         length, freeCallback);
    }
    return CHRE_BLE_SOCKET_SEND_STATUS_FAILURE;
  }
  context->appId = mAppId;
  context->freeCallback = freeCallback;

  // PW requires the deleter's size must not exceed 4 on a 32-bit system hence
  // the parameters are wrapped in a SocketSendContext
  auto deleter = [context](pw::ByteSpan byteSpan) {
    BleSocketManager::freeSocketPacket(context->appId, byteSpan.data(),
                                       static_cast<uint16_t>(byteSpan.size()),
                                       context->freeCallback);
    // Call after enqueuing free socket packet event on CHRE's event loop queue
    // TODO(b/429237573): Support enqueueing high power events on CHRE's event
    // queue
    forceDramAccess();
    memoryFree(context);
  };
  std::optional<pw::multibuf::MultiBuf> multibuf =
      pw::multibuf::FromSpan(mTxFirstFitAllocator, byteSpan, deleter);

  // If multibuf creation is not successful, the deleter will not be used.
  if (!multibuf.has_value()) {
    LOG_OOM();
    memoryFree(context);
    if (freeCallback != nullptr) {
      BleSocketManager::freeSocketPacket(mAppId, const_cast<void *>(data),
                                         length, freeCallback);
    }
    return CHRE_BLE_SOCKET_SEND_STATUS_FAILURE;
  }
  pw::bluetooth::proxy::StatusWithMultiBuf statusWithMultiBuf;
  if (mSocketType == SocketType::L2CAP_LE_COC) {
    statusWithMultiBuf = mL2capCoc->Write(std::move(*multibuf));
    // Nothing should write to the channel except CHRE so the IsWriteAvailable
    // check should ensure that there is space in the queue
    CHRE_ASSERT(statusWithMultiBuf.status != pw::Status::Unavailable());
    if (statusWithMultiBuf.status.ok()) {
      return CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS;
    }
    return CHRE_BLE_SOCKET_SEND_STATUS_FAILURE;
  } else {
#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
    statusWithMultiBuf = mRfcommChannel->Write(std::move(*multibuf));
    if (statusWithMultiBuf.status.ok()) {
      return CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS;
    } else if (statusWithMultiBuf.status == pw::Status::Unavailable()) {
      return CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL;
    }
#endif  // CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
    return CHRE_BLE_SOCKET_SEND_STATUS_FAILURE;
  }
}

}  // namespace chre
