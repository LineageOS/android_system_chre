/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "test_base.h"

#include <gtest/gtest.h>

#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/linux/platform_log.h"
#include "chre/platform/linux/task_util/task_manager.h"
#include "chre/platform/linux/thread_context.h"
#include "chre/platform/shared/init.h"
#include "chre/util/time.h"
#include "chre_api/chre/version.h"
#include "inc/test_util.h"
#include "test_util.h"

#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_containers/vector.h"
#include "pw_function/function.h"
#include "pw_span/span.h"

using pw::bluetooth::proxy::H4PacketWithH4;
using pw::bluetooth::proxy::H4PacketWithHci;

namespace chre {

/**
 * This base class initializes and runs the event loop.
 *
 * This test framework makes use of the TestEventQueue as a primary method
 * of a test execution barrier (see its documentation for details). To
 * simplify the test execution flow, it is encouraged that any communication
 * between threads (e.g. a nanoapp and the main test thread) through this
 * TestEventQueue. In this way, we can design simulation tests in a way that
 * validates an expected sequence of events in a well-defined manner.
 *
 * To avoid the test from potentially stalling, we also push a timeout event
 * to the TestEventQueue once a fixed timeout has elapsed since the start of
 * this test.
 */
void TestBase::SetUpBase(pw::span<EventLoop> eventLoops) {
  setWaitTimeout(getTimeoutNs() / 2);

  chre::PlatformLogSingleton::init();
  TaskManagerSingleton::init();
  TestEventQueueSingleton::init();
  mProxyHost.emplace(
      pw::bind_member<&MockBtOffload::sendToHost>(&mMockBtOffload),
      pw::bind_member<&MockBtOffload::sendToController>(&mMockBtOffload),
      /*le_acl_credits_to_reserve=*/2,
      /*br_edr_acl_credits_to_reserve=*/0);

  initBleSocketManager(mProxyHost.value());
  chre::initCommon(eventLoops);
  EventLoopManagerSingleton::get()->lateInit();

  auto callback = [](void *) {
    LOGE("Test timed out ...");
    TestEventQueueSingleton::get()->pushEvent(
        CHRE_EVENT_SIMULATION_TEST_TIMEOUT);
  };

  ASSERT_TRUE(mSystemTimer.init());
  ASSERT_TRUE(mSystemTimer.set(callback, nullptr /*data*/,
                               Nanoseconds(getTimeoutNs())));
}

void TestBase::TearDown() {
  mSystemTimer.cancel();
  // Free memory allocated for event on the test queue.
  TestEventQueueSingleton::get()->flush();

  chre::deinitCommon();
  TestEventQueueSingleton::deinit();
  TaskManagerSingleton::deinit();
  deleteNanoappInfos();
  unregisterAllTestNanoapps();
  chre::PlatformLogSingleton::deinit();
}

TEST_F(SingleThreadTestBase, CanLoadAndStartSingleNanoapp) {
  constexpr uint64_t kAppId = 0x0123456789abcdef;
  constexpr uint32_t kAppVersion = 0;
  constexpr uint32_t kAppPerms = 0;

  UniquePtr<Nanoapp> nanoapp = createStaticNanoapp(
      "Test nanoapp", kAppId, kAppVersion, kAppPerms, defaultNanoappStart,
      defaultNanoappHandleEvent, defaultNanoappEnd);

  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::FinishLoadingNanoapp, std::move(nanoapp),
      testFinishLoadingNanoappCallback);
  waitForEvent(CHRE_EVENT_SIMULATION_TEST_NANOAPP_LOADED);
}

TEST_F(SingleThreadTestBase, CanLoadAndStartMultipleNanoapps) {
  constexpr uint64_t kAppId1 = 0x123;
  constexpr uint64_t kAppId2 = 0x456;
  constexpr uint32_t kAppVersion = 0;
  constexpr uint32_t kAppPerms = 0;
  TestNanoappInfo info1;
  info1.name = "Test nanoapp 1";
  info1.id = kAppId1;
  info1.version = kAppVersion;
  info1.perms = kAppPerms;
  loadNanoapp(MakeUnique<TestNanoapp>(info1));
  TestNanoappInfo info2;
  info2.name = "Test nanoapp 2";
  info2.id = kAppId2;
  info2.version = kAppVersion;
  info2.perms = kAppPerms;
  loadNanoapp(MakeUnique<TestNanoapp>(info2));

  uint16_t id1;
  EXPECT_TRUE(
      getEventLoopForRequestedPriority(NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL)
          ->findNanoappInstanceIdByAppId(kAppId1, &id1));
  uint16_t id2;
  EXPECT_TRUE(
      getEventLoopForRequestedPriority(NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL)
          ->findNanoappInstanceIdByAppId(kAppId2, &id2));

  EXPECT_NE(id1, id2);
}

