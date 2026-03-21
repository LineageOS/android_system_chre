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

#include "chre_api/chre/sensor.h"

#include <cstdint>

#include "chre/platform/log.h"
#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre/common.h"
#include "chre_api/chre/event.h"

#include "gtest/gtest.h"
#include "inc/test_util.h"
#include "test_base.h"
#include "test_event.h"
#include "test_event_queue.h"
#include "test_util.h"

namespace chre {
namespace {

// A nanoapp-to-nanoapp send event type used for tests in this file
#define NANOAPP_SEND_EVENT (CHRE_EVENT_TEST_EVENT + 0x1000)

// The data to use for sending SEND_EVENT_CONFIG
struct SendEventConfig {
  uint64_t appId;
};
CREATE_CHRE_TEST_EVENT(SEND_EVENT_CONFIG, 0);

// The data to use for sending SEND_EVENT_RESPONSE
struct SendEventResponse {
  bool success;
  // Only valid if success is true
  uint32_t value;
};
CREATE_CHRE_TEST_EVENT(SEND_EVENT_RESPONSE, 1);

// No payload.
CREATE_CHRE_TEST_EVENT(SEND_EVENT_FREE, 2);

const uint32_t kEventValue = 0x12345678;
const uint64_t kSenderAppId = 0x1234567890abcdef;
const uint64_t kTargetAppId = 0xfedcba987654321;

class App : public TestNanoapp {
 public:
  explicit App(TestNanoappInfo info = {}) : TestNanoapp(info) {}

  void handleEvent(uint32_t, uint16_t eventType,
                   const void *eventData) override {
    switch (eventType) {
      case CHRE_EVENT_TEST_EVENT: {
        auto event = static_cast<const TestEvent *>(eventData);
        switch (event->type) {
          case SEND_EVENT_CONFIG: {
            auto config = static_cast<const SendEventConfig *>(event->data);
            sendEventToTargetNanoapp(config->appId);
            break;
          }
        }
        break;
      }

      case NANOAPP_SEND_EVENT: {
        uint32_t value = *static_cast<const uint32_t *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(
            SEND_EVENT_RESPONSE,
            SendEventResponse{.success = true, .value = value});
        break;
      }
      default:
        LOGE("Unknown event 0x%" PRIx16, eventType);
        break;
    }
  }

 private:
  void sendEventToTargetNanoapp(uint64_t targetAppId) {
    chreNanoappInfo info;
    bool success = false;
    if (!chreGetNanoappInfoByAppId(targetAppId, &info)) {
      LOGE("Failed to find nanoapp info for app id=0x%" PRIx64, targetAppId);
    } else {
      uint32_t *intPtr =
          static_cast<uint32_t *>(chreHeapAlloc(sizeof(uint32_t)));
      if (intPtr == nullptr) {
        LOG_OOM();
      } else {
        *intPtr = kEventValue;
        auto freeCallback = [](uint16_t /* eventType */, void *data) {
          uint64_t appId = chreGetAppId();
          LOGI("Freeing memory here, my app id is 0x%" PRIx64, appId);
          // Confirm that the free callback is happening under the
          // sender nanoapp's context.
          CHRE_ASSERT(appId == kSenderAppId);
          if (appId == kSenderAppId) {
            chreHeapFree(data);
            TestEventQueueSingleton::get()->pushEvent(SEND_EVENT_FREE);
          }
        };
        if (!chreSendEvent(NANOAPP_SEND_EVENT, intPtr, freeCallback,
                           info.instanceId)) {
          LOGE("Failed to send event to nanoapp");
        } else {
          LOGI("Send event succeeded");
          success = true;
        }
      }
    }
    if (!success) {
      TestEventQueueSingleton::get()->pushEvent(
          SEND_EVENT_RESPONSE, SendEventResponse{.success = false, .value = 0});
    }
  }
};

TEST_F(SingleThreadTestBase, SendEvent) {
  TestNanoappInfo info1;
  info1.id = kSenderAppId;
  uint64_t appId = loadNanoapp(MakeUnique<App>(info1));
  TestNanoappInfo info2;
  info2.id = kTargetAppId;
  uint64_t appId2 = loadNanoapp(MakeUnique<App>(info2));

  SendEventConfig config{.appId = info2.id};
  sendEventToNanoapp(appId, SEND_EVENT_CONFIG, config);

  SendEventResponse response;
  waitForEvent(SEND_EVENT_RESPONSE, &response);
  ASSERT_TRUE(response.success);
  EXPECT_EQ(response.value, kEventValue);

  waitForEvent(SEND_EVENT_FREE);
}

TEST_F(MultiThreadTestBase, SendEventMultiThread) {
  TestNanoappInfo info1;
  info1.id = kSenderAppId;
  info1.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL;
  uint64_t appId = loadNanoapp(MakeUnique<App>(info1));
  TestNanoappInfo info2;
  info2.id = kTargetAppId;
  info2.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND;
  uint64_t appId2 = loadNanoapp(MakeUnique<App>(info2));

  SendEventConfig config{.appId = info2.id};
  sendEventToNanoapp(appId, SEND_EVENT_CONFIG, config);

  SendEventResponse response;
  waitForEvent(SEND_EVENT_RESPONSE, &response);
  ASSERT_TRUE(response.success);
  EXPECT_EQ(response.value, kEventValue);

  waitForEvent(SEND_EVENT_FREE);
}

}  // namespace
}  // namespace chre
