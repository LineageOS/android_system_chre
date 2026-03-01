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

#include <cstdint>

#include "chre/core/event_loop_manager.h"
#include "chre/core/settings.h"
#include "chre/platform/linux/pal_wifi.h"
#include "chre/platform/log.h"
#include "chre/util/nanoapp/app_id.h"
#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre/event.h"
#include "chre_api/chre/re.h"
#include "chre_api/chre/wifi.h"
#include "gtest/gtest.h"
#include "test_base.h"
#include "test_event.h"
#include "test_event_queue.h"
#include "test_util.h"

namespace chre {
namespace {

template <class TestBaseClass>
// WifiTimeoutTest needs to set timeout more than the max waitForEvent()
// should process (Currently it is
// WifiCanDispatchSecondScanRequestInQueueAfterFirstTimeout). If not,
// waitForEvent will timeout before actual timeout happens in CHRE, making us
// unable to observe how system handles timeout.
class WifiTimeoutTest : public TestBaseClass {
 protected:
  uint64_t getTimeoutNs() const override {
    return 3 * CHRE_TEST_WIFI_SCAN_RESULT_TIMEOUT_NS;
  }
};

class WifiTimeoutTestSingleThread
    : public WifiTimeoutTest<SingleThreadTestBase> {};
class WifiTimeoutTestMultiThread : public WifiTimeoutTest<MultiThreadTestBase> {
};

CREATE_CHRE_TEST_EVENT(SCAN_REQUEST, 20);
CREATE_CHRE_TEST_EVENT(REQUEST_TIMED_OUT, 21);

void doWifiScanRequestTimeoutTest(TestBase *test,
                                  int8_t requestedThreadPriority) {
  class ScanTestNanoapp : public TestNanoapp {
   public:
    explicit ScanTestNanoapp(const TestNanoappInfo &info)
        : TestNanoapp(setPerms(info)) {}

    static TestNanoappInfo setPerms(TestNanoappInfo info) {
      info.perms |= NanoappPermissions::CHRE_PERMS_WIFI;
      return info;
    }

    bool start() override {
      mRequestTimer = CHRE_TIMER_INVALID;
      return true;
    }

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (mRequestTimer != CHRE_TIMER_INVALID) {
            chreTimerCancel(mRequestTimer);
            mRequestTimer = CHRE_TIMER_INVALID;
          }
          if (event->success) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_ASYNC_RESULT,
                *(static_cast<const uint32_t *>(event->cookie)));
          }
          break;
        }

        case CHRE_EVENT_WIFI_SCAN_RESULT: {
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_WIFI_SCAN_RESULT);
          break;
        }

        case CHRE_EVENT_TIMER: {
          TestEventQueueSingleton::get()->pushEvent(REQUEST_TIMED_OUT);
          mRequestTimer = CHRE_TIMER_INVALID;
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SCAN_REQUEST:
              bool success = false;
              mCookie = *static_cast<uint32_t *>(event->data);
              if (chreWifiRequestScanAsyncDefault(&mCookie)) {
                mRequestTimer =
                    chreTimerSet(CHRE_TEST_WIFI_SCAN_RESULT_TIMEOUT_NS, nullptr,
                                 true /* oneShot */);
                success = mRequestTimer != CHRE_TIMER_INVALID;
              }
              TestEventQueueSingleton::get()->pushEvent(SCAN_REQUEST, success);
              break;
          }
          break;
        }
      }
    }

   protected:
    uint32_t mCookie;
    uint32_t mRequestTimer;
  };

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<ScanTestNanoapp>(info));
  constexpr uint32_t timeOutCookie = 0xdead;
  chrePalWifiEnableResponse(PalWifiAsyncRequestTypes::SCAN,
                            false /* enableResponse */);
  sendEventToNanoapp(appId, SCAN_REQUEST, timeOutCookie);

  bool success;
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);

  test->waitForEvent(REQUEST_TIMED_OUT);

  // Make sure that we can still request scan after a timed out
  // request.
  constexpr uint32_t successCookie = 0x0101;
  chrePalWifiEnableResponse(PalWifiAsyncRequestTypes::SCAN,
                            true /* enableResponse */);
  sendEventToNanoapp(appId, SCAN_REQUEST, successCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);
  test->waitForEvent(CHRE_EVENT_WIFI_SCAN_RESULT);

  test->unloadNanoapp(appId);
}

