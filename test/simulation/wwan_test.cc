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

#include "chre_api/chre/wwan.h"

#include <cstdint>

#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre/event.h"
#include "gtest/gtest.h"
#include "test_base.h"
#include "test_event.h"
#include "test_event_queue.h"
#include "test_util.h"

namespace chre {
namespace {

CREATE_CHRE_TEST_EVENT(CELL_INFO_REQUEST, 0);

class WwanTestApp : public TestNanoapp {
 public:
  explicit WwanTestApp(const TestNanoappInfo &info) : TestNanoapp(info) {}

  void handleEvent(uint32_t /* senderInstanceId */, uint16_t eventType,
                   const void *eventData) override {
    switch (eventType) {
      case CHRE_EVENT_WWAN_CELL_INFO_RESULT: {
        auto *event =
            static_cast<const struct chreWwanCellInfoResult *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(
            CHRE_EVENT_WWAN_CELL_INFO_RESULT,
            reinterpret_cast<uintptr_t>(event->cookie));
        break;
      }

      case CHRE_EVENT_TEST_EVENT: {
        auto *event = static_cast<const TestEvent *>(eventData);
        if (event->type == CELL_INFO_REQUEST) {
          uintptr_t cookie = *static_cast<const uintptr_t *>(event->data);
          bool success =
              chreWwanGetCellInfoAsync(reinterpret_cast<const void *>(cookie));
          TestEventQueueSingleton::get()->pushEvent(CELL_INFO_REQUEST, success);
        } else {
          LOGE("Unexpected test event type 0x%" PRIx16, event->type);
        }
        break;
      }
    }
  }
};

void runWwanGetCellInfoTest(TestBase *test, int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.perms = NanoappPermissions::CHRE_PERMS_WWAN;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<WwanTestApp>(info));

  uintptr_t cookie = 0x1234;
  sendEventToNanoapp(appId, CELL_INFO_REQUEST, cookie);

  bool success;
  test->waitForEvent(CELL_INFO_REQUEST, &success);
  EXPECT_TRUE(success);

  uintptr_t receivedCookie;
  test->waitForEvent(CHRE_EVENT_WWAN_CELL_INFO_RESULT, &receivedCookie);
  EXPECT_EQ(receivedCookie, cookie);

  test->unloadNanoapp(appId);
}

class WwanTest : public SingleThreadTestBase {};

TEST_F(WwanTest, GetCellInfo) {
  runWwanGetCellInfoTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

class WwanMultiThreadTest : public MultiThreadTestBase {};

TEST_F(WwanMultiThreadTest, GetCellInfo) {
  runWwanGetCellInfoTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WwanMultiThreadTest, GetCellInfoForeground) {
  runWwanGetCellInfoTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

}  // namespace
}  // namespace chre
