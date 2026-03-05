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

#include <cinttypes>
#include <cstdint>

#include "chre/core/ble_l2cap_coc_socket_data.h"
#include "chre/core/bt_socket_data.h"
#include "chre/platform/mutex.h"
#include "chre/platform/platform_bt_socket_resources.h"
#include "chre/platform/platform_bt_socket_types.h"
#include "chre/util/array_queue.h"
#include "chre/util/non_copyable.h"
#include "chre/util/unique_ptr.h"
#include "chre/variant/config.h"

#include "pw_allocator/first_fit.h"
#include "pw_allocator/synchronized_allocator.h"
#include "pw_bluetooth_proxy/l2cap_coc.h"
#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
#include "pw_bluetooth_proxy/rfcomm/rfcomm_manager.h"
#endif // CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
#include "pw_multibuf/allocator.h"
#include "pw_multibuf/multibuf.h"
#include "pw_multibuf/simple_allocator.h"
#include "pw_status/status.h"

namespace chre {

/**
 * AOC-specific implementation of a BT socket.
 */
class PlatformBtSocketBase : public NonCopyable {
 public:
  PlatformBtSocketBase(const BleL2capCocSocketData &socketData,
                       PlatformBtSocketResources &platformBtSocketResources);

#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
  PlatformBtSocketBase(const BtRfcommChannelSocketData &socketData,
                       PlatformBtSocketResources &platformBtSocketResources);
#endif

  /**
   * Callback to be invoked on Rx SDUs.
   *
   * @see pw::bluetooth::proxy::ProxyHost::AcquireL2capCoc()
   *
   * NOTE: this callback will not be invoked from the CHRE thread. It is
   * expected that the caller invokes DramVoteClient::incrementDramVoteCount()
   * and DramVoteClient::decrementDramVoteCount() around use of this function.
   */
  void handleRxSocketPacket(pw::multibuf::MultiBuf &&payload);

  /**
   * Callback to be invoked when a LE COC socket event is received.
   *
   * @see pw::bluetooth::proxy::ProxyHost::AcquireL2capCoc()
   *
   * NOTE: this callback will not be invoked from the CHRE thread. It is
   * expected that the caller invokes DramVoteClient::incrementDramVoteCount()
   * and DramVoteClient::decrementDramVoteCount() around use of this function.
   */
  void handleL2capSocketEvent(pw::bluetooth::proxy::L2capChannelEvent event);

#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
  /**
   * Callback to be invoked when a RFCOMM socket event is received.
   *
   * @see pw::bluetooth::proxy::RfcommManager::AcquireRfcommChannel()
   *
   * NOTE: this callback will not be invoked from the CHRE thread. It is
   * expected that the caller invokes DramVoteClient::incrementDramVoteCount()
   * and DramVoteClient::decrementDramVoteCount() around use of this function.
   */
  void handleRfcommSocketEvent(pw::bluetooth::proxy::rfcomm::RfcommEvent event);
#endif // CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED

 protected:
  uint64_t mId;

  SocketType mSocketType;

  // Multibuf Rx allocators

  static constexpr uint16_t kRxMultiBufAreaSize = 1024 * 24;

  static constexpr uint16_t kRxMultiBufMetaDataSize = 1024 * 12;

  // Multibuf v1 requires significant meta data
  static constexpr uint8_t kMaxRxMultibufs = kRxMultiBufMetaDataSize / 512;

  static constexpr uint16_t kTxMultiBufMetaDataSize =
      CHRE_BLE_SOCKET_TX_MULTIBUF_METADATA_SIZE;

  std::array<std::byte, kRxMultiBufAreaSize> mRxMultibufArea{};

  std::array<std::byte, kRxMultiBufMetaDataSize> mRxMultibufMetaData{};

  pw::allocator::FirstFitAllocator<pw::allocator::FirstFitBlock<uintptr_t>>
      mRxFirstFitAllocator{mRxMultibufMetaData};

  pw::allocator::SynchronizedAllocator<pw::sync::Mutex> mRxSyncAllocator{
      mRxFirstFitAllocator};

  // Allocator used for Rx data received from the BT socket.
  pw::multibuf::SimpleAllocator mRxSimpleAllocator{mRxMultibufArea,
                                                   mRxSyncAllocator};

  /**
   * Tracks packets received from the socket. Stores a packet MultiBuf until the
   * nanoapp has received the packet. Destroying the MultiBuf before this can
   * result in loss of the socket packet data.
   *
   * NOTE: Initialization order is important. Rx socket packet MultiBufs should
   * be destroyed before destroying Rx allocator.
   */
  ArrayQueue<pw::multibuf::MultiBuf, kMaxRxMultibufs> mRxSocketPackets;

  Mutex mRxSocketPacketsMutex;

  // PW L2CAP COC utility used for interacting with the BT socket.
  std::optional<pw::bluetooth::proxy::L2capCoc> mL2capCoc;

#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
  // PW RFCOMM Channel utility used for interacting with the BT socket.
  std::optional<pw::bluetooth::proxy::rfcomm::RfcommChannel> mRfcommChannel;
#endif

  std::array<std::byte, kTxMultiBufMetaDataSize> mTxMultibufMetaData{};

  pw::allocator::FirstFitAllocator<pw::allocator::FirstFitBlock<uintptr_t>>
      mTxFirstFitAllocator{mTxMultibufMetaData};
};

}  // namespace chre
