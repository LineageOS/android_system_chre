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

#include "message_service_test.h"

#include <gtest/gtest.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <chrono>
#include <thread>

#include "chpp/app.h"
#include "chpp/common/discovery.h"
#include "chpp/common/message.h"
#include "chpp/crc.h"
#include "chpp/log.h"
#include "chpp/macros.h"
#include "chpp/memory.h"
#include "chpp/platform/platform_link.h"
#include "chpp/platform/utils.h"
#include "chpp/services/discovery.h"
#include "chpp/services/loopback.h"
#include "chpp/services/message.h"
#include "chpp/transport.h"

namespace chpp::test {

namespace {

// Max size of payload sent to chppRxDataCb (bytes)
constexpr size_t kMaxChunkSize = 20000;

constexpr size_t kMaxPacketSize =
    kMaxChunkSize + CHPP_TRANSPORT_ENCODING_OVERHEAD_BYTES;

// State of the link layer.
struct ChppLinuxLinkState gChppLinuxLinkContext;

}  // namespace

class MessageServiceTests : public testing::Test {
 protected:
  void SetUp() override {
    chppClearTotalAllocBytes();
    memset(&gChppLinuxLinkContext, 0, sizeof(struct ChppLinuxLinkState));
    gChppLinuxLinkContext.linkEstablished = true;
    gChppLinuxLinkContext.isLinkActive = true;
    const struct ChppLinkApi *linkApi = getLinuxLinkApi();
    chppTransportInit(&mTransportContext, &mAppContext, &gChppLinuxLinkContext,
                      linkApi);
    chppAppInit(&mAppContext, &mTransportContext);

    mTransportContext.resetState = CHPP_RESET_STATE_NONE;

    int numServices = CHPP_EXPECTED_SERVICE_COUNT;

    // Make sure CHPP has a correct count of the number of registered services
    // on this platform as registered in the function
    // chppRegisterCommonServices().
    ASSERT_EQ(mAppContext.registeredServiceCount, numServices);
  }

  void TearDown() override {
    chppAppDeinit(&mAppContext);
    chppTransportDeinit(&mTransportContext);

    EXPECT_EQ(chppGetTotalAllocBytes(), 0);
  }

  ChppTransportState mTransportContext = {};
  ChppAppState mAppContext = {};
  uint8_t mBuf[kMaxPacketSize] = {};
};

TEST_F(MessageServiceTests, ServiceOpen) {
  std::thread t1(chppWorkThreadStart, &mTransportContext);
  waitForLinkSendDone();

  uint8_t ackSeq = 1;
  uint8_t seq = 0;
  uint8_t handle = CHPP_HANDLE_NEGOTIATED_RANGE_START + 3;
  uint8_t transactionID = 0;

  EXPECT_EQ(findServiceHandle(&mAppContext, "Message", &handle), true);

  openService(&mTransportContext, mBuf, ackSeq++, seq++, handle,
              transactionID++, CHPP_MESSAGE_OPEN, gChppLinuxLinkContext);

  // Cleanup
  chppWorkThreadStop(&mTransportContext);
  t1.join();
}

}  // namespace chpp::test

/************************************************
 *  Fake CHRE Functions for Testing
 ***********************************************/
static IChppMsgEndpointCallbacks *gTestCallbacks;

class TestChppMsgEndpointApi : public IChppMsgEndpointApi {
  //! @see IChppEndpointApi
  bool publishServices(const struct chreMsgServiceInfo *services,
                       size_t numServices) override;
  bool configureEndpointReadyEvents(uint64_t fromEndpointId, uint64_t hubId,
                                    uint64_t endpointId, bool enable) override;
  bool configureServiceReadyEvents(uint64_t fromEndpointId, uint64_t hubId,
                                   const char *serviceDescriptor,
                                   bool enable) override;
  bool openSession(uint64_t fromEndpointId, uint64_t hubId, uint64_t endpointId,
                   const char *serviceDescriptor) override;
  bool closeSession(uint16_t sessionId) override;
  bool openSessionComplete(uint16_t sessionId) override;
  bool sendMessage(pw::UniquePtr<std::byte[]> &&data, uint32_t messageType,
                   uint32_t messagePermissions, uint16_t sessionId,
                   uint64_t fromEndpointId) override;
} gTestChppMsgEndpointApi;

bool TestChppMsgEndpointApi::publishServices(
    const struct chreMsgServiceInfo *services, size_t numServices) {
  CHPP_LOGI("%s not yet implemented", __func__);
  // TODO(b/453756093) implement this
  UNUSED_VAR(services);
  UNUSED_VAR(numServices);
  return true;
}

bool TestChppMsgEndpointApi::configureEndpointReadyEvents(
    uint64_t /* fromEndpointId */, uint64_t hubId, uint64_t endpointId,
    bool enable) {
  CHPP_LOGI("%s: hub 0x%" PRIx64 ", endpoint 0x%" PRIx64, __func__, hubId,
            endpointId);

  if (enable) {
    gTestCallbacks->onEndpointReady(hubId, endpointId);
  }
  return true;
}

