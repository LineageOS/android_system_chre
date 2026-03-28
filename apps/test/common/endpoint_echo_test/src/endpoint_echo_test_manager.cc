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

#include "chre/util/data_flow_sink.h"
#include "chre/util/data_flow_source.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/unique_ptr.h"
#include "chre_api/chre.h"

#include "pb.h"
#include "pb_encode.h"

#include <cstring>
#include <utility>

void EndpointEchoTestService::RunNanoappToHostTest(
    const google_protobuf_Empty & /* request */,
    EndpointEchoTestService::ServerWriter<chre_rpc_ReturnStatus> &writer) {
  EndpointEchoTestManagerSingleton::get()->startTest(std::move(writer));
}

void EndpointEchoTestService::RunNanoappGetEndpointInfoTest(
    const chre_rpc_HostEndpointInfo &request,
    EndpointEchoTestService::ServerWriter<chre_rpc_ReturnStatus> &writer) {
  EndpointEchoTestManagerSingleton::get()->startTest(std::move(writer),
                                                     request);
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
  if (handleEventDataFlowTest(senderInstanceId, eventType, eventData)) {
    return;
  }

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
    EndpointEchoTestService::ServerWriter<chre_rpc_ReturnStatus> &&writer,
    chre::Optional<chre_rpc_HostEndpointInfo> hostEndpointInfo) {
  if (mNanoappToHostTestInProgress) {
    LOGE("Test already in progress");
    sendTestStatus(writer, false, "Test already in progress");
    return;
  }

  if (hostEndpointInfo.has_value()) {
    LOGD("Started endpoint info verification test");
  } else {
    LOGD("Started nanoapp-initiated message echo test");
  }

  mNanoappToHostTestInProgress = true;
  mWriter = std::move(writer);
  mHostEndpointInfo = std::move(hostEndpointInfo);
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
        } else if (mHostEndpointInfo.has_value()) {
          validateEndpointInfo(info);
          runNanoappToHostTest(TestPhase::kCloseSession);
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
        uint8_t *messageBuffer = static_cast<uint8_t *>(
            chreHeapAlloc(static_cast<uint32_t>(msg->messageSize)));
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

void EndpointEchoTestManager::sendTestStatus(
    EndpointEchoTestService::ServerWriter<chre_rpc_ReturnStatus> &writer,
    bool success, const char *errorMessage) {
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
  if (!writer.Write(status).ok()) {
    LOGE("Failed to write status message");
  }
  setPermissionForNextMessage(CHRE_MESSAGE_PERMISSION_NONE);
  writer.Finish();
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

  sendTestStatus(*mWriter, success, errorMessage);
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

void EndpointEchoTestManager::validateEndpointInfo(
    const chreMsgSessionInfo *info) {
  struct chreMsgEndpointInfo endpointInfo;
  if (!chreMsgGetEndpointInfo(info->hubId, info->endpointId, &endpointInfo)) {
    failTest("Failed to get endpoint info");
  } else if (std::strcmp(endpointInfo.name, mHostEndpointInfo->expected_name) !=
             0) {
    LOGE("Name mismatch: expected %s, actual %s",
         mHostEndpointInfo->expected_name, endpointInfo.name);
    failTest("Endpoint name mismatch");
  } else if (endpointInfo.type != mHostEndpointInfo->expected_type) {
    LOGE("Type mismatch: expected %" PRIu32 ", actual %" PRIu32,
         mHostEndpointInfo->expected_type, endpointInfo.type);
    failTest("Endpoint type mismatch");
  } else if (endpointInfo.version != mHostEndpointInfo->expected_version) {
    LOGE("Version mismatch: expected %" PRIu32 ", actual %" PRIu32,
         mHostEndpointInfo->expected_version, endpointInfo.version);
    failTest("Endpoint version mismatch");
  } else if (chreGetApiVersion() >= CHRE_API_VERSION_1_12) {
    if (std::strlen(mHostEndpointInfo->expected_name) > 0 &&
        !endpointInfo.isNameValid) {
      LOGE("Expected isNameValid=1 for Name: %s",
           mHostEndpointInfo->expected_name);
      failTest("Endpoint name is invalid");
    } else if (std::strlen(mHostEndpointInfo->expected_tag) > 0 &&
               !endpointInfo.isTagValid) {
      LOGE("Expected isTagValid=1 for Tag: %s",
           mHostEndpointInfo->expected_tag);
      failTest("Endpoint tag is invalid");
    } else if (std::strcmp(endpointInfo.tag, mHostEndpointInfo->expected_tag) !=
               0) {
      LOGE("Tag mismatch: expected %s, actual %s",
           mHostEndpointInfo->expected_tag, endpointInfo.tag);
      failTest("Endpoint tag mismatch");
    }
  }
  LOGD("Completed host endpoint info validation");
}

void EndpointEchoTestManager::closeDataFlows() {
  if (mMessageDataFlowStopped && mEchoDataFlowSinkStopped) {
    mDataFlowSink.reset();
    mVariableDataFlowSink.reset();
    mDataFlowSource.reset();
    mVariableDataFlowSource.reset();
    mMessageDataFlowStopped = false;
    mEchoDataFlowSinkStopped = false;
    mIsDataFlowSinkConfigured = false;
  }
}

bool EndpointEchoTestManager::handleEventDataFlowTest(
    uint32_t /* senderInstanceId */, uint16_t eventType,
    const void *eventData) {
  switch (eventType) {
    case CHRE_EVENT_DATA_FLOW_SINK_CREATED: {
      const auto *info = static_cast<const chreDataFlowSinkInfo *>(eventData);
      mMessageDataFlowEndpointId = info->endpointId;

      LOGI("Data flow sink created: hubId=%" PRIx64 ", endpointId=%" PRIx64
           ", dataFlowId=%" PRIu32 ", elementSize=%" PRIu32,
           info->hubId, info->endpointId, info->dataFlowId, info->elementSize);

      if (info->elementSize == CHRE_DATA_FLOW_ELEMENT_SIZE_VARIABLE) {
        return handleVariableDataFlowSinkCreated(info);
      } else {
        return handleFixedDataFlowSinkCreated(info);
      }
    }

    case CHRE_EVENT_DATA_FLOW_CREATED: {
      const auto *info =
          static_cast<const chreDataFlowCreatedInfo *>(eventData);
      LOGI("Data flow created: status=%" PRIu32 ", dataFlowId=%" PRIu32,
           info->status, info->dataFlowId);

      if (info->status != CHRE_ERROR_NONE) {
        LOGE("Failed to create echo data flow, status=%" PRIu32, info->status);
        failTest("Failed to create echo data flow");
        return true;
      }

      if (mVariableDataFlowSource.has_value()) {
        return handleVariableDataFlowCreated();
      } else if (mDataFlowSource.has_value()) {
        return handleFixedDataFlowCreated();
      }
      return true;
    }

    case CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE: {
      const auto *info =
          static_cast<const chreDataFlowSinkConfigureInfo *>(eventData);
      LOGI("Data flow sink configure done: status=%" PRIu32
           ", dataFlowId=%" PRIu32,
           info->status, info->dataFlowId);

      if (info->status != CHRE_ERROR_NONE) {
        LOGE("Failed to configure echo data flow sink, status=%" PRIu32,
             info->status);
        failTest("Failed to configure echo data flow sink");
        return true;
      }
      mIsDataFlowSinkConfigured = true;
      return true;
    }

    case CHRE_EVENT_DATA_FLOW_ALERT: {
      LOGI("Received data flow alert");
      if (!mIsDataFlowSinkConfigured) {
        LOGE("Received data flow alert before sink configure done");
        failTest("Received data flow alert before sink configure done");
        return true;
      }

      if (mVariableDataFlowSink.has_value()) {
        return handleVariableDataFlowAlert();
      } else if (mDataFlowSink.has_value()) {
        return handleFixedDataFlowAlert();
      } else {
        LOGE("No data flow sink configured");
        failTest("No data flow sink configured");
        return true;
      }
    }

    case CHRE_EVENT_DATA_FLOW_SINK_STOPPED: {
      LOGI("Data flow sink stopped");
      mEchoDataFlowSinkStopped = true;
      closeDataFlows();
      return true;
    }

    case CHRE_EVENT_DATA_FLOW_STOPPED: {
      LOGI("Data flow stopped");
      mMessageDataFlowStopped = true;
      closeDataFlows();
      return true;
    }
  }

  return false;
}

bool EndpointEchoTestManager::handleVariableDataFlowSinkCreated(
    const chreDataFlowSinkInfo *info) {
  LOGI("Creating VariableDataFlowSource");
  auto source = chre::VariableDataFlowSource::createAsync(
      CHRE_DATA_FLOW_SINK_DOMAIN_HOST_AVAILABLE, 0, 0,
      CHRE_MESSAGE_PERMISSION_NONE, 1024, 16384, "EchoVariableDataFlow");
  if (!source.ok()) {
    LOGE("Failed to create VariableDataFlowSource");
    failTest("Failed to create VariableDataFlowSource");
    return true;
  }
  mVariableDataFlowSource = std::move(source.value());

  LOGI("Creating VariableDataFlowSink");
  auto sink = chre::VariableDataFlowSink::create(info->hubId, info->dataFlowId);
  if (!sink.ok()) {
    LOGE("Failed to create VariableDataFlowSink");
    failTest("Failed to create VariableDataFlowSink");
    return true;
  }
  mVariableDataFlowSink = std::move(sink.value());
  return true;
}

bool EndpointEchoTestManager::handleFixedDataFlowSinkCreated(
    const chreDataFlowSinkInfo *info) {
  LOGI("Creating DataFlowSource");
  auto source = chre::DataFlowSource<uint8_t>::createAsync(
      CHRE_DATA_FLOW_SINK_DOMAIN_HOST_AVAILABLE, 0, 0,
      CHRE_MESSAGE_PERMISSION_NONE, 1024, 16384, "EchoDataFlow");
  if (!source.ok()) {
    LOGE("Failed to create DataFlowSource");
    failTest("Failed to create DataFlowSource");
    return true;
  }
  mDataFlowSource = std::move(source.value());

  LOGI("Creating DataFlowSink");
  auto sink =
      chre::DataFlowSink<uint8_t>::create(info->hubId, info->dataFlowId);
  if (!sink.ok()) {
    LOGE("Failed to create DataFlowSink");
    failTest("Failed to create DataFlowSink");
    return true;
  }
  mDataFlowSink = std::move(sink.value());
  return true;
}

bool EndpointEchoTestManager::handleVariableDataFlowCreated() {
  chreDataFlowSinkPolicy policy = {};
  policy.newDataAlertPolicy =
      CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_STREAMING;
  policy.overwritePolicy = CHRE_DATA_FLOW_SINK_OVERWRITE_POLICY_DISALLOWED;

  LOGI("Adding sink to VariableDataFlowSource");
  if (!mVariableDataFlowSource
           ->addSinkAsync(mVariableDataFlowSink->hubId(),
                          mMessageDataFlowEndpointId, policy)
           .ok()) {
    LOGE("Failed to add sink to VariableDataFlowSource");
    failTest("Failed to add sink to VariableDataFlowSource");
  }
  return true;
}

bool EndpointEchoTestManager::handleFixedDataFlowCreated() {
  chreDataFlowSinkPolicy policy = {};
  policy.newDataAlertPolicy =
      CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_STREAMING;
  policy.overwritePolicy = CHRE_DATA_FLOW_SINK_OVERWRITE_POLICY_DISALLOWED;

  LOGI("Adding sink to DataFlowSource");
  if (!mDataFlowSource
           ->addSinkAsync(mDataFlowSink->hubId(), mMessageDataFlowEndpointId,
                          policy)
           .ok()) {
    LOGE("Failed to add sink to DataFlowSource");
    failTest("Failed to add sink to DataFlowSource");
  }
  return true;
}

bool EndpointEchoTestManager::handleVariableDataFlowAlert() {
  auto offsetResult = mVariableDataFlowSink->getOffset();
  if (!offsetResult.ok()) {
    LOGE("Failed to get offset from data flow sink");
    failTest("Failed to get offset from data flow sink");
    return true;
  }
  uint32_t offset = offsetResult.value();
  LOGI("Data flow sink offset is %" PRIu32, offset);

  while (offset > 0) {
    auto headSizeResult = mVariableDataFlowSink->getHeadSize();
    if (headSizeResult.status().IsUnavailable()) {
      LOGI("Data flow sink head size returned UNAVAILABLE, breaking");
      break;
    } else if (!headSizeResult.ok()) {
      LOGE("Failed to get head size from data flow sink with status %d",
           static_cast<int>(headSizeResult.status().code()));
      mMessageDataFlowStopped = true;
      mEchoDataFlowSinkStopped = true;
      closeDataFlows();
      failTest("Failed to get head size from data flow sink");
      return true;
    }

    uint32_t numBytes = headSizeResult.value();
    if (numBytes == 0) {
      LOGE("Failed to handle variable data flow alert, numBytes == 0");
      failTest("Failed to handle variable data flow alert, numBytes == 0");
      return true;
    }

    auto buffer = chre::MakeUniqueArray<uint8_t[]>(numBytes);
    if (buffer.isNull()) {
      LOGE("Failed to allocate memory for popping element");
      failTest("Failed to allocate memory for popping element");
      return true;
    }

    pw::ByteSpan element(reinterpret_cast<std::byte *>(buffer.get()), numBytes);
    auto popStatus = mVariableDataFlowSink->pop(element);
    if (!popStatus.ok()) {
      LOGE("Failed to pop from data flow sink with status %d",
           static_cast<int>(popStatus.code()));
      mMessageDataFlowStopped = true;
      mEchoDataFlowSinkStopped = true;
      closeDataFlows();
      failTest("Failed to pop from data flow sink");
      return true;
    }

    LOGI("Popped %" PRIu32 " bytes from data flow sink", numBytes);

    if (!mVariableDataFlowSource.has_value()) {
      LOGE("No VariableDataFlowSource available to push");
      failTest("No VariableDataFlowSource available to push");
      return true;
    }

    if (!mVariableDataFlowSource->push(pw::span<const std::byte>(element))
             .ok()) {
      LOGE("Failed to push to VariableDataFlowSource");
      failTest("Failed to push to VariableDataFlowSource");
      return true;
    }
    LOGI("Pushed %" PRIu32 " bytes to VariableDataFlowSource", numBytes);

    auto newOffsetResult = mVariableDataFlowSink->getOffset();
    if (!newOffsetResult.ok()) {
      LOGE("Failed to get offset from data flow sink");
      failTest("Failed to get offset from data flow sink");
      return true;
    }
    offset = newOffsetResult.value();
    LOGI("Remaining offset is %" PRIu32, offset);
  }
  return true;
}

bool EndpointEchoTestManager::handleFixedDataFlowAlert() {
  auto offsetResult = mDataFlowSink->getOffset();
  if (!offsetResult.ok()) {
    LOGE("Failed to get offset from data flow sink");
    failTest("Failed to get offset from data flow sink");
    return true;
  }
  uint32_t offset = offsetResult.value();
  LOGI("Data flow sink offset is %" PRIu32, offset);

  while (offset > 0) {
    auto peekResult = mDataFlowSink->peek(offset);
    if (peekResult.status().IsUnavailable()) {
      LOGI("Data flow sink peek returned UNAVAILABLE, breaking");
      break;
    } else if (!peekResult.ok()) {
      LOGE("Failed to peek from data flow sink with status %d",
           static_cast<int>(peekResult.status().code()));
      mMessageDataFlowStopped = true;
      mEchoDataFlowSinkStopped = true;
      closeDataFlows();
      failTest("Failed to peek from data flow sink");
      return true;
    }
    pw::span<const uint8_t> data = peekResult.value();
    uint32_t numBytes = data.size();

    LOGI("Peeked %" PRIu32 " bytes from data flow sink", numBytes);

    if (!mDataFlowSource.has_value()) {
      LOGE("No DataFlowSource available to push");
      failTest("No DataFlowSource available to push");
      return true;
    }

    auto pushResult = mDataFlowSource->push(data, /* allOrNothing= */ true);
    if (!pushResult.ok()) {
      LOGE("Failed to push to DataFlowSource");
      failTest("Failed to push to DataFlowSource");
      return true;
    }
    numBytes = pushResult.value();
    LOGI("Pushed %" PRIu32 " elements to DataFlowSource", numBytes);

    if (!mDataFlowSink->release(numBytes).ok()) {
      LOGE("Failed to release from data flow sink");
      failTest("Failed to release from data flow sink");
      return true;
    }
    auto newOffsetResult = mDataFlowSink->getOffset();
    if (!newOffsetResult.ok()) {
      LOGE("Failed to get offset from data flow sink");
      failTest("Failed to get offset from data flow sink");
      return true;
    }
    offset = newOffsetResult.value();
    LOGI("Remaining offset is %" PRIu32, offset);
  }
  return true;
}
