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

TEST_F(WifiTest, WifiCanSubscribeAndUnsubscribeToScanMonitoring) {
  CREATE_CHRE_TEST_EVENT(MONITORING_REQUEST, 0);

  struct MonitoringRequest {
    bool enable;
    uint32_t cookie;
  };

  class App : public TestNanoapp {
   public:
    App()
        : TestNanoapp(
              TestNanoappInfo{.perms = NanoappPermissions::CHRE_PERMS_WIFI}) {}

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

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  MonitoringRequest request{.enable = true, .cookie = 0x123};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  uint32_t cookie;
  waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_TRUE(chrePalWifiIsScanMonitoringActive());

  request = {.enable = false, .cookie = 0x456};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  bool success;
  waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());
}

TEST_F(WifiTest, WifiScanMonitoringDisabledOnUnload) {
  CREATE_CHRE_TEST_EVENT(MONITORING_REQUEST, 1);

  struct MonitoringRequest {
    bool enable;
    uint32_t cookie;
  };

  class App : public TestNanoapp {
   public:
    App()
        : TestNanoapp(
              TestNanoappInfo{.perms = NanoappPermissions::CHRE_PERMS_WIFI}) {}

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

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  MonitoringRequest request{.enable = true, .cookie = 0x123};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  bool success;
  waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  uint32_t cookie;
  waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_TRUE(chrePalWifiIsScanMonitoringActive());

  unloadNanoapp(appId);
  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());
}

TEST_F(WifiTest, WifiScanMonitoringDisabledOnUnloadAndCanBeReEnabled) {
  CREATE_CHRE_TEST_EVENT(MONITORING_REQUEST, 1);

  struct MonitoringRequest {
    bool enable;
    uint32_t cookie;
  };

  class App : public TestNanoapp {
   public:
    App()
        : TestNanoapp(
              TestNanoappInfo{.perms = NanoappPermissions::CHRE_PERMS_WIFI}) {}

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

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  MonitoringRequest request{.enable = true, .cookie = 0x123};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  bool success;
  waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  uint32_t cookie;
  waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_TRUE(chrePalWifiIsScanMonitoringActive());

  unloadNanoapp(appId);
  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  appId = loadNanoapp(MakeUnique<App>());
  EXPECT_FALSE(chrePalWifiIsScanMonitoringActive());

  request = {.enable = true, .cookie = 0x456};
  sendEventToNanoapp(appId, MONITORING_REQUEST, request);
  waitForEvent(MONITORING_REQUEST, &success);
  EXPECT_TRUE(success);
  waitForEvent(CHRE_EVENT_WIFI_ASYNC_RESULT, &cookie);
  EXPECT_EQ(cookie, request.cookie);
  EXPECT_TRUE(chrePalWifiIsScanMonitoringActive());
}

TEST_F(MultiThreadTestBase, ScanMonitorAndActiveScan) {
  CREATE_CHRE_TEST_EVENT(MONITORING_REQUEST, 0);
  CREATE_CHRE_TEST_EVENT(SCAN_REQUEST, 1);

  struct MonitoringRequest {
    bool enable;
    uint32_t cookie;
  };

  class WifiScanTestNanoapp : public TestNanoapp {
   public:
    explicit WifiScanTestNanoapp(const TestNanoappInfo &info)
        : TestNanoapp(info) {}

    bool start() {
      LOGI("Start: my id = 0x%" PRIx64 " instance id = 0x%" PRIx16,
           chreGetAppId(), chreGetInstanceId());
      return true;
    }

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
  info1.id = 0x123456789abcdef;
  info1.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL;
  info1.perms = NanoappPermissions::CHRE_PERMS_WIFI;
  uint64_t monitorAppId = loadNanoapp(MakeUnique<WifiScanTestNanoapp>(info1));
  TestNanoappInfo info2;
  info2.id = 0xfdceba987654321;
  info2.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND;
  info2.perms = NanoappPermissions::CHRE_PERMS_WIFI;
  uint64_t activeScanAppId =
      loadNanoapp(MakeUnique<WifiScanTestNanoapp>(info2));

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

  ASSERT_EQ(memcmp(&event1, &event2, sizeof(event1)), 0);
}

}  // namespace
}  // namespace chre