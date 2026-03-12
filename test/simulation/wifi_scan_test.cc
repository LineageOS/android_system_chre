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
#include "chre/platform/linux/pal_nan.h"
#include "chre/platform/linux/pal_wifi.h"
#include "chre/platform/log.h"
#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre/event.h"
#include "chre_api/chre/wifi.h"
#include "gtest/gtest.h"
#include "test_base.h"
#include "test_event.h"
#include "test_event_queue.h"
#include "test_util.h"

namespace chre {
namespace {

class WifiScanTestSingleThread : public SingleThreadTestBase {};
class WifiScanTestMultiThread : public MultiThreadTestBase {};

using namespace std::chrono_literals;

CREATE_CHRE_TEST_EVENT(SCAN_REQUEST, 20);

struct WifiAsyncData {
  const uint32_t *cookie;
  chreError errorCode;
};

constexpr uint64_t kAppOneId = 0x0123456789000001;
constexpr uint64_t kAppTwoId = 0x0123456789000002;

template <class TestBaseClass>
class WifiScanRequestQueueTest : public TestBaseClass {
  void SetUp() {
    TestBaseClass::SetUp();
    // Add delay to make sure the requests are queued.
    chrePalWifiDelayResponse(PalWifiAsyncRequestTypes::SCAN,
                             /* milliseconds= */ 100ms);
  }

  void TearDown() {
    TestBaseClass::TearDown();
    chrePalWifiDelayResponse(PalWifiAsyncRequestTypes::SCAN,
                             /* milliseconds= */ 0ms);
  }
};

class WifiScanRequestQueueTestSingleThread
    : public WifiScanRequestQueueTest<SingleThreadTestBase> {};
class WifiScanRequestQueueTestMultiThread
    : public WifiScanRequestQueueTest<MultiThreadTestBase> {};

class WifiScanTestNanoapp : public TestNanoapp {
 public:
  explicit WifiScanTestNanoapp(TestNanoappInfo info = {})
      : TestNanoapp(setPerms(info)) {}

  bool start() override {
    chreUserSettingConfigureEvents(CHRE_USER_SETTING_WIFI_AVAILABLE,
                                   true /* enable */);
    return true;
  }

  void handleEvent(uint32_t, uint16_t eventType,
                   const void *eventData) override {
    switch (eventType) {
      case CHRE_EVENT_WIFI_SCAN_RESULT:
        TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_WIFI_SCAN_RESULT);
        break;

      case CHRE_EVENT_SETTING_CHANGED_WIFI_AVAILABLE:
        TestEventQueueSingleton::get()->pushEvent(
            CHRE_EVENT_SETTING_CHANGED_WIFI_AVAILABLE,
            *static_cast<const chreUserSettingChangedEvent *>(eventData));
        break;

      case CHRE_EVENT_WIFI_ASYNC_RESULT: {
        auto *event = static_cast<const chreAsyncResult *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(
            CHRE_EVENT_WIFI_ASYNC_RESULT,
            WifiAsyncData{
                .cookie = static_cast<const uint32_t *>(event->cookie),
                .errorCode = static_cast<chreError>(event->errorCode)});
        break;
      }

      case CHRE_EVENT_TEST_EVENT: {
        auto event = static_cast<const TestEvent *>(eventData);
        switch (event->type) {
          case SCAN_REQUEST:
            bool success = false;
            if (mNextFreeCookieIndex < kMaxPendingCookie) {
              mCookies[mNextFreeCookieIndex] =
                  *static_cast<uint32_t *>(event->data);
              success = chreWifiRequestScanAsyncDefault(
                  &mCookies[mNextFreeCookieIndex]);
              mNextFreeCookieIndex++;
            } else {
              LOGE("Too many cookies passed from test body!");
            }
            TestEventQueueSingleton::get()->pushEvent(SCAN_REQUEST, success);
        }
      }
    }
  }

