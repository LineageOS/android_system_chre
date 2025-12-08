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

#include <stdlib.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "chre/core/event_loop_manager.h"
#include "chre/core/host_comms_manager.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/fragment.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

namespace chre {

void HostLink::flushMessagesSentByNanoapp(uint64_t /* appId */) {
  // We handle events immediately as they pop up, so this is not needed.
}

bool HostLink::sendMessage(const MessageToHost *message) {
  void *data_copy = nullptr;
  if (message->message.size() > 0) {
    data_copy = malloc(message->message.size());
    memcpy(data_copy, message->message.data(), message->message.size());
  }

  auto msg = new SafeChreMessageToHostData();
  msg->message = data_copy;
  msg->hostEndpoint = message->toHostData.hostEndpoint;
  msg->messageSize = message->message.size();
  msg->messageType = message->toHostData.messageType;
  msg->appId = message->appId;

  auto sim = lbs::contexthub::testing::Simulator::GetInstance();
  if (msg->messageType == lbs::contexthub::testing::kFragmentedMessageType) {
    lbs::contexthub::testing::FragmentHeader fh;
    lbs::contexthub::testing::FillFragmentHeader(*msg, &fh);

    auto fragment_vector =
        &sim->received_host_message_fragments_[fh.message_id];

    // data sanity check
    if (fragment_vector->size() != fh.index) {
      printf(
          "Error receiving message (with id %d) from nanoapp (with id %" PRIu64
          "): "
          "Expected index %zu, but instead received %d. Ignoring this message.",
          fh.message_id, msg->appId, fragment_vector->size(), fh.index);
      fragment_vector->clear();
    }

    fragment_vector->push_back(*msg);

    if (!fh.is_last_fragment) {
      return true;
    }

    // this is the last fragment. Combine the fragments and process the message.
    *msg =
        lbs::contexthub::testing::CombineHostMessageFragments(*fragment_vector);
    fragment_vector->clear();
  }

  sim->AddHostMessage(msg);

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

void HostLinkBase::sendNanConfiguration(bool enable) {
#if defined(CHRE_WIFI_SUPPORT_ENABLED) && defined(CHRE_WIFI_NAN_SUPPORT_ENABLED)
  EventLoopManagerSingleton::get()
      ->getWifiRequestManager()
      .updateNanAvailability(enable);
#else
  UNUSED_VAR(enable);
#endif
}

}  // namespace chre