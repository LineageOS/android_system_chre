/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "chre_api/chre/re.h"

#include <cstdint>

#include "chre/core/event_loop_manager.h"
#include "chre/core/settings.h"
#include "chre/platform/context.h"
#include "chre/platform/log.h"
#include "chre/util/time.h"
#include "chre_api/chre/event.h"

#include "gtest/gtest.h"
#include "inc/test_util.h"
#include "test_base.h"
#include "test_event.h"
#include "test_event_queue.h"
#include "test_util.h"

namespace chre {

CREATE_CHRE_TEST_EVENT(START_TIMER, 0);
CREATE_CHRE_TEST_EVENT(STOP_TIMER, 1);
CREATE_CHRE_TEST_EVENT(DELAYED_CALLBACK, 2);

// TimerTest is required to access private members of the TimerPool.
class TimerTest {
 public:
  static bool hasNanoappTimers(TimerPool &pool, uint16_t instanceId) {
    return pool.hasNanoappTimers(instanceId);
  }
};

namespace {

class TimerTestSingleThread : public SingleThreadTestBase {};
class TimerTestMultiThread : public MultiThreadTestBase {};

void doSetupAndCancelPeriodicTimerTest(TestBase *test,
                                       int8_t requestedThreadPriority) {
  class App : public TestNanoapp {
   public:
    explicit App(TestNanoappInfo info = {}) : TestNanoapp(info) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_TIMER: {
          auto data = static_cast<const uint32_t *>(eventData);
          if (*data == mCookie) {
            mCount++;
            if (mCount == 3) {
              TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_TIMER);
            }
          }
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case START_TIMER: {
              uint32_t handle = chreTimerSet(10 * kOneMillisecondInNanoseconds,
                                             &mCookie, false /*oneShot*/);
              TestEventQueueSingleton::get()->pushEvent(START_TIMER, handle);
              break;
            }
            case STOP_TIMER: {
              auto handle = static_cast<const uint32_t *>(event->data);
              bool success = chreTimerCancel(*handle);
              TestEventQueueSingleton::get()->pushEvent(STOP_TIMER, success);
              break;
            }
          }
        }
      }
    }

   protected:
    const uint32_t mCookie = 123;
    int mCount = 0;
  };

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  TimerPool &timerPool = EventLoopManagerSingleton::get()->getTimerPool();

  uint16_t instanceId;
  EventLoop *eventLoop =
      test->getEventLoopForRequestedPriority(requestedThreadPriority);
  EXPECT_TRUE(eventLoop->findNanoappInstanceIdByAppId(appId, &instanceId));

  uint32_t handle;
  sendEventToNanoapp(appId, START_TIMER);
  test->waitForEvent(START_TIMER, &handle);
  EXPECT_NE(handle, CHRE_TIMER_INVALID);
  EXPECT_TRUE(TimerTest::hasNanoappTimers(timerPool, instanceId));

  test->waitForEvent(CHRE_EVENT_TIMER);

  bool success;

  // Cancelling an active timer should be successful.
  sendEventToNanoapp(appId, STOP_TIMER, handle);
  test->waitForEvent(STOP_TIMER, &success);
  EXPECT_TRUE(success);
  EXPECT_FALSE(TimerTest::hasNanoappTimers(timerPool, instanceId));

  // Cancelling an inactive time should return false.
  sendEventToNanoapp(appId, STOP_TIMER, handle);
  test->waitForEvent(STOP_TIMER, &success);
  EXPECT_FALSE(success);
}

void doCancelPeriodicTimerOnUnloadTest(TestBase *test,
                                       int8_t requestedThreadPriority) {
  class App : public TestNanoapp {
   public:
    explicit App(TestNanoappInfo info = {}) : TestNanoapp(info) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_TIMER: {
          auto data = static_cast<const uint32_t *>(eventData);
          if (*data == mCookie) {
            mCount++;
            if (mCount == 3) {
              TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_TIMER);
            }
          }
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case START_TIMER: {
              uint32_t handle = chreTimerSet(10 * kOneMillisecondInNanoseconds,
                                             &mCookie, false /*oneShot*/);
              TestEventQueueSingleton::get()->pushEvent(START_TIMER, handle);
              break;
            }
          }
        }
      }
    }

   protected:
    const uint32_t mCookie = 123;
    int mCount = 0;
  };

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  TimerPool &timerPool = EventLoopManagerSingleton::get()->getTimerPool();

  uint16_t instanceId;
  EventLoop *eventLoop =
      test->getEventLoopForRequestedPriority(requestedThreadPriority);
  EXPECT_TRUE(eventLoop->findNanoappInstanceIdByAppId(appId, &instanceId));

  uint32_t handle;
  sendEventToNanoapp(appId, START_TIMER);
  test->waitForEvent(START_TIMER, &handle);
  EXPECT_NE(handle, CHRE_TIMER_INVALID);
  EXPECT_TRUE(TimerTest::hasNanoappTimers(timerPool, instanceId));

  test->waitForEvent(CHRE_EVENT_TIMER);

  test->unloadNanoapp(appId);
  EXPECT_FALSE(TimerTest::hasNanoappTimers(timerPool, instanceId));
}

TEST_F(TimerTestSingleThread, SetupAndCancelPeriodicTimer) {
  doSetupAndCancelPeriodicTimerTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(TimerTestMultiThread, SetupAndCancelPeriodicTimer) {
  doSetupAndCancelPeriodicTimerTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(TimerTestMultiThread, SetupAndCancelPeriodicTimerForeground) {
  doSetupAndCancelPeriodicTimerTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

TEST_F(TimerTestSingleThread, CancelPeriodicTimerOnUnload) {
  doCancelPeriodicTimerOnUnloadTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(TimerTestMultiThread, CancelPeriodicTimerOnUnload) {
  doCancelPeriodicTimerOnUnloadTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(TimerTestMultiThread, CancelPeriodicTimerOnUnloadForeground) {
  doCancelPeriodicTimerOnUnloadTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void delayedCallback(uint16_t /*type*/, void *data, void * /*extraData*/) {
  EventLoop *expectedEventLoop = static_cast<EventLoop *>(data);
  EventLoop *currentEventLoop = getCurrentEventLoop();
  bool correctThread = currentEventLoop == expectedEventLoop;
  if (!correctThread) {
    LOGE("Delayed callback executed in the wrong thread: expected %p, got %p",
         expectedEventLoop, currentEventLoop);
  }
  TestEventQueueSingleton::get()->pushEvent(DELAYED_CALLBACK, correctThread);
}

void doDelayedCallbackTest(TestBase *test, int8_t requestedThreadPriority) {
  EventLoop *eventLoop =
      test->getEventLoopForRequestedPriority(requestedThreadPriority);
  Nanoseconds delay = Nanoseconds(kOneMillisecondInNanoseconds);
  TimerHandle handle = EventLoopManagerSingleton::get()->setDelayedCallback(
      SystemCallbackType::TimerPoolTick, eventLoop, delayedCallback, delay,
      eventLoop);

  bool correctThread;
  test->waitForEvent(DELAYED_CALLBACK, &correctThread);
  if (!correctThread) {
    test->printEventLoopInfo();
  }
  ASSERT_TRUE(correctThread);
}

TEST_F(TimerTestSingleThread, DelayedCallback) {
  doDelayedCallbackTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(TimerTestMultiThread, DelayedCallback) {
  doDelayedCallbackTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(TimerTestMultiThread, DelayedCallbackForeground) {
  doDelayedCallbackTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

}  // namespace
}  // namespace chre