  void end() override {
    chreUserSettingConfigureEvents(CHRE_USER_SETTING_WIFI_AVAILABLE,
                                   false /* enable */);
  }

 protected:
  static TestNanoappInfo setPerms(TestNanoappInfo info) {
    info.perms |= NanoappPermissions::CHRE_PERMS_WIFI;
    return info;
  }

  static constexpr uint8_t kMaxPendingCookie = 10;
  uint32_t mCookies[kMaxPendingCookie];
  uint8_t mNextFreeCookieIndex = 0;
};

void doWifiScanBasicSettingTest(TestBase *test,
                                int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<WifiScanTestNanoapp>(info));

  chreUserSettingChangedEvent settingChangedEvent;
  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::WIFI_AVAILABLE, true /* enabled */);
  test->waitForEvent(CHRE_EVENT_SETTING_CHANGED_WIFI_AVAILABLE,
                     &settingChangedEvent);
  EXPECT_EQ(settingChangedEvent.setting, CHRE_USER_SETTING_WIFI_AVAILABLE);
  EXPECT_EQ(settingChangedEvent.settingState, CHRE_USER_SETTING_STATE_ENABLED);

  constexpr uint32_t firstCookie = 0x1010;
  bool success;
  WifiAsyncData wifiAsyncData;
  sendEventToNanoapp(appId, SCAN_REQUEST, firstCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);

  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &wifiAsyncData);
  EXPECT_EQ(wifiAsyncData.errorCode, CHRE_ERROR_NONE);
  EXPECT_EQ(*wifiAsyncData.cookie, firstCookie);
  test->waitForEvent(CHRE_EVENT_WIFI_SCAN_RESULT);

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::WIFI_AVAILABLE, false /* enabled */);
  test->waitForEvent(CHRE_EVENT_SETTING_CHANGED_WIFI_AVAILABLE,
                     &settingChangedEvent);
  EXPECT_EQ(settingChangedEvent.setting, CHRE_USER_SETTING_WIFI_AVAILABLE);
  EXPECT_EQ(settingChangedEvent.settingState, CHRE_USER_SETTING_STATE_DISABLED);

  constexpr uint32_t secondCookie = 0x2020;
  sendEventToNanoapp(appId, SCAN_REQUEST, secondCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);

  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &wifiAsyncData);
  EXPECT_EQ(wifiAsyncData.errorCode, CHRE_ERROR_FUNCTION_DISABLED);
  EXPECT_EQ(*wifiAsyncData.cookie, secondCookie);

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::WIFI_AVAILABLE, true /* enabled */);
  test->waitForEvent(CHRE_EVENT_SETTING_CHANGED_WIFI_AVAILABLE,
                     &settingChangedEvent);
  EXPECT_EQ(settingChangedEvent.setting, CHRE_USER_SETTING_WIFI_AVAILABLE);
  EXPECT_EQ(settingChangedEvent.settingState, CHRE_USER_SETTING_STATE_ENABLED);
  test->unloadNanoapp(appId);
}

