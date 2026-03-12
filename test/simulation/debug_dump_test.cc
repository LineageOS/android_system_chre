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

#include "chre/core/debug_dump_manager.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/linux/debug_dump_helper.h"

#include "gtest/gtest.h"
#include "test_base.h"

namespace chre {

namespace {

/** A simple nanoapp which writes one string in debug dump. */
class DebugDumpNanoapp : public TestNanoapp {
 public:
  explicit DebugDumpNanoapp(const TestNanoappInfo &info = {})
      : TestNanoapp(info) {}

  bool start() override {
    chreConfigureDebugDumpEvent(/* enable= */ true);
    return true;
  }

  void handleEvent(uint32_t /* senderInstanceId */, uint16_t eventType,
                   const void * /* eventData */) override {
    switch (eventType) {
      case CHRE_EVENT_DEBUG_DUMP: {
        chreDebugDumpLog("Debug dump from app ID 0x%" PRIx64, chreGetAppId());
        TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_DEBUG_DUMP);
        break;
      }
    }
  }

  void end() override {
    chreConfigureDebugDumpEvent(/* enable= */ false);
  }
};

TEST_F(SingleThreadTestBase, DebugDumpTest) {
  clearDebugDumpString();
  uint64_t appId = loadNanoapp(MakeUnique<DebugDumpNanoapp>());

  EventLoopManagerSingleton::get()->getDebugDumpManager().trigger();
  waitForEvent(CHRE_EVENT_DEBUG_DUMP);

  std::string debugDump = getDebugDumpStringBlocking(/* timeoutMs= */ 1000);
  char expectedString[64];
  snprintf(expectedString, sizeof(expectedString),
           "Debug dump from app ID 0x%" PRIx64, appId);
  EXPECT_NE(debugDump.find(expectedString), std::string::npos)
      << "Did not find '" << expectedString << "' in debug dump";
  snprintf(expectedString, sizeof(expectedString),
           kEventLoopDebugDumpFormatString, 0);
  EXPECT_NE(debugDump.find(expectedString), std::string::npos)
      << "Did not find '" << expectedString << "' in debug dump";
}

TEST_F(MultiThreadTestBase, DebugDumpMultiThreadedTest) {
  clearDebugDumpString();
  TestNanoappInfo info1;
  info1.id = 0x123456789abcdef;
  info1.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL;
  uint64_t appId1 = loadNanoapp(MakeUnique<DebugDumpNanoapp>(info1));
  TestNanoappInfo info2;
  info2.id = 0xfdceba987654321;
  info2.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND;
  uint64_t appId2 = loadNanoapp(MakeUnique<DebugDumpNanoapp>(info2));

  EventLoopManagerSingleton::get()->getDebugDumpManager().trigger();
  waitForEvent(CHRE_EVENT_DEBUG_DUMP);
  waitForEvent(CHRE_EVENT_DEBUG_DUMP);

  std::string debugDump = getDebugDumpStringBlocking(/* timeoutMs= */ 1000);
  char expectedString[64];
  snprintf(expectedString, sizeof(expectedString),
           "Debug dump from app ID 0x%" PRIx64, appId1);
  EXPECT_NE(debugDump.find(expectedString), std::string::npos)
      << "Did not find '" << expectedString << "' in debug dump";
  snprintf(expectedString, sizeof(expectedString),
           "Debug dump from app ID 0x%" PRIx64, appId2);
  EXPECT_NE(debugDump.find(expectedString), std::string::npos)
      << "Did not find '" << expectedString << "' in debug dump";
  snprintf(expectedString, sizeof(expectedString),
           kEventLoopDebugDumpFormatString, 0);
  EXPECT_NE(debugDump.find(expectedString), std::string::npos)
      << "Did not find '" << expectedString << "' in debug dump";
  snprintf(expectedString, sizeof(expectedString),
           kEventLoopDebugDumpFormatString, 1);
  EXPECT_NE(debugDump.find(expectedString), std::string::npos)
      << "Did not find '" << expectedString << "' in debug dump";
  if (HasFailure()) {
    LOGD("Debug dump: \n%s", debugDump.c_str());
  }
}

}  // namespace
}  // namespace chre
