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

#include "chre_api/chre/wifi.h"
#include <cstdint>
#include "chre/core/event_loop_manager.h"
#include "chre/core/settings.h"
#include "chre/platform/linux/pal_nan.h"
#include "chre/platform/linux/pal_wifi.h"
#include "chre/platform/log.h"
#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre/event.h"

#include "gtest/gtest.h"
#include "test_base.h"
#include "test_event.h"
#include "test_event_queue.h"
#include "test_util.h"

namespace chre {
namespace {

class WifiTest : public SingleThreadTestBase {};
class WifiTestMultiThread : public MultiThreadTestBase {};

void doWifiCanSubscribeAndUnsubscribeToScanMonitoringTest(
    TestBase *test, int8_t requestedThreadPriority) {
  CREATE_CHRE_TEST_EVENT(MONITORING_REQUEST, 0);

  struct MonitoringRequest {
    bool enable;
    uint32_t cookie;
  };

  class App : public TestNanoapp {
   public:
    explicit App(const TestNanoappInfo &info) : TestNanoapp(info) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (event->success) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_ASYNC_RESULT,
                *(static_cast<const uint32_t *>(event->cookie)));
          }
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case MONITORING_REQUEST:
              auto request =
                  static_cast<const MonitoringRequest *>(event->data);
              mCookie = request->cookie;
              bool success =
                  chreWifiConfigureScanMonitorAsync(request->enable, &mCookie);
              TestEventQueueSingleton::get()->pushEvent(MONITORING_REQUEST,
                                                        success);
          }
        }
      }
    }

   protected:
    uint32_t mCookie;
  };

  TestNanoappInfo info;
  info.perms = NanoappPermissions::CHRE_PERMS_WIFI;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  MonitoringRequest request{.enable = true, .cookie = 0x123};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  bool success;
  test->waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  uint32_t cookie;
  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_TRUE(chrePalWifiIsScanMonitoringActive());

  request = {.enable = false, .cookie = 0x456};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  test->waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());
}

TEST_F(WifiTest, WifiCanSubscribeAndUnsubscribeToScanMonitoring) {
  doWifiCanSubscribeAndUnsubscribeToScanMonitoringTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTestMultiThread, WifiCanSubscribeAndUnsubscribeToScanMonitoring) {
  doWifiCanSubscribeAndUnsubscribeToScanMonitoringTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTestMultiThread,
       WifiCanSubscribeAndUnsubscribeToScanMonitoringForeground) {
  doWifiCanSubscribeAndUnsubscribeToScanMonitoringTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doWifiScanMonitoringDisabledOnUnloadTest(TestBase *test,
                                              int8_t requestedThreadPriority) {
  CREATE_CHRE_TEST_EVENT(MONITORING_REQUEST, 1);

  struct MonitoringRequest {
    bool enable;
    uint32_t cookie;
  };

  class App : public TestNanoapp {
   public:
    explicit App(const TestNanoappInfo &info) : TestNanoapp(info) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (event->success) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_ASYNC_RESULT,
                *(static_cast<const uint32_t *>(event->cookie)));
          }
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case MONITORING_REQUEST:
              auto request =
                  static_cast<const MonitoringRequest *>(event->data);
              mCookie = request->cookie;
              bool success =
                  chreWifiConfigureScanMonitorAsync(request->enable, &mCookie);
              TestEventQueueSingleton::get()->pushEvent(MONITORING_REQUEST,
                                                        success);
          }
        }
      }
    }

   protected:
    uint32_t mCookie;
  };

  TestNanoappInfo info;
  info.perms = NanoappPermissions::CHRE_PERMS_WIFI;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  MonitoringRequest request{.enable = true, .cookie = 0x123};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  bool success;
  test->waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  uint32_t cookie;
  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_TRUE(chrePalWifiIsScanMonitoringActive());

  test->unloadNanoapp(appId);
  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());
}

