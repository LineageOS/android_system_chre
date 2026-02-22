/*
 * Copyright (C) 2026 The Android Open Source Project
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

#include <cstdint>

#include "chre/platform/log.h"
#include "chre_api/chre/event.h"
#include "gtest/gtest.h"
#include "inc/test_util.h"
#include "test_base.h"
#include "test_event_queue.h"
#include "test_util.h"

namespace chre {
namespace {

constexpr uint32_t kMessageType = 0x87654321;
constexpr uint16_t kHostEndpoint = 0;

CREATE_CHRE_TEST_EVENT(SEND_RELIABLE_MESSAGE, 1);
CREATE_CHRE_TEST_EVENT(SEND_RELIABLE_MESSAGE_RESULT, 2);

class App : public TestNanoapp {
 public:
  explicit App(TestNanoappInfo info = {}) : TestNanoapp(info) {}

  void handleEvent(uint32_t, uint16_t eventType,
                   const void *eventData) override {
    switch (eventType) {
      case CHRE_EVENT_MESSAGE_FROM_HOST: {
        auto message = static_cast<const chreMessageFromHostData *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_MESSAGE_FROM_HOST,
                                                  message->messageType);
        break;
      }
      case CHRE_EVENT_RELIABLE_MSG_ASYNC_RESULT: {
        auto event = static_cast<const chreAsyncResult *>(eventData);
        if (event->cookie != &mCookie) {
          LOGE("Unexpected cookie: %p, expected %p", event->cookie, &mCookie);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_RELIABLE_MSG_ASYNC_RESULT,
              /* success= */ false);
        } else {
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_RELIABLE_MSG_ASYNC_RESULT, event->success);
        }
        break;
      }
      case CHRE_EVENT_TEST_EVENT: {
        auto event = static_cast<const TestEvent *>(eventData);
        switch (event->type) {
          case SEND_RELIABLE_MESSAGE:
            bool success = chreSendReliableMessageAsync(
                /* messageData= */ nullptr,
                /* messageSize= */ 0, kMessageType, kHostEndpoint,
                /* messagePermissions= */ 0,
                /* freeCallback= */ nullptr, &mCookie);
            TestEventQueueSingleton::get()->pushEvent(
                SEND_RELIABLE_MESSAGE_RESULT, success);
        }
        break;
      }
      default:
        LOGE("Unknown event 0x%" PRIx16, eventType);
        break;
    }
  }

 private:
  uint32_t mCookie;
};

/**
 * A simple test that sends a message to a nanoapp from the host and verifies
 * that it is delivered.
 *
 * @param test The TestBase object running this gTest.
 * @param requestedThreadPriority The requested thread priority of the nanoapp.
 */
void doRunHostMessageToNanoappTest(TestBase *test,
                                   int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  EventLoopManagerSingleton::get()
      ->getHostCommsManager()
      .sendMessageToNanoappFromHost(
          appId, kMessageType, kHostEndpoint, /* messageData= */ nullptr,
          /* messageSize= */ 0,
          /* isReliable= */ false, /* messageSequenceNumber= */ 0);

  uint32_t messageType;
  test->waitForEvent(CHRE_EVENT_MESSAGE_FROM_HOST, &messageType);
  EXPECT_EQ(messageType, kMessageType);
}

class HostMessageTest : public SingleThreadTestBase {};
class HostMessageTestMultiThread : public MultiThreadTestBase {};

TEST_F(HostMessageTest, HostMessageToNanoapp) {
  doRunHostMessageToNanoappTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(HostMessageTestMultiThread, HostMessageToNanoapp) {
  doRunHostMessageToNanoappTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(HostMessageTestMultiThread, HostMessageToNanoappForeground) {
  doRunHostMessageToNanoappTest(this,
                                NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

class ReliableMessageTest : public SingleThreadTestBase {};
class ReliableMessageTestMultiThread : public MultiThreadTestBase {};

void doNanoappReliableSendMessageTest(TestBase *test,
                                      int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));
  sendEventToNanoapp(appId, SEND_RELIABLE_MESSAGE);
  bool success;
  test->waitForEvent(SEND_RELIABLE_MESSAGE_RESULT, &success);
  EXPECT_TRUE(success);

  // The host link implementation automatically completes the transaction.
  test->waitForEvent(CHRE_EVENT_RELIABLE_MSG_ASYNC_RESULT, &success);
  EXPECT_TRUE(success);
}

TEST_F(ReliableMessageTest, NanoappReliableSendMessage) {
  doNanoappReliableSendMessageTest(this,
                                   NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(ReliableMessageTestMultiThread, NanoappReliableSendMessage) {
  doNanoappReliableSendMessageTest(this,
                                   NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(ReliableMessageTestMultiThread, NanoappReliableSendMessageForeground) {
  doNanoappReliableSendMessageTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

}  // namespace
}  // namespace chre
