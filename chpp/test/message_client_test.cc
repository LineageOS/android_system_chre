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
#include "chpp/clients/discovery.h"
#include "chpp/clients/message.h"
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
#include "chpp/transport.h"
#include "chre/platform/shared/pal_system_api.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/libc_allocator.h"
#include "pw_allocator/unique_ptr.h"
#include "pw_function/function.h"

namespace chpp::test {

namespace {

// Max size of payload sent to chppRxDataCb (bytes)
constexpr size_t kMaxChunkSize = 20000;

constexpr size_t kMaxPacketSize =
    kMaxChunkSize + CHPP_TRANSPORT_ENCODING_OVERHEAD_BYTES;

// Number of services
constexpr int kServiceCount = CHPP_EXPECTED_SERVICE_COUNT + 1;

// State of the link layer.
// struct ChppLinuxLinkState gChppLinuxLinkContext;

constexpr uint16_t kNumCommands = 1;

struct ClientState {
  struct ChppEndpointState chppClientState;
  struct ChppOutgoingRequestState outReqStates[kNumCommands];
  bool resetNotified;
  bool matchNotified;
};

// Service
struct ServiceState {
  struct ChppEndpointState chppServiceState;
  struct ChppIncomingRequestState inReqStates[kNumCommands];
  bool resetNotified;
};

}  // namespace

class MessageClientTests : public testing::Test {
 protected:
  void SetUp() override {
    chppClearTotalAllocBytes();
    memset(&mServiceLinkContext, 0, sizeof(mServiceLinkContext));
    memset(&mClientLinkContext, 0, sizeof(mClientLinkContext));

    // services
    mServiceLinkContext.linkThreadName = "Host Link";
    mServiceLinkContext.workThreadName = "Host worker";
    // mServiceLinkContext.manualSendCycle = true;
    // mServiceLinkContext.linkEstablished = true;
    mServiceLinkContext.isLinkActive = true;
    mServiceLinkContext.remoteLinkState = &mClientLinkContext;
    mServiceLinkContext.rxInRemoteEndpointWorker = false;

    // mServiceTransportContext.resetState = CHPP_RESET_STATE_NONE;

    // clients
    mClientLinkContext.linkThreadName = "CHRE Link";
    mClientLinkContext.workThreadName = "CHRE worker";
    mClientLinkContext.isLinkActive = true;
    mClientLinkContext.remoteLinkState = &mServiceLinkContext;
    mClientLinkContext.rxInRemoteEndpointWorker = false;

    // No default clients/services.
    struct ChppClientServiceSet set;
    memset(&set, 0, sizeof(set));

    const struct ChppLinkApi *linkApi = getLinuxLinkApi();

    // init client side
    set.messageClient = 1;
    chppTransportInit(&mClientTransportContext, &mClientAppContext,
                      &mClientLinkContext, linkApi);
    chppAppInitWithClientServiceSet(&mClientAppContext,
                                    &mClientTransportContext, set);

    // init service side
    set.messageClient = 0;
    set.messageService = 1;
    chppTransportInit(&mServiceTransportContext, &mServiceAppContext,
                      &mServiceLinkContext, linkApi);
    chppAppInitWithClientServiceSet(&mServiceAppContext,
                                    &mServiceTransportContext, set);
  }

  void TearDown() override {
    // Deinit client side.
    chppAppDeinit(&mClientAppContext);
    chppTransportDeinit(&mClientTransportContext);

    // Deinit service side.
    chppAppDeinit(&mServiceAppContext);
    chppTransportDeinit(&mServiceTransportContext);

    EXPECT_EQ(chppGetTotalAllocBytes(), 0);
  }

  // Client side.
  ChppLinuxLinkState mClientLinkContext = {};
  ChppTransportState mClientTransportContext = {};
  ChppAppState mClientAppContext = {};
  ClientState mClientState;

  // Service side
  ChppLinuxLinkState mServiceLinkContext = {};
  ChppTransportState mServiceTransportContext = {};
  ChppAppState mServiceAppContext = {};
  ServiceState mServiceState;