TEST_F(WifiTimeoutTestSingleThread, WifiScanRequestTimeoutTest) {
  doWifiScanRequestTimeoutTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTimeoutTestMultiThread, WifiScanRequestTimeoutTest) {
  doWifiScanRequestTimeoutTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTimeoutTestMultiThread, WifiScanRequestTimeoutTestForeground) {
  doWifiScanRequestTimeoutTest(this,
                               NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doWifiCanDispatchQueuedRequestAfterOneTimeoutTest(
    TestBase *test, int8_t requestedThreadPriority) {
  constexpr uint64_t kAppOneId = makeExampleNanoappId(1);
  constexpr uint64_t kAppTwoId = makeExampleNanoappId(2);
  constexpr uint8_t kNanoappNum = 2;
  // receivedTimeout is shared across apps and must be static.
  // But we want it initialized each time the test is executed.
  static uint8_t receivedTimeout;
  receivedTimeout = 0;
  constexpr uint32_t timeOutCookie = 0xdead;
  constexpr uint32_t successCookie = 0x0101;

  class ScanTestNanoapp : public TestNanoapp {
   public:
    explicit ScanTestNanoapp(const TestNanoappInfo &info)
        : TestNanoapp(setPerms(info)) {}

    static TestNanoappInfo setPerms(TestNanoappInfo info) {
      info.perms |= NanoappPermissions::CHRE_PERMS_WIFI;
      return info;
    }

    bool start() override {
      mRequestTimer = CHRE_TIMER_INVALID;
      return true;
    }

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      size_t index;
      if (id() == kAppOneId) {
        index = 0;
      } else {
        index = 1;
      }
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (mRequestTimer != CHRE_TIMER_INVALID) {
            chreTimerCancel(mRequestTimer);
            mRequestTimer = CHRE_TIMER_INVALID;
          }
          if (event->success) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_ASYNC_RESULT,
                *(static_cast<const uint32_t *>(event->cookie)));
          }
          break;
        }

        case CHRE_EVENT_WIFI_SCAN_RESULT: {
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_WIFI_SCAN_RESULT);
          break;
        }

        case CHRE_EVENT_TIMER: {
          if (eventData == &mCookie) {
            receivedTimeout++;
            mRequestTimer = CHRE_TIMER_INVALID;
          }
          if (receivedTimeout == 2) {
            TestEventQueueSingleton::get()->pushEvent(REQUEST_TIMED_OUT);
          }
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SCAN_REQUEST:
              bool success = false;
              mCookie = *static_cast<uint32_t *>(event->data);
              if (chreWifiRequestScanAsyncDefault(&mCookie)) {
                // Stagger timeouts by index (e.g., 1x for App 1, 2x for App 2)
                // to simulate CHRE’s sequential queuing. Since CHRE only starts
                // the timer for App 2 after App 1 expires, this manual delay
                // mimics that behavior without requiring production code
                // changes to CHRE’s internal timeout events.
                uint64_t timeout =
                    (mCookie == successCookie)
                        ? CHRE_TEST_WIFI_SCAN_RESULT_TIMEOUT_NS
                        : (index + 1) * CHRE_TEST_WIFI_SCAN_RESULT_TIMEOUT_NS;
                mRequestTimer =
                    chreTimerSet(timeout, &mCookie, true /* oneShot */);
                success = mRequestTimer != CHRE_TIMER_INVALID;
              }
              TestEventQueueSingleton::get()->pushEvent(SCAN_REQUEST, success);
              break;
          }
          break;
        }
      }
    }

   protected:
    uint32_t mCookie;
    uint32_t mRequestTimer;
  };

  TestNanoappInfo info1;
  info1.id = kAppOneId;
  info1.requestedThreadPriority = requestedThreadPriority;
  uint64_t firstAppId = test->loadNanoapp(MakeUnique<ScanTestNanoapp>(info1));
  TestNanoappInfo info2;
  info2.id = kAppTwoId;
  info2.requestedThreadPriority = requestedThreadPriority;
  uint64_t secondAppId = test->loadNanoapp(MakeUnique<ScanTestNanoapp>(info2));

  chrePalWifiEnableResponse(PalWifiAsyncRequestTypes::SCAN,
                            false /* enableResponse */);
  bool success;
  sendEventToNanoapp(firstAppId, SCAN_REQUEST, timeOutCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);
  sendEventToNanoapp(secondAppId, SCAN_REQUEST, timeOutCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);

  test->waitForEvent(REQUEST_TIMED_OUT);

  // Make sure that we can still request scan for both nanoapps after a timed
  // out request.
  chrePalWifiEnableResponse(PalWifiAsyncRequestTypes::SCAN,
                            true /* enableResponse */);
  sendEventToNanoapp(firstAppId, SCAN_REQUEST, successCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);
  test->waitForEvent(CHRE_EVENT_WIFI_SCAN_RESULT);
  sendEventToNanoapp(secondAppId, SCAN_REQUEST, successCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);
  test->waitForEvent(CHRE_EVENT_WIFI_SCAN_RESULT);

  test->unloadNanoapp(firstAppId);
  test->unloadNanoapp(secondAppId);
}