bool TestChppMsgEndpointApi::configureServiceReadyEvents(
    uint64_t /* fromEndpointId */, uint64_t hubId,
    const char *serviceDescriptor, bool enable) {
  CHPP_LOGI("%s: hub 0x%" PRIx64 ", service %s", __func__, hubId,
            serviceDescriptor);
  if (strcmp(serviceDescriptor, kTestServiceDescriptor) != 0) {
    CHPP_LOGE("%s: incorrect service", __func__);
    return false;
  }

  if (enable) {
    gTestCallbacks->onServiceReady(hubId, kTestNanoappId, serviceDescriptor);
  }
  return true;
}

bool TestChppMsgEndpointApi::openSession(uint64_t /* fromEndpointId */,
                                         uint64_t /* hubId */,
                                         uint64_t /* endpointId */,
                                         const char *serviceDescriptor) {
  CHPP_LOGI("%s: service %s", __func__, serviceDescriptor);
  if (strcmp(serviceDescriptor, kTestServiceDescriptor) != 0) {
    CHPP_LOGE("%s: incorrect service", __func__);
    return false;
  }
  struct chreMsgSessionInfo session = {
      .sessionId = kTestSessionId, .hubId = 0, .endpointId = 0, .reason = 0};
  strncpy(session.serviceDescriptor, kTestServiceDescriptor,
          sizeof(session.serviceDescriptor) - 1);
  session.serviceDescriptor[sizeof(session.serviceDescriptor) - 1] = 0;

  gTestCallbacks->onSessionOpened(session);
  return true;
}

bool TestChppMsgEndpointApi::closeSession(uint16_t sessionId) {
  CHPP_LOGI("%s: session %" PRIu16, __func__, sessionId);
  if (sessionId != kTestSessionId) {
    CHPP_LOGE("%s: incorrect session", __func__);
    return false;
  }
  struct chreMsgSessionInfo session = {
      .sessionId = kTestSessionId, .hubId = 0, .endpointId = 0, .reason = 0};
  strncpy(session.serviceDescriptor, kTestServiceDescriptor,
          sizeof(session.serviceDescriptor) - 1);
  session.serviceDescriptor[sizeof(session.serviceDescriptor) - 1] = 0;

  gTestCallbacks->onSessionClosed(session);
  return true;
}

bool TestChppMsgEndpointApi::openSessionComplete(uint16_t sessionId) {
  CHPP_LOGI("%s: session %" PRIu16, __func__, sessionId);
  if (sessionId != kTestSessionId) {
    CHPP_LOGE("%s: incorrect session", __func__);
    return false;
  }
  struct chreMsgSessionInfo session = {
      .sessionId = kTestSessionId, .hubId = 0, .endpointId = 0, .reason = 0};
  strncpy(session.serviceDescriptor, kTestServiceDescriptor,
          sizeof(session.serviceDescriptor) - 1);
  session.serviceDescriptor[sizeof(session.serviceDescriptor) - 1] = 0;

  gTestCallbacks->onSessionOpened(session);
  return true;
}

bool TestChppMsgEndpointApi::sendMessage(pw::UniquePtr<std::byte[]> &&data,
                                         uint32_t messageType,
                                         uint32_t messagePermissions,
                                         uint16_t sessionId,
                                         uint64_t /* fromEndpointId */) {
  CHPP_LOGI("%s: session %" PRIu16 ", size %zu", __func__, sessionId,
            data.size());
  if (sessionId != kTestSessionId) {
    CHPP_LOGE("%s: session ID incorrect", __func__);
    return false;
  }
  // validate data content 1:size+1
  // see message_client_test.cc createMessageData()
  bool result = true;
  for (size_t i = 0; i < data.size(); i++) {
    if ((size_t)data[i] != i + 1) {
      CHPP_LOGE("%s: content error", __func__);
      result = false;
      break;
    }
  }
  gTestCallbacks->onMessageReceived(std::move(data), messageType,
                                    messagePermissions, sessionId);
  return result;
}

void chrePalMsgSendNotification(ChppMsgCommands command,
                                const char *serviceDescriptor) {
  // echo back to client
  struct chreMsgSessionInfo session = {
      .sessionId = kTestSessionId, .hubId = 0, .endpointId = 0, .reason = 0};
  if (serviceDescriptor != nullptr) {
    strncpy(session.serviceDescriptor, serviceDescriptor,
            sizeof(session.serviceDescriptor) - 1);
    session.serviceDescriptor[sizeof(session.serviceDescriptor) - 1] = 0;
  } else {
    session.serviceDescriptor[0] = 0;
  }

  if (command == CHPP_MESSAGE_ON_SESSION_OPENED) {
    gTestCallbacks->onSessionOpened(session);
  } else if (command == CHPP_MESSAGE_ON_SESSION_CLOSED) {
    gTestCallbacks->onSessionClosed(session);
  } else if (command == CHPP_MESSAGE_ON_SESSION_OPEN_REQUEST) {
    gTestCallbacks->onSessionOpenRequest(session);
  } else if (command == CHPP_MESSAGE_ON_MESSAGE_RECEIVED) {
    // TODO(b/453756093)
    // gTestCallbacks->onMessageReceived();
  }
}

IChppMsgEndpointApi *registerChppMsgEndpoint(
    struct ChppAppState *appContext, IChppMsgEndpointCallbacks *callbacks) {
  CHPP_LOGI("%s", __func__);
  UNUSED_VAR(appContext);
  gTestCallbacks = callbacks;
  return &gTestChppMsgEndpointApi;
}
