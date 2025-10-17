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

#include "chre/platform/host_link.h"
#include "chre/target_platform/host_link_base.h"

#include "chre/core/event_loop_manager.h"

#include <cstdlib>

namespace chre {

void HostLink::flushMessagesSentByNanoapp(uint64_t /* appId */) {
  // (empty)
}

bool HostLink::sendMessage(const MessageToHost *message) {
  if (!mMessageCallback) {
    return false;
  }
  mMessageCallback(message->appId, message->toHostData.messageType,
                   message->message.data(), message->message.size());
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
    uint32_t /*leCocNumberOfSupportedSockets*/, uint32_t /*leCocMtu*/,
    uint32_t /*rfcommNumberOfSupportedSockets*/,
    uint32_t /*rfcommMaxFrameSize*/) {
  return false;
}

bool HostLink::sendBtSocketOpenResponse(uint64_t /*socketId*/, bool /*success*/,
                                        const char * /*reason*/) {
  return false;
}

bool HostLink::sendBtSocketClose(uint64_t /*socketId*/,
                                 const char * /*reason*/) {
  return false;
}

void HostLinkBase::registerMessageCallback(
    const HostLinkBase::MessageCallback &callback) {
  mMessageCallback = callback;
}
}  // namespace chre