TEST_F(WifiScanTestSingleThread, WifiScanBasicSettingTest) {
  doWifiScanBasicSettingTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiScanTestMultiThread, WifiScanBasicSettingTest) {
  doWifiScanBasicSettingTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiScanTestMultiThread, WifiScanBasicSettingTestForeground) {
  doWifiScanBasicSettingTest(this,
                             NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doWifiScanRequestDuringResultTest(TestBase *test,
                                       int8_t requestedThreadPriority) {
  // Test that a nanoapp can request a scan during the result of a previous
  // scan request.

  // 1. Make nanoapp request scan
  // 2. Have nanoapp programmed to re-request scan during result (only one
  // time)
  // 3. Make sure that the second request is accepted

  class WifiScanTestRequestDuringResultNanoapp : public TestNanoapp {
   public:
    explicit WifiScanTestRequestDuringResultNanoapp(const TestNanoappInfo &info)
        : TestNanoapp(setPerms(info)) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          LOGI("got async result success= %d", event->success);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_WIFI_ASYNC_RESULT,
              WifiAsyncData{
                  .cookie = static_cast<const uint32_t *>(event->cookie),
                  .errorCode = static_cast<chreError>(event->errorCode)});
          break;
        }

        case CHRE_EVENT_WIFI_SCAN_RESULT: {
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_WIFI_SCAN_RESULT);

          // If this is the first time we receive a scan result, we should
          // request another scan immediately.
          if (mScanRequestCount == 1) {
            mScanRequestCount++;
            bool success = chreWifiRequestScanAsyncDefault(&mSentCookie);
            LOGI("requested second scan with success= %d", success);
            TestEventQueueSingleton::get()->pushEvent(SCAN_REQUEST, success);
          }

          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          bool success = false;
          switch (event->type) {
            case SCAN_REQUEST:
              mSentCookie = *static_cast<uint32_t *>(event->data);
              mScanRequestCount++;
              success = chreWifiRequestScanAsyncDefault(&(mSentCookie));
              LOGI("requested scan with success= %d", success);
              TestEventQueueSingleton::get()->pushEvent(SCAN_REQUEST, success);
              break;
          }
        }
      }
    }

   protected:
    static TestNanoappInfo setPerms(TestNanoappInfo info) {
      info.perms |= NanoappPermissions::CHRE_PERMS_WIFI;
      return info;
    }

    uint32_t mSentCookie;
    uint32_t mScanRequestCount = 0;
    WifiAsyncData mReceivedAsyncResult;
  };
  TestNanoappInfo info;
  info.id = kAppOneId;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appOneId = test->loadNanoapp(
      MakeUnique<WifiScanTestRequestDuringResultNanoapp>(info));

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::WIFI_AVAILABLE, true /* enabled */);

  constexpr uint32_t appOneRequestCookie = 0x1010;
  bool success;
  WifiAsyncData wifiAsyncData;

  // Request the first scan, which will trigger the second as well
  sendEventToNanoapp(appOneId, SCAN_REQUEST, appOneRequestCookie);
  for (int i = 0; i < 2; ++i) {
    test->waitForEvent(SCAN_REQUEST, &success);
    EXPECT_TRUE(success);
    test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &wifiAsyncData);
    EXPECT_EQ(wifiAsyncData.errorCode, CHRE_ERROR_NONE);
    EXPECT_EQ(*wifiAsyncData.cookie, appOneRequestCookie);
    test->waitForEvent(CHRE_EVENT_WIFI_SCAN_RESULT);
  }

  test->unloadNanoapp(appOneId);
}

