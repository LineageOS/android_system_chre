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

#include "endpoint_echo_test_manager.h"

#include "chre/util/nanoapp/log.h"
#include "chre_api/chre.h"

#include <cstring>

bool EndpointEchoTestManager::start() {
  bool endpointSupported = (chreGetCapabilities() &
                            CHRE_CAPABILITIES_GENERIC_ENDPOINT_MESSAGES) != 0;
  if (endpointSupported) {
    bool success =
        chreMsgPublishServices(&kTestEchoService, /* numServices= */ 1);
    if (!success) {
      LOGE("Failed to publish test echo service");
    }
    return success;
  }
  return true;
}

void EndpointEchoTestManager::end() {}

void EndpointEchoTestManager::handleEvent(uint32_t /* senderInstanceId */,
                                          uint16_t eventType,
                                          const void *eventData) {
  switch (eventType) {
    case CHRE_EVENT_MESSAGE_FROM_HOST: {
      // Do nothing
      break;
    }
    case CHRE_EVENT_MSG_FROM_ENDPOINT: {
      auto *msg =
          static_cast<const chreMsgMessageFromEndpointData *>(eventData);
      if (!mOpenSession.has_value()) {
        LOGE("Received message when no session opened");
      } else if (mOpenSession->sessionId != msg->sessionId) {
        LOGE("Message from invalid session ID: expected %" PRIu16
             " received %" PRIu16,
             mOpenSession->sessionId, msg->sessionId);
      } else {
        uint8_t *messageBuffer =
            static_cast<uint8_t *>(chreHeapAlloc(msg->messageSize));
        if (msg->messageSize != 0 && messageBuffer == nullptr) {
          LOGE("Failed to allocate memory for message buffer");
        } else {
          std::memcpy(static_cast<void *>(messageBuffer),
                      const_cast<void *>(msg->message), msg->messageSize);
          bool success = chreMsgSend(
              messageBuffer, msg->messageSize, msg->messageType, msg->sessionId,
              msg->messagePermissions,
              [](void *message, size_t /* size */) { chreHeapFree(message); });
          if (!success) {
            LOGE("Echo service failed to echo message");
          }
        }
      }
      break;
    }
    case CHRE_EVENT_MSG_SESSION_OPENED: {
      [[fallthrough]];
    }
    case CHRE_EVENT_MSG_SESSION_CLOSED: {
      bool open = (eventType == CHRE_EVENT_MSG_SESSION_OPENED);
      auto *info = static_cast<const chreMsgSessionInfo *>(eventData);
      LOGD("Session %s (id=%" PRIu16 "): hub ID 0x%" PRIx64
           ", endpoint ID 0x%" PRIx64,
           open ? "opened" : "closed", info->sessionId, info->hubId,
           info->endpointId);
      if (open) {
        mOpenSession = *info;
      } else {
        mOpenSession.reset();
      }
      break;
    }
    default: {
      LOGE("Unexpected event type %" PRIu16, eventType);
      break;
    }
  }
}