TEST_F(WifiTest, WifiScanMonitoringDisabledOnUnload) {
  doWifiScanMonitoringDisabledOnUnloadTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTestMultiThread, WifiScanMonitoringDisabledOnUnload) {
  doWifiScanMonitoringDisabledOnUnloadTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTestMultiThread, WifiScanMonitoringDisabledOnUnloadForeground) {
  doWifiScanMonitoringDisabledOnUnloadTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doWifiScanMonitoringDisabledOnUnloadAndCanBeReEnabledTest(
    TestBase *test, int8_t requestedThreadPriority) {
  CREATE_CHRE_TEST_EVENT(MONITORING_REQUEST, 1);

  struct MonitoringRequest {
    bool enable;
    uint32_t cookie;
  };

  class App : public TestNanoapp {
   public:
    explicit App(const TestNanoappInfo &info) : TestNanoapp(info) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (event->success) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_ASYNC_RESULT,
                *(static_cast<const uint32_t *>(event->cookie)));
          }
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case MONITORING_REQUEST:
              auto request =
                  static_cast<const MonitoringRequest *>(event->data);
              mCookie = request->cookie;
              bool success =
                  chreWifiConfigureScanMonitorAsync(request->enable, &mCookie);
              TestEventQueueSingleton::get()->pushEvent(MONITORING_REQUEST,
                                                        success);
          }
        }
      }
    }

   protected:
    uint32_t mCookie;
  };

  TestNanoappInfo info;
  info.perms = NanoappPermissions::CHRE_PERMS_WIFI;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  MonitoringRequest request{.enable = true, .cookie = 0x123};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  bool success;
  test->waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  uint32_t cookie;
  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_TRUE(chrePalWifiIsScanMonitoringActive());

  test->unloadNanoapp(appId);
  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  appId = test->loadNanoapp(MakeUnique<App>(info));
  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  request = {.enable = true, .cookie = 0x456};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  test->waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  test->waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_TRUE(chrePalWifiIsScanMonitoringActive());
}

