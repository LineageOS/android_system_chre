/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "chre/platform/host_link.h"

#include "chre/core/event_loop_manager.h"
#include "chre/platform/linux/pal_ble.h"
#include "chre/platform/shared/host_protocol_chre.h"
#include "chre/util/flatbuffers/helpers.h"

namespace chre {

void HostLink::flushMessagesSentByNanoapp(uint64_t /* appId */) {
  // (empty)
}

bool HostLink::sendMessage(const MessageToHost *message) {
  // Just drop the message since we do not have a real host to send the message
  EventLoopManagerSingleton::get()
      ->getHostCommsManager()
      .onMessageToHostComplete(message);
  return true;
}

bool HostLink::sendMessageDeliveryStatus(uint32_t /* messageSequenceNumber */,
                                         uint8_t /* errorCode */) {
  // Just drop the message delivery status since we do not have a
  // real host to send the status
  return true;
}

bool HostLink::sendBtSocketGetCapabilitiesResponse(
    uint32_t leCocNumberOfSupportedSockets, uint32_t leCocMtu,
    uint32_t rfcommNumberOfSupportedSockets, uint32_t rfcommMaxFrameSize) {
  setSocketCapabilities(
      BtSocketCapabilities{leCocNumberOfSupportedSockets, leCocMtu,
                           rfcommNumberOfSupportedSockets, rfcommMaxFrameSize});
  return true;
}

bool HostLink::sendBtSocketOpenResponse(uint64_t socketId, bool success,
                                        const char *reason) {
  setSocketOpenSuccess(success);
  setSocketOpenFailureReason(reason);
  constexpr size_t kFixedSizePortion = 52;
  ChreFlatBufferBuilder builder(kFixedSizePortion);
  HostProtocolChre::encodeBtSocketOpenResponse(builder, socketId, success,
                                               reason);
  return true;
}

bool HostLink::sendBtSocketClose(uint64_t /*socketId*/,
                                 const char * /*reason*/) {
  incrementSocketClosureCount();
  return true;
}

void HostLinkBase::sendNanConfiguration(bool enable) {
#if defined(CHRE_WIFI_SUPPORT_ENABLED) && defined(CHRE_WIFI_NAN_SUPPORT_ENABLED)
  EventLoopManagerSingleton::get()
      ->getWifiRequestManager()
      .updateNanAvailability(enable);
#else
  UNUSED_VAR(enable);
#endif
}

void HostMessageHandlers::sendFragmentResponse(uint16_t, uint32_t, uint32_t,
                                               bool) {}

void HostMessageHandlers::handleDebugDumpRequest(uint16_t) {}

void HostMessageHandlers::handleHubInfoRequest(uint16_t) {}

void HostMessageHandlers::handleLoadNanoappRequest(uint16_t, uint32_t, uint64_t,
                                                   uint32_t, uint32_t, uint32_t,
                                                   const void *, size_t,
                                                   const char *, uint32_t,
                                                   size_t, bool) {}

void HostMessageHandlers::handleNanoappListRequest(uint16_t) {}

void HostMessageHandlers::handleNanoappMessage(uint64_t, uint32_t, uint16_t,
                                               const void *, size_t, bool,
                                               uint32_t) {}

void HostMessageHandlers::handleMessageDeliveryStatus(uint32_t, uint8_t) {}

void HostMessageHandlers::handleSettingChangeMessage(fbs::Setting,
                                                     fbs::SettingState) {}

void HostMessageHandlers::handleTimeSyncMessage(int64_t) {}

void HostMessageHandlers::handleUnloadNanoappRequest(uint16_t, uint32_t,
                                                     uint64_t, bool) {}

void HostMessageHandlers::handleSelfTestRequest(uint16_t) {}

void HostMessageHandlers::handlePulseRequest() {}

void HostMessageHandlers::handleDebugConfiguration(
    const fbs::DebugConfiguration *) {}

void HostMessageHandlers::handleNanConfigurationUpdate(bool) {}

void HostMessageHandlers::handleBtSocketOpen(uint64_t,
                                             const BleL2capCocSocketData &,
                                             const char *, uint32_t) {}

void HostMessageHandlers::handleBtSocketCapabilitiesRequest() {}

void HostMessageHandlers::handleBtSocketClosed(uint64_t) {}

}  // namespace chre