  //  ChppTransportState mTransportContext = {};
  //  ChppAppState mAppContext = {};
  uint8_t mBuf[kMaxPacketSize] = {};
};

class TestChppMsgEndpointCallbacks : public IChppMsgEndpointCallbacks {
  void onEndpointInitialized(
      const struct chreMsgEndpointInfo &endpointInfo) override;
  void onEndpointReady(uint64_t hubId, uint64_t endpointId) override;
  void onServiceReady(uint64_t hubId, uint64_t endpointId,
                      const char *serviceDescriptor) override;
  void onSessionOpened(const struct chreMsgSessionInfo &session) override;
  void onSessionClosed(const struct chreMsgSessionInfo &session) override;
  void onSessionOpenRequest(const struct chreMsgSessionInfo &session) override;
  void onMessageReceived(pw::UniquePtr<std::byte[]> &&data,
                         uint32_t messageType, uint32_t messagePermissions,
                         uint16_t sessionId) override;
};

namespace {

TestChppMsgEndpointCallbacks gCallbacks;
IChppMsgEndpointApi *gApi;
uint16_t gSessionId = CHRE_MSG_SESSION_ID_INVALID;
bool gMessageVerified = false;
uint64_t gHubId = 0;
uint64_t gEndpointId = 0;

constexpr uint64_t kResetWaitTimeMs = 10000;
constexpr uint64_t kDiscoveryWaitTimeMs = 10000;
constexpr size_t kMessageSize = 5;

void workThread(void *transportState) {
  ChppTransportState *state = static_cast<ChppTransportState *>(transportState);

  auto linkContext =
      static_cast<struct ChppLinuxLinkState *>(state->linkContext);

  pthread_setname_np(pthread_self(), linkContext->workThreadName);

  chppWorkThreadStart(state);
}

// Creates a message with data from 1 to messageSize
pw::UniquePtr<std::byte[]> createMessageData(
    pw::allocator::Allocator &allocator, size_t messageSize) {
  pw::UniquePtr<std::byte[]> messageData =
      allocator.MakeUniqueArray<std::byte>(messageSize);
  EXPECT_NE(messageData.get(), nullptr);
  for (size_t i = 0; i < messageSize; ++i) {
    messageData[i] = static_cast<std::byte>(i + 1);
  }
  return messageData;
}

}  // namespace

void TestChppMsgEndpointCallbacks::onEndpointInitialized(
    const struct chreMsgEndpointInfo &endpointInfo) {
  CHPP_LOGI("%s: hub 0x%" PRIx64 ", endpoint 0x%" PRIx64, __func__,
            endpointInfo.hubId, endpointInfo.endpointId);
}

void TestChppMsgEndpointCallbacks::onEndpointReady(uint64_t hubId,
                                                   uint64_t endpointId) {
  CHPP_LOGI("%s: hub 0x%" PRIx64 ", endpoint 0x%" PRIx64, __func__, hubId,
            endpointId);
  gHubId = hubId;
  gEndpointId = endpointId;
}

void TestChppMsgEndpointCallbacks::onServiceReady(
    uint64_t hubId, uint64_t endpointId, const char *serviceDescriptor) {
  CHPP_LOGI("%s: hub 0x%" PRIx64 ", service %s", __func__, hubId,
            serviceDescriptor);
  gHubId = hubId;
  gEndpointId = endpointId;
}

void TestChppMsgEndpointCallbacks::onSessionOpened(
    const struct chreMsgSessionInfo &session) {
  CHPP_LOGI("%s: session %" PRIu16, __func__, session.sessionId);
  gSessionId = session.sessionId;
}

void TestChppMsgEndpointCallbacks::onSessionClosed(
    const struct chreMsgSessionInfo &session) {
  CHPP_LOGI("%s: session %" PRIu16, __func__, session.sessionId);
  if (gSessionId != session.sessionId) {
    CHPP_LOGE("%s: incorrect session %" PRIu16, __func__, session.sessionId);
  }
  gSessionId = CHRE_MSG_SESSION_ID_INVALID;
}

void TestChppMsgEndpointCallbacks::onSessionOpenRequest(
    const struct chreMsgSessionInfo &session) {
  CHPP_LOGI("%s: session %" PRIu16, __func__, session.sessionId);
  if (strcmp(session.serviceDescriptor, kTestServiceDescriptor) == 0) {
    gApi->openSessionComplete(session.sessionId);
  } else {
    gApi->closeSession(session.sessionId);
  }
}

void TestChppMsgEndpointCallbacks::onMessageReceived(
    pw::UniquePtr<std::byte[]> &&message, uint32_t messageType,
    uint32_t messagePermissions, uint16_t sessionId) {
  CHPP_LOGI("%s: session %" PRIu16, __func__, sessionId);
  UNUSED_VAR(messageType);
  UNUSED_VAR(messagePermissions);
  bool result = true;
  for (size_t i = 0; i < message.size(); i++) {
    if ((size_t)message[i] != i + 1) {
      CHPP_LOGE("%s: content error", __func__);
      result = false;
      break;
    }
  }
  gMessageVerified = result;
}

TEST_F(MessageClientTests, DiscoveryTest) {
  std::thread t1(workThread, &mClientTransportContext);
  std::this_thread::sleep_for(std::chrono::milliseconds(450));
  std::thread t2(workThread, &mServiceTransportContext);

  mClientLinkContext.linkEstablished = true;
  mServiceLinkContext.linkEstablished = true;

  EXPECT_TRUE(chppTransportWaitForResetComplete(&mClientTransportContext,
                                                kResetWaitTimeMs));
  EXPECT_TRUE(chppTransportWaitForResetComplete(&mServiceTransportContext,
                                                kResetWaitTimeMs));

  EXPECT_TRUE(
      chppWaitForDiscoveryComplete(&mClientAppContext, kDiscoveryWaitTimeMs));

  int numServices = 1;
  EXPECT_EQ(mClientAppContext.discoveredServiceCount, numServices);
  EXPECT_EQ(mClientAppContext.matchedClientCount, numServices);

  // clean up
  chppWorkThreadStop(&mClientTransportContext);
  chppWorkThreadStop(&mServiceTransportContext);
  t1.join();
  t2.join();
}

TEST_F(MessageClientTests, OpenSessionTest) {
  std::thread t1(workThread, &mClientTransportContext);
  std::this_thread::sleep_for(std::chrono::milliseconds(450));
  std::thread t2(workThread, &mServiceTransportContext);

  mClientLinkContext.linkEstablished = true;
  mServiceLinkContext.linkEstablished = true;

  EXPECT_TRUE(chppTransportWaitForResetComplete(&mClientTransportContext,
                                                kResetWaitTimeMs));
  EXPECT_TRUE(chppTransportWaitForResetComplete(&mServiceTransportContext,
                                                kResetWaitTimeMs));

  gApi = registerChppMsgEndpoint(nullptr, &gCallbacks);
  ASSERT_NE(gApi, nullptr);

  gSessionId = CHRE_MSG_SESSION_ID_INVALID;
  bool success = gApi->openSession(kFromEndpointId, kChreMessageHubId,
                                   kTestNanoappId, kTestServiceDescriptor);
  ASSERT_TRUE(success);

  // check onSessionOpened() callback
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  ASSERT_EQ(gSessionId, kTestSessionId);

  success = gApi->closeSession(kTestSessionId);
  ASSERT_TRUE(success);

  // check onSessionClosed() callback
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  ASSERT_EQ(gSessionId, CHRE_MSG_SESSION_ID_INVALID);

  // clean up
  chppWorkThreadStop(&mClientTransportContext);
  chppWorkThreadStop(&mServiceTransportContext);
  t1.join();
  t2.join();
}

TEST_F(MessageClientTests, SendMessageTest) {
  std::thread t1(workThread, &mClientTransportContext);
  std::this_thread::sleep_for(std::chrono::milliseconds(450));
  std::thread t2(workThread, &mServiceTransportContext);

  mClientLinkContext.linkEstablished = true;
  mServiceLinkContext.linkEstablished = true;

  EXPECT_TRUE(chppTransportWaitForResetComplete(&mClientTransportContext,
                                                kResetWaitTimeMs));
  EXPECT_TRUE(chppTransportWaitForResetComplete(&mServiceTransportContext,
                                                kResetWaitTimeMs));

  gApi = registerChppMsgEndpoint(nullptr, &gCallbacks);
  ASSERT_NE(gApi, nullptr);

  // Create the message
  pw::allocator::LibCAllocator allocator = pw::allocator::GetLibCAllocator();
  pw::UniquePtr<std::byte[]> messageData =
      createMessageData(allocator, kMessageSize);

  gMessageVerified = false;
  bool success = gApi->sendMessage(std::move(messageData), /* messageType */ 0,
                                   /* messagePermission */ 0, kTestSessionId,
                                   kFromEndpointId);
  ASSERT_TRUE(success);

  // check onMessageReceived() callback
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  ASSERT_TRUE(gMessageVerified);

  // clean up
  chppWorkThreadStop(&mClientTransportContext);
  chppWorkThreadStop(&mServiceTransportContext);
  t1.join();
  t2.join();
}

TEST_F(MessageClientTests, SessionRequestTest) {
  std::thread t1(workThread, &mClientTransportContext);
  std::this_thread::sleep_for(std::chrono::milliseconds(450));
  std::thread t2(workThread, &mServiceTransportContext);

  mClientLinkContext.linkEstablished = true;
  mServiceLinkContext.linkEstablished = true;

  EXPECT_TRUE(chppTransportWaitForResetComplete(&mClientTransportContext,
                                                kResetWaitTimeMs));
  EXPECT_TRUE(chppTransportWaitForResetComplete(&mServiceTransportContext,
                                                kResetWaitTimeMs));

  gApi = registerChppMsgEndpoint(nullptr, &gCallbacks);
  ASSERT_NE(gApi, nullptr);

  gSessionId = CHRE_MSG_SESSION_ID_INVALID;
  chrePalMsgSendNotification(CHPP_MESSAGE_ON_SESSION_OPEN_REQUEST,
                             kTestServiceDescriptor);

  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  ASSERT_EQ(gSessionId, kTestSessionId);

  chrePalMsgSendNotification(CHPP_MESSAGE_ON_SESSION_OPEN_REQUEST, nullptr);

  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  ASSERT_EQ(gSessionId, CHRE_MSG_SESSION_ID_INVALID);

  // clean up
  chppWorkThreadStop(&mClientTransportContext);
  chppWorkThreadStop(&mServiceTransportContext);
  t1.join();
  t2.join();
}

TEST_F(MessageClientTests, EndpointReadyTest) {
  std::thread t1(workThread, &mClientTransportContext);
  std::this_thread::sleep_for(std::chrono::milliseconds(450));
  std::thread t2(workThread, &mServiceTransportContext);

  mClientLinkContext.linkEstablished = true;
  mServiceLinkContext.linkEstablished = true;

  EXPECT_TRUE(chppTransportWaitForResetComplete(&mClientTransportContext,
                                                kResetWaitTimeMs));
  EXPECT_TRUE(chppTransportWaitForResetComplete(&mServiceTransportContext,
                                                kResetWaitTimeMs));

  gApi = registerChppMsgEndpoint(nullptr, &gCallbacks);
  ASSERT_NE(gApi, nullptr);

  gHubId = 0;
  gEndpointId = 0;
  gApi->configureEndpointReadyEvents(kFromEndpointId, kChreMessageHubId,
                                     kTestNanoappId, true);

  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  ASSERT_EQ(gHubId, kChreMessageHubId);
  ASSERT_EQ(gEndpointId, kTestNanoappId);

  // clean up
  chppWorkThreadStop(&mClientTransportContext);
  chppWorkThreadStop(&mServiceTransportContext);
  t1.join();
  t2.join();
}

}  // namespace chpp::test