TEST_F(WifiTest, WifiScanMonitoringDisabledOnUnloadAndCanBeReEnabled) {
  doWifiScanMonitoringDisabledOnUnloadAndCanBeReEnabledTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTestMultiThread,
       WifiScanMonitoringDisabledOnUnloadAndCanBeReEnabled) {
  doWifiScanMonitoringDisabledOnUnloadAndCanBeReEnabledTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(WifiTestMultiThread,
       WifiScanMonitoringDisabledOnUnloadAndCanBeReEnabledForeground) {
  doWifiScanMonitoringDisabledOnUnloadAndCanBeReEnabledTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

TEST_F(MultiThreadTestBase, ScanMonitorAndActiveScan) {
  // A nanoapp-to-nanoapp send event type used for this test.
  constexpr uint16_t kNanoappSendEvent = CHRE_EVENT_TEST_EVENT + 0x1000;

  CREATE_CHRE_TEST_EVENT(MONITORING_REQUEST, 0);
  CREATE_CHRE_TEST_EVENT(SCAN_REQUEST, 1);

  struct MonitoringRequest {
    bool enable;
    uint32_t cookie;
  };

  constexpr uint64_t kScanMonitorAppId = 0x123456789abcdef;
  class WifiScanMonitorTestNanoapp : public TestNanoapp {
   public:
    explicit WifiScanMonitorTestNanoapp(const TestNanoappInfo &info)
        : TestNanoapp(info) {}

    void handleEvent(uint32_t /*senderInstanceId*/, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (event->success) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_ASYNC_RESULT, *event);
          }
          break;
        }

        case CHRE_EVENT_WIFI_SCAN_RESULT: {
          auto *event = static_cast<const chreWifiScanEvent *>(eventData);
          // Since it's not possible to synchronize the ordering of the events
          // between the multiple threads, we rely on the active scan nanoapp to
          // signal this nanoapp that the active scan processing is complete,
          // then we provide the event to the main test thread. This ensures
          // proper ordering (active async result, then both scan events).
          if (mReceivedNanoappEvent) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_SCAN_RESULT, *event);
          } else {
            mScanEvent = *event;
          }
          break;
        }

        case kNanoappSendEvent: {
          mReceivedNanoappEvent = true;
          if (mScanEvent.has_value()) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_SCAN_RESULT, mScanEvent.value());
            mScanEvent.reset();
          }
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case MONITORING_REQUEST: {
              auto request =
                  static_cast<const MonitoringRequest *>(event->data);
              mCookie = request->cookie;
              bool success =
                  chreWifiConfigureScanMonitorAsync(request->enable, &mCookie);
              TestEventQueueSingleton::get()->pushEvent(MONITORING_REQUEST,
                                                        success);
              break;
            }
          }
        }
      }
    }

   protected:
    uint32_t mCookie;
    std::optional<chreWifiScanEvent> mScanEvent;
    bool mReceivedNanoappEvent = false;
  };

  constexpr uint64_t kActiveScanAppId = 0xfdceba987654321;
  class WifiActiveScanTestNanoapp : public TestNanoapp {
   public:
    explicit WifiActiveScanTestNanoapp(const TestNanoappInfo &info)
        : TestNanoapp(info) {}

    void handleEvent(uint32_t /*senderInstanceId*/, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_WIFI_ASYNC_RESULT: {
          auto *event = static_cast<const chreAsyncResult *>(eventData);
          if (event->success) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_WIFI_ASYNC_RESULT, *event);
          }
          break;
        }

        case CHRE_EVENT_WIFI_SCAN_RESULT: {
          auto *event = static_cast<const chreWifiScanEvent *>(eventData);
          TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_WIFI_SCAN_RESULT,
                                                    *event);
          chreNanoappInfo info;
          ASSERT_TRUE(chreGetNanoappInfoByAppId(kScanMonitorAppId, &info));
          ASSERT_TRUE(chreSendEvent(kNanoappSendEvent, /* eventData= */ nullptr,
                                    /* freeCallback= */ nullptr,
                                    info.instanceId));
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SCAN_REQUEST: {
              bool success = chreWifiRequestScanAsyncDefault(&mCookie);
              TestEventQueueSingleton::get()->pushEvent(SCAN_REQUEST, success);
              break;
            }
          }
        }
      }
    }

   protected:
    uint32_t mCookie;
  };

  TestNanoappInfo info1;
  info1.id = kScanMonitorAppId;
  info1.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL;
  info1.perms = NanoappPermissions::CHRE_PERMS_WIFI;
  uint64_t monitorAppId =
      loadNanoapp(MakeUnique<WifiScanMonitorTestNanoapp>(info1));
  TestNanoappInfo info2;
  info2.id = kActiveScanAppId;
  info2.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND;
  info2.perms = NanoappPermissions::CHRE_PERMS_WIFI;
  uint64_t activeScanAppId =
      loadNanoapp(MakeUnique<WifiActiveScanTestNanoapp>(info2));

  MonitoringRequest request = {.enable = true, .cookie = 0x123};
  sendEventToNanoapp(monitorAppId, MONITORING_REQUEST, request);
  bool success;
  waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  chreAsyncResult result;
  waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &result);
  EXPECT_TRUE(result.success);

  sendEventToNanoapp(activeScanAppId, SCAN_REQUEST);
  waitForEvent(SCAN_REQUEST, &success);
  EXPECT_TRUE(success);
  waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &result);
  EXPECT_TRUE(result.success);

  chreWifiScanEvent event1;
  waitForEvent(CHRE_EVENT_WIFI_SCAN_RESULT, &event1);
  chreWifiScanEvent event2;
  waitForEvent(CHRE_EVENT_WIFI_SCAN_RESULT, &event2);
  // Note that we can't do a deep comparison of chreWifiScanEvent, and we just
  // want to ensure that the same scan event is received by both nanaopps.
  // That's ok, as long as we don't try to access the nested pointers.
  EXPECT_EQ(memcmp(&event1, &event2, sizeof(event1)), 0);
}

}  // namespace
}  // namespace chre