TEST_F(SingleThreadTestBase, methods) {
  CREATE_CHRE_TEST_EVENT(SOME_EVENT, 0);

  class App : public TestNanoapp {
   public:
    explicit App(TestNanoappInfo info) : TestNanoapp(info) {}
    bool start() override {
      LOGE("start");
      mTest = 0xc0ffee;
      return true;
    }

    void handleEvent(uint32_t /*senderInstanceId*/, uint16_t /*eventType*/,
                     const void * /**eventData*/) override {
      LOGE("handleEvent %" PRIx16, mTest);
    }

    void end() override {
      LOGE("end");
    }

   protected:
    uint32_t mTest = 0;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>(TestNanoappInfo{.id = 0x456}));

  sendEventToNanoapp(appId, SOME_EVENT);
}

// Basic test to ensure getting ID works on start and end
TEST_F(SingleThreadTestBase, GetIdOnStartAndEnd) {
  constexpr uint64_t kAppId = 0x1234567890abcdef;
  class App : public TestNanoapp {
   public:
    explicit App(TestNanoappInfo info) : TestNanoapp(info) {}
    bool start() override {
      uint64_t appId = chreGetAppId();
      uint16_t instanceId = chreGetInstanceId();
      LOGI("start: id=0x%" PRIx64 " instance=%" PRIu16, appId, instanceId);
      mInstanceId = instanceId;
      mAppId = appId;
      return kAppId == appId;
    }

    void end() override {
      uint64_t appId = chreGetAppId();
      uint16_t instanceId = chreGetInstanceId();
      LOGI("end: id=0x%" PRIx64 " instance=%" PRIu16, appId, instanceId);
      CHRE_ASSERT(mAppId == appId);
      CHRE_ASSERT(mInstanceId == instanceId);
    }

   private:
    uint64_t mAppId;
    uint16_t mInstanceId;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>(TestNanoappInfo{.id = kAppId}));
  unloadNanoapp(appId);
}

TEST_F(SingleThreadTestBase, PostEventWithNullEventIsHandledGracefully) {
  // This test verifies that calling EventLoop::postEvent with a null event
  // does not cause a crash and returns false, which is the expected behavior
  // for a failed push to the event queue.
  EventLoop &eventLoop = EventLoopManagerSingleton::get()->getEventLoop();
  bool success = eventLoop.postEvent(nullptr);
  EXPECT_FALSE(success);
}

void SingleThreadTestBase::SetUp() {
  mEventLoop.emplace();
  pw::span<EventLoop> span(&mEventLoop.value(), 1);
  TestBase::SetUpBase(span);

  mChreThread = std::thread([]() {
    registerThreadContext(&EventLoopManagerSingleton::get()->getEventLoop());
    EventLoopManagerSingleton::get()->getEventLoop().run();
  });
}

void SingleThreadTestBase::TearDown() {
  EventLoopManagerSingleton::get()->getEventLoop().stop();
  mChreThread.join();
  TestBase::TearDown();
}

template <size_t kNumEventLoops>
void MultiThreadTestBaseT<kNumEventLoops>::SetUp() {
  mEventLoops.emplace();
  pw::span<EventLoop> span(mEventLoops->data(), mEventLoops->size());
  SetUpBase(span);

  ASSERT_EQ(mChreThreads.size(), mEventLoops->size());
  for (size_t i = 0; i < mChreThreads.size(); i++) {
    mChreThreads[i] = std::thread([i, this]() {
      registerThreadContext(getEventLoop(i));
      getEventLoop(i)->run();
    });
  }
}

template <size_t kNumEventLoops>
void MultiThreadTestBaseT<kNumEventLoops>::TearDown() {
  for (EventLoop &eventLoop : *mEventLoops) {
    eventLoop.stop();
  }
  for (std::thread &chreThread : mChreThreads) {
    chreThread.join();
  }
  TestBase::TearDown();
}

TEST_F(MultiThreadTestBase, CanLoadAndStartMultiThreadNanoapp) {
  constexpr uint64_t kAppId1 = 0x0123456789abcdef;
  constexpr uint32_t kAppVersion = 0;
  constexpr uint32_t kAppPerms = 0;
  TestNanoappInfo info1;
  info1.name = "Test nanoapp 1";
  info1.id = kAppId1;
  info1.version = kAppVersion;
  info1.perms = kAppPerms;
  info1.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL;
  loadNanoapp(MakeUnique<TestNanoapp>(info1));
  EXPECT_NE(
      getEventLoopForRequestedPriority(NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL)
          ->findNanoappByAppId(kAppId1),
      nullptr);

  constexpr uint64_t kAppId2 = 0xfedcba9876543210;
  TestNanoappInfo info2;
  info2.name = "Test nanoapp 2";
  info2.id = kAppId2;
  info2.version = kAppVersion;
  info2.perms = kAppPerms;
  info2.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND;
  loadNanoapp(MakeUnique<TestNanoapp>(info2));
  EXPECT_NE(getEventLoopForRequestedPriority(
                NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND)
                ->findNanoappByAppId(kAppId2),
            nullptr);
}

// Explicitly instantiate the TestEventQueueSingleton to reduce codesize.
template class Singleton<TestEventQueue>;

}  // namespace chre
