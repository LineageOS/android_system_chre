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

#include "pb.h"
#include "pb_encode.h"

#include <cstring>

void EndpointEchoTestService::RunNanoappToHostTest(
    const google_protobuf_Empty & /* request */,
    EndpointEchoTestService::ServerWriter<chre_rpc_ReturnStatus> &writer) {
  EndpointEchoTestManagerSingleton::get()->startTest(std::move(writer));
}

bool EndpointEchoTestManager::start() {
  bool endpointSupported = (chreGetCapabilities() &
                            CHRE_CAPABILITIES_GENERIC_ENDPOINT_MESSAGES) != 0;
  if (endpointSupported) {
    chre::RpcServer::Service service = {.service = mEndpointEchoTestService,
                                        .id = 0xb157d6b46418c40b,
                                        .version = 0x01000000};
    if (!mServer.registerServices(1, &service)) {
      LOGE("Error while registering the service");
      return false;
    }

    if (!chreMsgPublishServices(&kTestEchoService, /* numServices= */ 1)) {
      LOGE("Failed to publish test echo service");
      return false;
    }
  }
  return true;
}

void EndpointEchoTestManager::end() {
  mServer.close();
}

void EndpointEchoTestManager::handleEvent(uint32_t senderInstanceId,
                                          uint16_t eventType,
                                          const void *eventData) {
  if (!mServer.handleEvent(senderInstanceId, eventType, eventData)) {
    LOGE("An RPC error occurred");
  }

  // Handle the nanoapp-initiated part of the test first. This is done before
  // the host-initiated part of the test as during the host-initiated part of
  // the test, the nanoapp acts as a simple echo service with no control
  // information.
  if (handleEventNanoappToHostTest(senderInstanceId, eventType, eventData)) {
    return;
  }

  if (handleEventHostToNanoappTest(senderInstanceId, eventType, eventData)) {
    return;
  }

  LOGE("Unexpected event type %" PRIu16, eventType);
}

void EndpointEchoTestManager::setPermissionForNextMessage(uint32_t permission) {
  mServer.setPermissionForNextMessage(permission);
}

void EndpointEchoTestManager::startTest(
    EndpointEchoTestService::ServerWriter<chre_rpc_ReturnStatus> &&writer) {
  LOGD("Started nanoapp-initiated message test");

  mNanoappToHostTestInProgress = true;
  mWriter = std::move(writer);
  mTimerHandle =
      chreTimerSet(kTestTimeout.toRawNanoseconds(), /* cookie= */ nullptr,
                   /* oneShot= */ true);
  if (mTimerHandle == CHRE_TIMER_INVALID) {
    failTest("Failed to set test timeout timer");
    return;
  }

  runNanoappToHostTest(TestPhase::kOpenSession);
}

bool EndpointEchoTestManager::handleEventNanoappToHostTest(
    uint32_t /* senderInstanceId */, uint16_t eventType,
    const void *eventData) {
  if (!mNanoappToHostTestInProgress) {
    // Only handle these events if we are in the nanoapp-initiated part of the
    // test. Otherwise, we should allow the other handlers a chance to handle
    // the event.
    return false;
  }

  switch (eventType) {
    case CHRE_EVENT_MSG_SESSION_OPENED: {
      auto *info = static_cast<const chreMsgSessionInfo *>(eventData);
      if (info->hubId != CHRE_MSG_HUB_ID_ANDROID ||
          std::strcmp(info->serviceDescriptor, kTestEchoServiceDescriptor) !=
              0) {
        failTest("Received session opened event for invalid session");
      } else {
        mSessionId = info->sessionId;
        if (mSessionId == CHRE_MSG_SESSION_ID_INVALID) {
          failTest(
              "Received a corrupted session opened event with an invalid "
              "session ID");
        } else {
          runNanoappToHostTest(TestPhase::kSendMessage);
        }
      }
      return true;
    }
    case CHRE_EVENT_MSG_SESSION_CLOSED: {
      if (mSessionId == CHRE_MSG_SESSION_ID_INVALID) {
        failTest("Session open rejected by the host");
      } else {
        auto *info = static_cast<const chreMsgSessionInfo *>(eventData);
        if (info->sessionId != mSessionId) {
          failTest("Received session closed event for invalid session");
        } else {
          mSessionId = CHRE_MSG_SESSION_ID_INVALID;
          passTest();
        }
      }
      return true;
    }
    case CHRE_EVENT_MSG_FROM_ENDPOINT: {
      auto *msg =
          static_cast<const chreMsgMessageFromEndpointData *>(eventData);
      if (msg->sessionId != mSessionId) {
        failTest("Received message from invalid session ID");
        return true;
      }
      if (msg->messageSize != sizeof(mMessageBuffer)) {
        failTest("Received message with invalid size");
        return true;
      }

      auto *message = static_cast<const uint8_t *>(msg->message);
      for (uint8_t i = 0; i < sizeof(mMessageBuffer); ++i) {
        if (message[i] != mMessageBuffer[i]) {
          failTest("Received message with invalid payload");
          return true;
        }
      }

      runNanoappToHostTest(TestPhase::kCloseSession);
      return true;
    }
    case CHRE_EVENT_TIMER: {
      if (mTimerHandle == CHRE_TIMER_INVALID) {
        LOGE("Received timer event when no timer is set");
      } else {
        mTimerHandle = CHRE_TIMER_INVALID;
        failTest("Test timed out");
      }
      return true;
    }
  }
  return false;
}