TEST_F(WifiScanRequestQueueTestSingleThread, WifiScanRequestDuringResultTest) {
  doWifiScanRequestDuringResultTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiScanRequestQueueTestMultiThread, WifiScanRequestDuringResultTest) {
  doWifiScanRequestDuringResultTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiScanRequestQueueTestMultiThread,
       WifiScanRequestDuringResultTestForeground) {
  doWifiScanRequestDuringResultTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doWifiScanEventBeforeResponseTest(TestBase *test,
                                       int8_t requestedThreadPriority) {
  // Test that the system correctly handles the case where a scan monitor
  // event comes between a scan request and the corresponding response.

  // 1. Make nanoapp request scan monitor
  // 2. Request scan when scan monitor setup is complete
  // 3. Deliver a scan monitor event
  // 4. Deliver the scan request result
  class WifiScanTestEventBeforeResponseNanoapp : public TestNanoapp {
   public:
    explicit WifiScanTestEventBeforeResponseNanoapp(TestNanoappInfo info)
        : TestNanoapp(info) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          SCOPED_TRACE(::testing::Message()
                       << "Async result requestType=" << event->requestType);
          EXPECT_TRUE(event->success);
          if (event->requestType ==
              CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR) {
            EXPECT_TRUE(chrePalWifiTriggerScanMonitorEvent());
            EXPECT_TRUE(chreWifiRequestScanAsyncDefault(/*cookie=*/nullptr));
          } else {
            EXPECT_EQ(event->requestType, CHRE_WIFI_REQUEST_TYPE_REQUEST_SCAN);
          }
          break;
        }

        case CHRE_EVENT_WIFI_SCAN_RESULT: {
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_WIFI_SCAN_RESULT);
          triggerWait(CHRE_EVENT_WIFI_SCAN_RESULT);
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SCAN_REQUEST:
              EXPECT_TRUE(
                  chreWifiConfigureScanMonitorAsync(true, /*cookie=*/nullptr));
              break;
          }
        }
      }
    }
  };

  TestNanoappInfo info;
  info.id = kAppOneId;
  info.perms = NanoappPermissions::CHRE_PERMS_WIFI;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appOneId = test->loadNanoapp(
      MakeUnique<WifiScanTestEventBeforeResponseNanoapp>(info));

  // Get the nanoapp flow started
  sendEventToNanoapp(appOneId, SCAN_REQUEST);

  // We should get 2 scan results, one for the scan monitor and one for the
  // scan request.
  test->waitForEvent(CHRE_EVENT_WIFI_SCAN_RESULT);
  test->waitForEvent(CHRE_EVENT_WIFI_SCAN_RESULT);

  test->unloadNanoapp(appOneId);
}