TEST_F(WifiTimeoutTestSingleThread,
       WifiCanDispatchQueuedRequestAfterOneTimeout) {
  doWifiCanDispatchQueuedRequestAfterOneTimeoutTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTimeoutTestMultiThread,
       WifiCanDispatchQueuedRequestAfterOneTimeout) {
  doWifiCanDispatchQueuedRequestAfterOneTimeoutTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTimeoutTestMultiThread,
       WifiCanDispatchQueuedRequestAfterOneTimeoutForeground) {
  doWifiCanDispatchQueuedRequestAfterOneTimeoutTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doWifiScanMonitorTimeoutTest(TestBase *test,
                                  int8_t requestedThreadPriority) {
  CREATE_CHRE_TEST_EVENT(SCAN_MONITOR_REQUEST, 1);

  struct MonitoringRequest {
    bool enable;
    uint32_t cookie;
  };

  class App : public TestNanoapp {
   public:
    explicit App(const TestNanoappInfo &info) : TestNanoapp(setPerms(info)) {}

    static TestNanoappInfo setPerms(TestNanoappInfo info) {
      info.perms |= NanoappPermissions::CHRE_PERMS_WIFI;
      return info;
    }

    bool start() override {
      mRequestTimer = CHRE_TIMER_INVALID;
      return true;
    }

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (event->success) {
            if (mRequestTimer != CHRE_TIMER_INVALID) {
              chreTimerCancel(mRequestTimer);
              mRequestTimer = CHRE_TIMER_INVALID;
            }
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_ASYNC_RESULT,
                *(static_cast<const uint32_t *>(event->cookie)));
          }
          break;
        }

        case CHRE_EVENT_TIMER: {
          mRequestTimer = CHRE_TIMER_INVALID;
          TestEventQueueSingleton::get()->pushEvent(REQUEST_TIMED_OUT);
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SCAN_MONITOR_REQUEST:
              bool success = false;
              auto request =
                  static_cast<const MonitoringRequest *>(event->data);
              if (chreWifiConfigureScanMonitorAsync(request->enable,
                                                    &mCookie)) {
                mCookie = request->cookie;
                mRequestTimer = chreTimerSet(CHRE_TEST_ASYNC_RESULT_TIMEOUT_NS,
                                             nullptr, true /* oneShot */);
                success = mRequestTimer != CHRE_TIMER_INVALID;
              }

              TestEventQueueSingleton::get()->pushEvent(SCAN_MONITOR_REQUEST,
                                                        success);
          }
        }
      }
    }

   protected:
    uint32_t mCookie;
    uint32_t mRequestTimer;
  };

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  MonitoringRequest timeoutRequest{.enable = true, .cookie = 0xdead};
  chrePalWifiEnableResponse(PalWifiAsyncRequestTypes::SCAN_MONITORING, false);
  sendEventToNanoapp(appId, SCAN_MONITOR_REQUEST, timeoutRequest);
  bool success;
  test->waitForEvent(SCAN_MONITOR_REQUEST, &success);
  EXPECT_TRUE(success);

  test->waitForEvent(REQUEST_TIMED_OUT);

  // Make sure that we can still request to change scan monitor after a timed
  // out request.
  MonitoringRequest enableRequest{.enable = true, .cookie = 0x1010};
  chrePalWifiEnableResponse(PalWifiAsyncRequestTypes::SCAN_MONITORING, true);
  sendEventToNanoapp(appId, SCAN_MONITOR_REQUEST, enableRequest);
  test->waitForEvent(SCAN_MONITOR_REQUEST, &success);
  EXPECT_TRUE(success);

  uint32_t cookie;
  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, enableRequest.cookie);
  EXPECT_TRUE(chrePalWifiIsScanMonitoringActive());

  MonitoringRequest disableRequest{.enable = false, .cookie = 0x0101};
  sendEventToNanoapp(appId, SCAN_MONITOR_REQUEST, disableRequest);
  test->waitForEvent(SCAN_MONITOR_REQUEST, &success);
  EXPECT_TRUE(success);

  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, disableRequest.cookie);
  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  test->unloadNanoapp(appId);
}