bool EndpointEchoTestManager::handleEventHostToNanoappTest(
    uint32_t /* senderInstanceId */, uint16_t eventType,
    const void *eventData) {
  switch (eventType) {
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
      return true;
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
      return true;
    }
  }
  return false;
}

void EndpointEchoTestManager::runNanoappToHostTest(TestPhase phase) {
  switch (phase) {
    case TestPhase::kOpenSession: {
      bool success = chreMsgSessionOpenAsync(CHRE_MSG_HUB_ID_ANDROID,
                                             CHRE_MSG_ENDPOINT_ID_ANY,
                                             kTestEchoServiceDescriptor);
      if (!success) {
        failTest("Failed to open session");
      }
      break;
    }
    case TestPhase::kSendMessage: {
      for (uint8_t i = 0; i < sizeof(mMessageBuffer); ++i) {
        mMessageBuffer[i] = i;
      }

      bool success = chreMsgSend(
          static_cast<void *>(mMessageBuffer), sizeof(mMessageBuffer),
          /* messageType= */ 0, mSessionId, CHRE_MESSAGE_PERMISSION_NONE,
          [](void *, size_t) {});
      if (!success) {
        failTest("Failed to send message");
      }
      break;
    }
    case TestPhase::kCloseSession: {
      bool success = chreMsgSessionCloseAsync(mSessionId);
      if (!success) {
        failTest("Failed to close session");
      }
      break;
    }
    default:
      failTest("Invalid test part");
  }
}

void EndpointEchoTestManager::sendTestStatus(bool success,
                                             const char *errorMessage) {
  if (!mWriter.has_value()) {
    LOGE("No writer available to send test status");
    return;
  }

  if (mTimerHandle != CHRE_TIMER_INVALID) {
    chreTimerCancel(mTimerHandle);
    mTimerHandle = CHRE_TIMER_INVALID;
  }

  chre_rpc_ReturnStatus status = chre_rpc_ReturnStatus_init_default;
  status.status = success;

  status.error_message.funcs.encode =
      [](pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
        const char *errorMessage = static_cast<const char *>(*arg);
        return pb_encode_tag_for_field(stream, field) &&
               pb_encode_string(stream,
                                reinterpret_cast<const uint8_t *>(errorMessage),
                                strlen(errorMessage));
      };
  status.error_message.arg = const_cast<char *>(errorMessage);

  setPermissionForNextMessage(CHRE_MESSAGE_PERMISSION_NONE);
  CHRE_ASSERT(mWriter->Write(status).ok());
  setPermissionForNextMessage(CHRE_MESSAGE_PERMISSION_NONE);
  mWriter->Finish();
  mWriter.reset();

  mNanoappToHostTestInProgress = false;

  LOGD("Finished nanoapp-initiated message test");
}

void EndpointEchoTestManager::passTest() {
  sendTestStatus(/* success= */ true, /* errorMessage= */ "");
}

void EndpointEchoTestManager::failTest(const char *errorMessage) {
  sendTestStatus(/* success= */ false, errorMessage);
}