TEST_F(WifiScanRequestQueueTestSingleThread, WifiScanEventBeforeResponseTest) {
  doWifiScanEventBeforeResponseTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiScanRequestQueueTestMultiThread, WifiScanEventBeforeResponseTest) {
  doWifiScanEventBeforeResponseTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

void doWifiQueuedScanSettingChangeTest(TestBase *test,
                                       int8_t requestedThreadPriority1,
                                       int8_t requestedThreadPriority2) {
  CREATE_CHRE_TEST_EVENT(CONCURRENT_NANOAPP_RECEIVED_EXPECTED_ASYNC_EVENT_COUNT,
                         1);
  CREATE_CHRE_TEST_EVENT(CONCURRENT_NANOAPP_READ_ASYNC_EVENT, 2);
  // Expecting to receive two event, one from each nanoapp.
  constexpr uint8_t kExpectedReceiveAsyncResultCount = 2;
  // receivedAsyncEventCount is shared across apps and must be static.
  // But we want it initialized each time the test is executed.
  static uint8_t receivedAsyncEventCount;
  receivedAsyncEventCount = 0;

  class WifiScanTestConcurrentNanoapp : public TestNanoapp {
   public:
    explicit WifiScanTestConcurrentNanoapp(const TestNanoappInfo &info)
        : TestNanoapp(setPerms(info)) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          mReceivedAsyncResult = WifiAsyncData{
              .cookie = static_cast<const uint32_t *>(event->cookie),
              .errorCode = static_cast<chreError>(event->errorCode)};
          ++receivedAsyncEventCount;
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          bool success = false;
          switch (event->type) {
            case SCAN_REQUEST:
              mSentCookie = *static_cast<uint32_t *>(event->data);
              success = chreWifiRequestScanAsyncDefault(&(mSentCookie));
              TestEventQueueSingleton::get()->pushEvent(SCAN_REQUEST, success);
              break;
            case CONCURRENT_NANOAPP_READ_ASYNC_EVENT:
              TestEventQueueSingleton::get()->pushEvent(
                  CONCURRENT_NANOAPP_READ_ASYNC_EVENT, mReceivedAsyncResult);
              break;
          }
        }
      }

      if (receivedAsyncEventCount == kExpectedReceiveAsyncResultCount) {
        TestEventQueueSingleton::get()->pushEvent(
            CONCURRENT_NANOAPP_RECEIVED_EXPECTED_ASYNC_EVENT_COUNT);
      }
    }

   protected:
    static TestNanoappInfo setPerms(TestNanoappInfo info) {
      info.perms |= NanoappPermissions::CHRE_PERMS_WIFI;
      return info;
    }

    uint32_t mSentCookie;
    WifiAsyncData mReceivedAsyncResult;
  };

  TestNanoappInfo info1;
  info1.id = kAppOneId;
  info1.requestedThreadPriority = requestedThreadPriority1;
  uint64_t appOneId =
      test->loadNanoapp(MakeUnique<WifiScanTestConcurrentNanoapp>(info1));
  TestNanoappInfo info2;
  info2.id = kAppTwoId;
  info2.requestedThreadPriority = requestedThreadPriority2;
  uint64_t appTwoId =
      test->loadNanoapp(MakeUnique<WifiScanTestConcurrentNanoapp>(info2));

  constexpr uint32_t appOneRequestCookie = 0x1010;
  constexpr uint32_t appTwoRequestCookie = 0x2020;
  bool success;
  sendEventToNanoapp(appOneId, SCAN_REQUEST, appOneRequestCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);
  sendEventToNanoapp(appTwoId, SCAN_REQUEST, appTwoRequestCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::WIFI_AVAILABLE, false /* enabled */);

  // We need to make sure that each nanoapp has received one async result
  // before further analysis.
  test->waitForEvent(CONCURRENT_NANOAPP_RECEIVED_EXPECTED_ASYNC_EVENT_COUNT);

  WifiAsyncData wifiAsyncData;
  sendEventToNanoapp(appOneId, CONCURRENT_NANOAPP_READ_ASYNC_EVENT);
  test->waitForEvent(CONCURRENT_NANOAPP_READ_ASYNC_EVENT, &wifiAsyncData);
  EXPECT_EQ(wifiAsyncData.errorCode, CHRE_ERROR_NONE);
  EXPECT_EQ(*wifiAsyncData.cookie, appOneRequestCookie);

  sendEventToNanoapp(appTwoId, CONCURRENT_NANOAPP_READ_ASYNC_EVENT);
  test->waitForEvent(CONCURRENT_NANOAPP_READ_ASYNC_EVENT, &wifiAsyncData);
  EXPECT_EQ(wifiAsyncData.errorCode, CHRE_ERROR_FUNCTION_DISABLED);
  EXPECT_EQ(*wifiAsyncData.cookie, appTwoRequestCookie);

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::WIFI_AVAILABLE, true /* enabled */);

  test->unloadNanoapp(appOneId);
  test->unloadNanoapp(appTwoId);
}