TEST_F(WifiTimeoutTestSingleThread, WifiScanMonitorTimeoutTest) {
  doWifiScanMonitorTimeoutTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTimeoutTestMultiThread, WifiScanMonitorTimeoutTest) {
  doWifiScanMonitorTimeoutTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTimeoutTestMultiThread, WifiScanMonitorTimeoutTestForeground) {
  doWifiScanMonitorTimeoutTest(this,
                               NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doWifiRequestRangingTimeoutTest(TestBase *test,
                                     int8_t requestedThreadPriority) {
  CREATE_CHRE_TEST_EVENT(RANGING_REQUEST, 0);

  class App : public TestNanoapp {
   public:
    App()
        : TestNanoapp(TestNanoappInfo{
              .perms = chre::NanoappPermissions::CHRE_PERMS_WIFI}) {}

    explicit App(const TestNanoappInfo &info) : TestNanoapp(info) {}

    bool start() override {
      mRequestTimer = CHRE_TIMER_INVALID;
      return true;
    }

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          if (mRequestTimer != CHRE_TIMER_INVALID) {
            chreTimerCancel(mRequestTimer);
            mRequestTimer = CHRE_TIMER_INVALID;
          }

          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (event->success) {
            if (event->errorCode == 0) {
              TestEventQueueSingleton::get()->pushEvent(
                  CHRE_EVENT_WIFI_ASYNC_RESULT,
                  *(static_cast<const uint32_t *>(event->cookie)));
            }
          }
          break;
        }

        case CHRE_EVENT_TIMER: {
          mRequestTimer = CHRE_TIMER_INVALID;
          TestEventQueueSingleton::get()->pushEvent(REQUEST_TIMED_OUT);
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case RANGING_REQUEST:
              bool success = false;
              mCookie = *static_cast<uint32_t *>(event->data);

              // Placeholder parameters since linux PAL does not use this to
              // generate response
              struct chreWifiRangingTarget dummyRangingTarget = {
                  .macAddress = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc},
                  .primaryChannel = 0xdef02468,
                  .centerFreqPrimary = 0xace13579,
                  .centerFreqSecondary = 0xbdf369cf,
                  .channelWidth = 0x48,
              };

              struct chreWifiRangingParams dummyRangingParams = {
                  .targetListLen = 1,
                  .targetList = &dummyRangingTarget,
              };

              if (!chreWifiRequestRangingAsync(&dummyRangingParams, &mCookie)) {
                LOGE("Failed to request ranging");
              } else {
                mRequestTimer =
                    chreTimerSet(CHRE_TEST_WIFI_RANGING_RESULT_TIMEOUT_NS,
                                 nullptr, true /* oneShot */);
                success = mRequestTimer != CHRE_TIMER_INVALID;
              }
              TestEventQueueSingleton::get()->pushEvent(RANGING_REQUEST,
                                                        success);
          }
        }
      }
    }

   protected:
    uint32_t mCookie;
    uint32_t mRequestTimer;
  };

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  info.perms = NanoappPermissions::CHRE_PERMS_WIFI;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  uint32_t timeOutCookie = 0xdead;

  chrePalWifiEnableResponse(PalWifiAsyncRequestTypes::RANGING, false);
  sendEventToNanoapp(appId, RANGING_REQUEST, timeOutCookie);
  bool success;
  test->waitForEvent(RANGING_REQUEST, &success);
  EXPECT_TRUE(success);

  test->waitForEvent(REQUEST_TIMED_OUT);

  // Make sure that we can still request ranging after a timed out request
  uint32_t successCookie = 0x0101;
  chrePalWifiEnableResponse(PalWifiAsyncRequestTypes::RANGING, true);
  sendEventToNanoapp(appId, RANGING_REQUEST, successCookie);
  test->waitForEvent(RANGING_REQUEST, &success);
  EXPECT_TRUE(success);

  uint32_t cookie;
  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, successCookie);

  test->unloadNanoapp(appId);
}

TEST_F(WifiTimeoutTestSingleThread, WifiRequestRangingTimeoutTest) {
  doWifiRequestRangingTimeoutTest(this,
                                  NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTimeoutTestMultiThread, WifiRequestRangingTimeoutTest) {
  doWifiRequestRangingTimeoutTest(this,
                                  NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTimeoutTestMultiThread, WifiRequestRangingTimeoutTestForeground) {
  doWifiRequestRangingTimeoutTest(this,
                                  NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

}  // namespace
}  // namespace chre