TEST_F(WifiScanRequestQueueTestSingleThread, WifiQueuedScanSettingChangeTest) {
  doWifiQueuedScanSettingChangeTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiScanRequestQueueTestMultiThread, WifiQueuedScanSettingChangeTest) {
  doWifiQueuedScanSettingChangeTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiScanRequestQueueTestMultiThread,
       WifiQueuedScanSettingChangeTestForeground) {
  doWifiQueuedScanSettingChangeTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL,
      NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doWifiScanRejectRequestFromSameNanoappTest(
    TestBase *test, int8_t requestedThreadPriority) {
  CREATE_CHRE_TEST_EVENT(RECEIVED_ALL_EXPECTED_EVENTS, 1);
  CREATE_CHRE_TEST_EVENT(READ_ASYNC_EVENT, 2);

  static constexpr uint8_t kExpectedReceivedScanRequestCount = 2;

  class WifiScanTestBufferedAsyncResultNanoapp : public TestNanoapp {
   public:
    explicit WifiScanTestBufferedAsyncResultNanoapp(const TestNanoappInfo &info)
        : TestNanoapp(setPerms(info)) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          mReceivedAsyncResult = WifiAsyncData{
              .cookie = static_cast<const uint32_t *>(event->cookie),
              .errorCode = static_cast<chreError>(event->errorCode)};
          ++mReceivedAsyncEventCount;
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          bool success = false;
          switch (event->type) {
            case SCAN_REQUEST:
              if (mReceivedScanRequestCount >=
                  kExpectedReceivedScanRequestCount) {
                LOGE("Asking too many scan request");
              } else {
                mReceivedCookies[mReceivedScanRequestCount] =
                    *static_cast<uint32_t *>(event->data);
                success = chreWifiRequestScanAsyncDefault(
                    &(mReceivedCookies[mReceivedScanRequestCount]));
                ++mReceivedScanRequestCount;
                TestEventQueueSingleton::get()->pushEvent(SCAN_REQUEST,
                                                          success);
              }
              break;
            case READ_ASYNC_EVENT:
              TestEventQueueSingleton::get()->pushEvent(READ_ASYNC_EVENT,
                                                        mReceivedAsyncResult);
              break;
          }
        }
      }
      if (mReceivedAsyncEventCount == kExpectedReceivedAsyncResultCount &&
          mReceivedScanRequestCount == kExpectedReceivedScanRequestCount) {
        TestEventQueueSingleton::get()->pushEvent(RECEIVED_ALL_EXPECTED_EVENTS);
      }
    }

   protected:
    static TestNanoappInfo setPerms(TestNanoappInfo info) {
      info.perms |= NanoappPermissions::CHRE_PERMS_WIFI;
      return info;
    }

    // We are only expecting to receive one async result since the second
    // request is expected to fail.
    const uint8_t kExpectedReceivedAsyncResultCount = 1;
    uint8_t mReceivedAsyncEventCount = 0;
    uint8_t mReceivedScanRequestCount = 0;

    // We need to have two cookie storage to separate the two scan request.
    uint32_t mReceivedCookies[kExpectedReceivedScanRequestCount];
    WifiAsyncData mReceivedAsyncResult;
  };

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(
      MakeUnique<WifiScanTestBufferedAsyncResultNanoapp>(info));

  constexpr uint32_t kFirstRequestCookie = 0x1010;
  constexpr uint32_t kSecondRequestCookie = 0x2020;
  bool success;
  sendEventToNanoapp(appId, SCAN_REQUEST, kFirstRequestCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);
  sendEventToNanoapp(appId, SCAN_REQUEST, kSecondRequestCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_FALSE(success);

  // We need to make sure that the nanoapp has received one async result and
  // did two scan requests before further analysis.
  test->waitForEvent(RECEIVED_ALL_EXPECTED_EVENTS);

  WifiAsyncData wifiAsyncData;
  sendEventToNanoapp(appId, READ_ASYNC_EVENT);
  test->waitForEvent(READ_ASYNC_EVENT, &wifiAsyncData);
  EXPECT_EQ(wifiAsyncData.errorCode, CHRE_ERROR_NONE);
  EXPECT_EQ(*wifiAsyncData.cookie, kFirstRequestCookie);

  test->unloadNanoapp(appId);
}

TEST_F(WifiScanRequestQueueTestSingleThread,
       WifiScanRejectRequestFromSameNanoapp) {
  doWifiScanRejectRequestFromSameNanoappTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiScanRequestQueueTestMultiThread,
       WifiScanRejectRequestFromSameNanoapp) {
  doWifiScanRejectRequestFromSameNanoappTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

void doWifiScanActiveScanFromDistinctNanoappsTest(
    TestBase *test, int8_t requestedThreadPriority1,
    int8_t requestedThreadPriority2) {
  CREATE_CHRE_TEST_EVENT(CONCURRENT_NANOAPP_RECEIVED_EXPECTED_ASYNC_EVENT_COUNT,
                         1);
  CREATE_CHRE_TEST_EVENT(CONCURRENT_NANOAPP_READ_COOKIE, 2);

  constexpr uint8_t kExpectedReceiveAsyncResultCount = 2;
  // receivedCookieCount is shared across apps and must be static.
  // But we want it initialized each time the test is executed.
  static uint8_t receivedCookieCount;
  receivedCookieCount = 0;

  class WifiScanTestConcurrentNanoapp : public TestNanoapp {
   public:
    explicit WifiScanTestConcurrentNanoapp(const TestNanoappInfo &info)
        : TestNanoapp(setPerms(info)) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (event->errorCode == CHRE_ERROR_NONE) {
            mReceivedCookie = *static_cast<const uint32_t *>(event->cookie);
            ++receivedCookieCount;
          } else {
            LOGE("Received failed async result");
          }

          if (receivedCookieCount == kExpectedReceiveAsyncResultCount) {
            TestEventQueueSingleton::get()->pushEvent(
                CONCURRENT_NANOAPP_RECEIVED_EXPECTED_ASYNC_EVENT_COUNT);
          }
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          bool success = false;
          switch (event->type) {
            case SCAN_REQUEST:
              mSentCookie = *static_cast<uint32_t *>(event->data);
              success = chreWifiRequestScanAsyncDefault(&(mSentCookie));
              TestEventQueueSingleton::get()->pushEvent(SCAN_REQUEST, success);
              break;
            case CONCURRENT_NANOAPP_READ_COOKIE:
              TestEventQueueSingleton::get()->pushEvent(
                  CONCURRENT_NANOAPP_READ_COOKIE, mReceivedCookie);
              break;
          }
        }
      }
    }

   protected:
    static TestNanoappInfo setPerms(TestNanoappInfo info) {
      info.perms |= NanoappPermissions::CHRE_PERMS_WIFI;
      return info;
    }

    uint32_t mSentCookie;
    uint32_t mReceivedCookie;
  };

  TestNanoappInfo info1;
  info1.id = kAppOneId;
  info1.requestedThreadPriority = requestedThreadPriority1;
  uint64_t appOneId =
      test->loadNanoapp(MakeUnique<WifiScanTestConcurrentNanoapp>(info1));
  TestNanoappInfo info2;
  info2.id = kAppTwoId;
  info2.requestedThreadPriority = requestedThreadPriority2;
  uint64_t appTwoId =
      test->loadNanoapp(MakeUnique<WifiScanTestConcurrentNanoapp>(info2));

  constexpr uint32_t kAppOneRequestCookie = 0x1010;
  constexpr uint32_t kAppTwoRequestCookie = 0x2020;
  bool success;
  sendEventToNanoapp(appOneId, SCAN_REQUEST, kAppOneRequestCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);
  sendEventToNanoapp(appTwoId, SCAN_REQUEST, kAppTwoRequestCookie);
  test->waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);

  test->waitForEvent(CONCURRENT_NANOAPP_RECEIVED_EXPECTED_ASYNC_EVENT_COUNT);

  uint32_t receivedCookie;
  sendEventToNanoapp(appOneId, CONCURRENT_NANOAPP_READ_COOKIE);
  test->waitForEvent(CONCURRENT_NANOAPP_READ_COOKIE, &receivedCookie);
  EXPECT_EQ(kAppOneRequestCookie, receivedCookie);

  sendEventToNanoapp(appTwoId, CONCURRENT_NANOAPP_READ_COOKIE);
  test->waitForEvent(CONCURRENT_NANOAPP_READ_COOKIE, &receivedCookie);
  EXPECT_EQ(kAppTwoRequestCookie, receivedCookie);

  test->unloadNanoapp(appOneId);
  test->unloadNanoapp(appTwoId);
}

TEST_F(WifiScanRequestQueueTestSingleThread,
       WifiScanActiveScanFromDistinctNanoapps) {
  doWifiScanActiveScanFromDistinctNanoappsTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL,
      NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiScanRequestQueueTestMultiThread,
       WifiScanActiveScanFromDistinctNanoapps) {
  doWifiScanActiveScanFromDistinctNanoappsTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL,
      NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

}  // namespace
}  // namespace chre
