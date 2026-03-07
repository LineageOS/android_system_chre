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

#include "chre/common.h"
#include "inc/test_util.h"
#include "test_base.h"

#include <gtest/gtest.h>
#include <cstdint>

#include "chre/core/event_loop_manager.h"
#include "chre/core/settings.h"
#include "chre/platform/linux/pal_ble.h"
#include "chre_api/chre/ble.h"

#include "chre/util/nested_data_ptr.h"
#include "chre_api/chre/user_settings.h"

namespace chre {

namespace {

CREATE_CHRE_TEST_EVENT(START_SCAN, 0);
CREATE_CHRE_TEST_EVENT(SCAN_STARTED, 1);
CREATE_CHRE_TEST_EVENT(STOP_SCAN, 2);
CREATE_CHRE_TEST_EVENT(SCAN_STOPPED, 3);
CREATE_CHRE_TEST_EVENT(CALL_FLUSH, 4);
CREATE_CHRE_TEST_EVENT(GET_SCAN_STATUS, 5);
CREATE_CHRE_TEST_EVENT(RSSI_REQUEST, 6);
CREATE_CHRE_TEST_EVENT(RSSI_REQUEST_SENT, 7);

class BleTest : public SingleThreadTestBase {};
class BleTestMultiThread : public MultiThreadTestBase {};

class BleTestNanoapp : public TestNanoapp {
 public:
  explicit BleTestNanoapp(TestNanoappInfo info = {})
      : TestNanoapp(updateInfo(info)) {}

  static TestNanoappInfo updateInfo(TestNanoappInfo info) {
    info.perms |= CHRE_PERMS_BLE;
    return info;
  }

  bool start() override {
    chreUserSettingConfigureEvents(CHRE_USER_SETTING_BLE_AVAILABLE,
                                   true /* enable */);
    return true;
  }

  void end() override {
    chreUserSettingConfigureEvents(CHRE_USER_SETTING_BLE_AVAILABLE,
                                   false /* enable */);
  }

  void handleEvent(uint32_t, uint16_t eventType,
                   const void *eventData) override {
    switch (eventType) {
      case CHRE_EVENT_BLE_ASYNC_RESULT: {
        auto *event = static_cast<const struct chreAsyncResult *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_BLE_ASYNC_RESULT,
                                                  *event);
        break;
      }

      case CHRE_EVENT_BLE_ADVERTISEMENT: {
        auto event = static_cast<const chreBatchCompleteEvent *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_BLE_ADVERTISEMENT,
                                                  *event);
        break;
      }
      case CHRE_EVENT_BLE_RSSI_READ: {
        auto *event =
            static_cast<const struct chreBleReadRssiEvent *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_BLE_RSSI_READ,
                                                  *event);
        break;
      }

      case CHRE_EVENT_BLE_FLUSH_COMPLETE: {
        auto *event = static_cast<const struct chreAsyncResult *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_BLE_FLUSH_COMPLETE,
                                                  *event);
        break;
      }
      case CHRE_EVENT_BLE_BATCH_COMPLETE: {
        auto event = static_cast<const chreBatchCompleteEvent *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(CHRE_EVENT_BLE_BATCH_COMPLETE,
                                                  *event);
        break;
      }

      case CHRE_EVENT_BLE_SCAN_STATUS_CHANGE: {
        auto *event = static_cast<const struct chreBleScanStatus *>(eventData);
        TestEventQueueSingleton::get()->pushEvent(
            CHRE_EVENT_BLE_SCAN_STATUS_CHANGE, *event);
        break;
      }

      case CHRE_EVENT_SETTING_CHANGED_BLE_AVAILABLE: {
        auto *event =
            static_cast<const chreUserSettingChangedEvent *>(eventData);
        bool enabled = event->settingState == CHRE_USER_SETTING_STATE_ENABLED;
        TestEventQueueSingleton::get()->pushEvent(
            CHRE_EVENT_SETTING_CHANGED_BLE_AVAILABLE, enabled);
        break;
      }

      case CHRE_EVENT_TEST_EVENT: {
        auto event = static_cast<const TestEvent *>(eventData);
        switch (event->type) {
          case START_SCAN: {
            uint32_t reportDelayMs = 0;
            if (event->data != nullptr) {
              reportDelayMs = *static_cast<uint32_t *>(event->data);
            }
            const bool success = chreBleStartScanAsync(
                CHRE_BLE_SCAN_MODE_AGGRESSIVE, reportDelayMs, nullptr);
            TestEventQueueSingleton::get()->pushEvent(START_SCAN, success);
            break;
          }

          case STOP_SCAN: {
            const bool success = chreBleStopScanAsync();
            TestEventQueueSingleton::get()->pushEvent(STOP_SCAN, success);
            break;
          }
          case RSSI_REQUEST: {
            constexpr uint16_t kConnectionHandle = 6;
            constexpr uint32_t kCookie = 123;
            const bool success =
                chreBleReadRssiAsync(kConnectionHandle, (void *)kCookie);
            TestEventQueueSingleton::get()->pushEvent(RSSI_REQUEST_SENT,
                                                      success);
            break;
          }
          case CALL_FLUSH: {
            const bool success = chreBleFlushAsync(&mCookie);
            TestEventQueueSingleton::get()->pushEvent(CALL_FLUSH, success);
            break;
          }
          case GET_SCAN_STATUS: {
            chreBleScanStatus status{};
            const bool success = chreBleGetScanStatus(&status);
            if (success) {
              TestEventQueueSingleton::get()->pushEvent(GET_SCAN_STATUS,
                                                        status);
            }
            break;
          }
          default: {
            LOGE("Unhandled test event: 0x%04x", event->type);
          }
        }
        break;
      }
      default:
        LOGE("Unhandled event type: 0x%04x", eventType);
    }
  }

 protected:
  uint32_t mCookie = 0;
};

void assertStartScanSuccess(TestBase *test, const uint64_t appId,
                            const uint32_t reportDelayMs = 0) {
  bool success;
  sendEventToNanoapp(appId, START_SCAN, NestedDataPtr(reportDelayMs));
  test->waitForEvent(START_SCAN, &success);
  ASSERT_TRUE(success);
  chreAsyncResult result{};
  test->waitForEvent(CHRE_EVENT_BLE_ASYNC_RESULT, &result);
  ASSERT_EQ(result.errorCode, CHRE_ERROR_NONE);
  ASSERT_TRUE(chrePalIsBleEnabled());
}

void assertStopScanSuccess(TestBase *test, const uint64_t appId) {
  bool success;
  sendEventToNanoapp(appId, STOP_SCAN);
  test->waitForEvent(STOP_SCAN, &success);
  ASSERT_TRUE(success);
  chreAsyncResult result{};
  test->waitForEvent(CHRE_EVENT_BLE_ASYNC_RESULT, &result);
  ASSERT_EQ(result.errorCode, CHRE_ERROR_NONE);
  ASSERT_FALSE(chrePalIsBleEnabled());
}

/**
 * This test verifies that a nanoapp can query for BLE capabilities and filter
 * capabilities. Note that a nanoapp does not require BLE permissions to use
 * these APIs.
 */
void doBleCapabilitiesTest(TestBase *test, int8_t requestedThreadPriority) {
  CREATE_CHRE_TEST_EVENT(GET_CAPABILITIES, 0);
  CREATE_CHRE_TEST_EVENT(GET_FILTER_CAPABILITIES, 1);

  class App : public TestNanoapp {
   public:
    explicit App(TestNanoappInfo info = {}) : TestNanoapp(updateInfo(info)) {}

    static TestNanoappInfo updateInfo(TestNanoappInfo info) {
      info.perms |= NanoappPermissions::CHRE_PERMS_WIFI;
      return info;
    }

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case GET_CAPABILITIES: {
              TestEventQueueSingleton::get()->pushEvent(
                  GET_CAPABILITIES, chreBleGetCapabilities());
              break;
            }

            case GET_FILTER_CAPABILITIES: {
              TestEventQueueSingleton::get()->pushEvent(
                  GET_FILTER_CAPABILITIES, chreBleGetFilterCapabilities());
              break;
            }
          }
        }
      }
    }
  };

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  uint32_t capabilities;
  sendEventToNanoapp(appId, GET_CAPABILITIES);
  test->waitForEvent(GET_CAPABILITIES, &capabilities);
  ASSERT_EQ(capabilities, CHRE_BLE_CAPABILITIES_SCAN |
                              CHRE_BLE_CAPABILITIES_SCAN_RESULT_BATCHING |
                              CHRE_BLE_CAPABILITIES_SCAN_FILTER_BEST_EFFORT);

  sendEventToNanoapp(appId, GET_FILTER_CAPABILITIES);
  test->waitForEvent(GET_FILTER_CAPABILITIES, &capabilities);
  ASSERT_EQ(capabilities, CHRE_BLE_FILTER_CAPABILITIES_RSSI |
                              CHRE_BLE_FILTER_CAPABILITIES_SERVICE_DATA);
}

TEST_F(BleTest, BleCapabilitiesTest) {
  doBleCapabilitiesTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleCapabilitiesTest) {
  doBleCapabilitiesTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleCapabilitiesTestForeground) {
  doBleCapabilitiesTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

/**
 * This test validates the case in which a nanoapp starts a scan, receives
 * at least one advertisement event, and stops a scan.
 */
void doBleSimpleScanTest(TestBase *test, int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  assertStartScanSuccess(test, appId);
  test->waitForEvent(CHRE_EVENT_BLE_ADVERTISEMENT);
  assertStopScanSuccess(test, appId);
}

TEST_F(BleTest, BleSimpleScanTest) {
  doBleSimpleScanTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleSimpleScanTest) {
  doBleSimpleScanTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleSimpleScanTestForeground) {
  doBleSimpleScanTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doBleStopScanOnUnload(TestBase *test, int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));
  assertStartScanSuccess(test, appId);
  test->unloadNanoapp(appId);
  ASSERT_FALSE(chrePalIsBleEnabled());
}

TEST_F(BleTest, BleStopScanOnUnload) {
  doBleStopScanOnUnload(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleStopScanOnUnload) {
  doBleStopScanOnUnload(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleStopScanOnUnloadForeground) {
  doBleStopScanOnUnload(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

/**
 * This test validates that a nanoapp can start a scan twice and the platform
 * will be enabled.
 */
void doBleStartTwiceScanTest(TestBase *test, int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  assertStartScanSuccess(test, appId);  // First scan
  assertStartScanSuccess(test, appId);  // Second scan

  test->waitForEvent(CHRE_EVENT_BLE_ADVERTISEMENT);

  assertStopScanSuccess(test, appId);
}

TEST_F(BleTest, BleStartTwiceScanTest) {
  doBleStartTwiceScanTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleStartTwiceScanTest) {
  doBleStartTwiceScanTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleStartTwiceScanTestForeground) {
  doBleStartTwiceScanTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

/**
 * This test validates that a nanoapp can request to stop a scan twice without
 * any ongoing scan existing. It asserts that the nanoapp did not receive any
 * advertisment events because a scan was never started.
 */
void doBleStopTwiceScanTest(TestBase *test, int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));
  assertStopScanSuccess(test, appId);
  assertStopScanSuccess(test, appId);
  test->unloadNanoapp(appId);
}

TEST_F(BleTest, BleStopTwiceScanTest) {
  doBleStopTwiceScanTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleStopTwiceScanTest) {
  doBleStopTwiceScanTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleStopTwiceScanTestForeground) {
  doBleStopTwiceScanTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

/**
 * This test verifies the following BLE settings behavior:
 * 1) Nanoapp makes BLE scan request
 * 2) Toggle BLE setting -> disabled
 * 3) Toggle BLE setting -> enabled.
 * 4) Verify things resume.
 */
void doBleSettingChangeTest(TestBase *test, int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  assertStartScanSuccess(test, appId);

  test->waitForEvent(CHRE_EVENT_BLE_ADVERTISEMENT);

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::BLE_AVAILABLE, false /* enabled */);
  bool enabled;
  test->waitForEvent(CHRE_EVENT_SETTING_CHANGED_BLE_AVAILABLE, &enabled);
  EXPECT_FALSE(enabled);
  EXPECT_FALSE(
      EventLoopManagerSingleton::get()->getSettingManager().getSettingEnabled(
          Setting::BLE_AVAILABLE));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_FALSE(chrePalIsBleEnabled());

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::BLE_AVAILABLE, true /* enabled */);
  test->waitForEvent(CHRE_EVENT_SETTING_CHANGED_BLE_AVAILABLE, &enabled);
  EXPECT_TRUE(enabled);
  EXPECT_TRUE(
      EventLoopManagerSingleton::get()->getSettingManager().getSettingEnabled(
          Setting::BLE_AVAILABLE));
  test->waitForEvent(CHRE_EVENT_BLE_ADVERTISEMENT);
  EXPECT_TRUE(chrePalIsBleEnabled());

  assertStopScanSuccess(test, appId);
  test->unloadNanoapp(appId);
}

TEST_F(BleTest, BleSettingChangeTest) {
  doBleSettingChangeTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleSettingChangeTest) {
  doBleSettingChangeTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleSettingChangeTestForeground) {
  doBleSettingChangeTest(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

/**
 * Test that a nanoapp receives a function disabled error if it attempts to
 * start a scan when the BLE setting is disabled.
 */
void doBleSettingDisabledStartScanTest(TestBase *test,
                                       int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::BLE_AVAILABLE, /* enable= */ false);

  bool enabled;
  test->waitForEvent(CHRE_EVENT_SETTING_CHANGED_BLE_AVAILABLE, &enabled);
  EXPECT_FALSE(enabled);

  bool success;
  sendEventToNanoapp(appId, START_SCAN);
  test->waitForEvent(START_SCAN, &success);
  EXPECT_TRUE(success);
  chreAsyncResult result{};
  test->waitForEvent(CHRE_EVENT_BLE_ASYNC_RESULT, &result);
  EXPECT_EQ(result.errorCode, CHRE_ERROR_FUNCTION_DISABLED);
}

TEST_F(BleTest, BleSettingDisabledStartScanTest) {
  doBleSettingDisabledStartScanTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleSettingDisabledStartScanTest) {
  doBleSettingDisabledStartScanTest(this,
                                    NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleSettingDisabledStartScanTestForeground) {
  doBleSettingDisabledStartScanTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

/**
 * Test that a nanoapp receives a success response when it attempts to stop a
 * BLE scan while the BLE setting is disabled.
 */
void doBleSettingDisabledStopScanTest(TestBase *test,
                                      int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::BLE_AVAILABLE, /* enable= */ false);

  bool enabled;
  test->waitForEvent(CHRE_EVENT_SETTING_CHANGED_BLE_AVAILABLE, &enabled);
  EXPECT_FALSE(enabled);

  assertStopScanSuccess(test, appId);
}

TEST_F(BleTest, BleSettingDisabledStopScanTest) {
  doBleSettingDisabledStopScanTest(this,
                                   NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleSettingDisabledStopScanTest) {
  doBleSettingDisabledStopScanTest(this,
                                   NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleSettingDisabledStopScanTestForeground) {
  doBleSettingDisabledStopScanTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

/**
 * Test that a nanoapp can read RSSI successfully.
 */
void doBleReadRssi(TestBase *test, int8_t requestedThreadPriority) {
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::BLE_AVAILABLE, true /* enabled */);
  bool enabled;
  test->waitForEvent(CHRE_EVENT_SETTING_CHANGED_BLE_AVAILABLE, &enabled);
  ASSERT_TRUE(enabled);

  bool success;
  sendEventToNanoapp(appId, RSSI_REQUEST);
  test->waitForEvent(RSSI_REQUEST_SENT, &success);
  ASSERT_TRUE(success);
  chreBleReadRssiEvent event;
  test->waitForEvent(CHRE_EVENT_BLE_RSSI_READ, &event);
  ASSERT_EQ(event.result.errorCode, CHRE_ERROR_NONE);
}

TEST_F(BleTest, BleReadRssi) {
  doBleReadRssi(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleReadRssi) {
  doBleReadRssi(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleReadRssiForeground) {
  doBleReadRssi(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

/**
 * This test validates that a nanoapp can start call start scan twice before
 * receiving an async response. It should invalidate its original request by
 * calling start scan a second time.
 */
void doBleStartScanTwiceBeforeAsyncResponseTest(
    TestBase *test, int8_t requestedThreadPriority) {
  struct testData {
    void *cookie;
  };

  class App : public BleTestNanoapp {
   public:
    explicit App(TestNanoappInfo info = {}) : BleTestNanoapp(info) {}

    void handleEvent(uint32_t, uint16_t eventType, const void *eventData) {
      switch (eventType) {
        case CHRE_EVENT_BLE_ASYNC_RESULT: {
          auto *event = static_cast<const struct chreAsyncResult *>(eventData);
          uint16_t type =
              (event->requestType == CHRE_BLE_REQUEST_TYPE_START_SCAN)
                  ? SCAN_STARTED
                  : SCAN_STOPPED;
          TestEventQueueSingleton::get()->pushEvent(type, *event);
          break;
        }
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case START_SCAN: {
              auto data = static_cast<testData *>(event->data);
              const bool success = chreBleStartScanAsyncV1_9(
                  CHRE_BLE_SCAN_MODE_AGGRESSIVE, 0, nullptr, data->cookie);
              TestEventQueueSingleton::get()->pushEvent(START_SCAN, success);
              break;
            }

            case STOP_SCAN: {
              auto data = static_cast<testData *>(event->data);
              const bool success = chreBleStopScanAsyncV1_9(data->cookie);
              TestEventQueueSingleton::get()->pushEvent(STOP_SCAN, success);
              break;
            }
          }
        }
      }
    }
  };

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));
  bool success;

  delayBleScanStart(true /* delay */);

  testData data;
  uint32_t cookieOne = 1;
  data.cookie = &cookieOne;
  sendEventToNanoapp(appId, START_SCAN, data);
  test->waitForEvent(START_SCAN, &success);
  EXPECT_TRUE(success);

  uint32_t cookieTwo = 2;
  data.cookie = &cookieTwo;
  sendEventToNanoapp(appId, START_SCAN, data);
  test->waitForEvent(START_SCAN, &success);
  EXPECT_TRUE(success);

  chreAsyncResult result;
  test->waitForEvent(SCAN_STARTED, &result);
  EXPECT_EQ(result.errorCode, CHRE_ERROR_OBSOLETE_REQUEST);
  EXPECT_EQ(result.cookie, &cookieOne);

  // Respond to the first scan request. CHRE will then attempt the next scan
  // request at which point the PAL should no longer delay the response.
  delayBleScanStart(false /* delay */);
  EXPECT_TRUE(startBleScan());

  test->waitForEvent(SCAN_STARTED, &result);
  EXPECT_EQ(result.errorCode, CHRE_ERROR_NONE);
  EXPECT_EQ(result.cookie, &cookieTwo);

  sendEventToNanoapp(appId, STOP_SCAN, data);
  test->waitForEvent(STOP_SCAN, &success);
  EXPECT_TRUE(success);
  test->waitForEvent(SCAN_STOPPED);
}

TEST_F(BleTest, BleStartScanTwiceBeforeAsyncResponseTest) {
  doBleStartScanTwiceBeforeAsyncResponseTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleStartScanTwiceBeforeAsyncResponseTest) {
  doBleStartScanTwiceBeforeAsyncResponseTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleStartScanTwiceBeforeAsyncResponseTestForeground) {
  doBleStartScanTwiceBeforeAsyncResponseTest(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

/**
 * This test validates that a nanoapp can call flush only when an existing scan
 * is enabled for the nanoapp. This test validates that batching will hold the
 * data and flush will send the batched data and then a flush complete event.
 */
void doBleFlush(TestBase *test, int8_t requestedThreadPriority) {
  CREATE_CHRE_TEST_EVENT(SAW_BLE_AD_AND_FLUSH_COMPLETE, 8);
  class App : public BleTestNanoapp {
   public:
    explicit App(TestNanoappInfo info = {}) : BleTestNanoapp(info) {}

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_ASYNC_RESULT: {
          auto *event = static_cast<const struct chreAsyncResult *>(eventData);
          if (event->errorCode == CHRE_ERROR_NONE) {
            uint16_t type =
                (event->requestType == CHRE_BLE_REQUEST_TYPE_START_SCAN)
                    ? SCAN_STARTED
                    : SCAN_STOPPED;
            TestEventQueueSingleton::get()->pushEvent(type);
          }
          break;
        }

        case CHRE_EVENT_BLE_ADVERTISEMENT: {
          mSawBleAdvertisementEvent = true;
          break;
        }

        case CHRE_EVENT_BLE_FLUSH_COMPLETE: {
          auto *event = static_cast<const struct chreAsyncResult *>(eventData);
          mSawFlushCompleteEvent = event->success && event->cookie == &mCookie;
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case START_SCAN: {
              const bool success = chreBleStartScanAsync(
                  CHRE_BLE_SCAN_MODE_AGGRESSIVE, 60000, nullptr);
              TestEventQueueSingleton::get()->pushEvent(START_SCAN, success);
              break;
            }

            case STOP_SCAN: {
              const bool success = chreBleStopScanAsync();
              TestEventQueueSingleton::get()->pushEvent(STOP_SCAN, success);
              break;
            }

            case CALL_FLUSH: {
              const bool success = chreBleFlushAsync(&mCookie);
              TestEventQueueSingleton::get()->pushEvent(CALL_FLUSH, success);
              break;
            }
          }
          break;
        }
      }

      if (mSawBleAdvertisementEvent && mSawFlushCompleteEvent) {
        TestEventQueueSingleton::get()->pushEvent(
            SAW_BLE_AD_AND_FLUSH_COMPLETE);
        mSawBleAdvertisementEvent = false;
        mSawFlushCompleteEvent = false;
      }
    }

   private:
    bool mSawBleAdvertisementEvent = false;
    bool mSawFlushCompleteEvent = false;
  };

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<App>(info));

  // Flushing before a scan should fail.
  bool success;
  sendEventToNanoapp(appId, CALL_FLUSH);
  test->waitForEvent(CALL_FLUSH, &success);
  ASSERT_FALSE(success);

  // Start a scan with batching.
  sendEventToNanoapp(appId, START_SCAN);
  test->waitForEvent(START_SCAN, &success);
  ASSERT_TRUE(success);
  test->waitForEvent(SCAN_STARTED);
  ASSERT_TRUE(chrePalIsBleEnabled());

  // Call flush again multiple times and get the complete event.
  // We should only receive data when flush is called as the batch
  // delay is extremely large.
  constexpr uint32_t kNumFlushCalls = 3;
  for (uint32_t i = 0; i < kNumFlushCalls; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    sendEventToNanoapp(appId, CALL_FLUSH);
    test->waitForEvent(CALL_FLUSH, &success);
    ASSERT_TRUE(success);

    // Wait for some data and a flush complete.
    // This ensures we receive both advertisement events
    // and a flush complete event. We are not guaranteed
    // that the advertisement events will come after
    // the CALL_FLUSH event or before. If they come
    // before, then they will be ignored. This
    // change allows the advertisement events to come
    // after during the normal expiration of the
    // batch timer, which is valid (call flush, get
    // any advertisement events, flush complete event
    // might get some advertisement events afterwards).
    test->waitForEvent(SAW_BLE_AD_AND_FLUSH_COMPLETE);
  }

  // Stop a scan.
  sendEventToNanoapp(appId, STOP_SCAN);
  test->waitForEvent(STOP_SCAN, &success);
  ASSERT_TRUE(success);
  test->waitForEvent(SCAN_STOPPED);
  ASSERT_FALSE(chrePalIsBleEnabled());

  // Flushing after a scan should fail.
  sendEventToNanoapp(appId, CALL_FLUSH);
  test->waitForEvent(CALL_FLUSH, &success);
  ASSERT_FALSE(success);
}

TEST_F(BleTest, BleFlush) {
  doBleFlush(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleFlush) {
  doBleFlush(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleFlushForeground) {
  doBleFlush(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doBleBatchCompleteViaDelayMs(TestBase *test,
                                  int8_t requestedThreadPriority) {
  uint32_t kReportDelayMs = 200;
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  // Start a scan with batching.
  assertStartScanSuccess(test, appId, kReportDelayMs);
  test->waitForEvent(CHRE_EVENT_BLE_SCAN_STATUS_CHANGE);

  // Batch complete must be called while flush is being called.
  chreBatchCompleteEvent batchCompleteEvent{};
  test->waitForEvent(CHRE_EVENT_BLE_BATCH_COMPLETE, &batchCompleteEvent);
  ASSERT_EQ(batchCompleteEvent.eventType, CHRE_EVENT_BLE_ADVERTISEMENT);

  // Stop a scan.
  assertStopScanSuccess(test, appId);
}

TEST_F(BleTest, BleBatchCompleteViaDelayMs) {
  doBleBatchCompleteViaDelayMs(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleBatchCompleteViaDelayMs) {
  doBleBatchCompleteViaDelayMs(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleBatchCompleteViaDelayMsForeground) {
  doBleBatchCompleteViaDelayMs(this,
                               NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doBleScanStatusChange(TestBase *test, int8_t requestedThreadPriority) {
  constexpr uint32_t kReportDelayMs = 123;

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  // Initial status check. Note that the first status change event is sent
  // before the nanoapp is loaded, so we check with getScanStatus.
  chreBleScanStatus status;
  sendEventToNanoapp(appId, GET_SCAN_STATUS);
  test->waitForEvent(GET_SCAN_STATUS, &status);
  EXPECT_FALSE(status.enabled);

  // Start scan and check for status change
  assertStartScanSuccess(test, appId, kReportDelayMs);
  test->waitForEvent(CHRE_EVENT_BLE_SCAN_STATUS_CHANGE, &status);
  EXPECT_TRUE(status.enabled);
  EXPECT_EQ(status.reportDelayMs, kReportDelayMs);

  // Stop scan and check for status change
  assertStopScanSuccess(test, appId);
  test->waitForEvent(CHRE_EVENT_BLE_SCAN_STATUS_CHANGE, &status);
  EXPECT_FALSE(status.enabled);

  test->unloadNanoapp(appId);
}

TEST_F(BleTest, BleScanStatusChange) {
  doBleScanStatusChange(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleScanStatusChange) {
  doBleScanStatusChange(this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleScanStatusChangeForeground) {
  doBleScanStatusChange(this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doBleScanStatusChangeWithSettingToggle(TestBase *test,
                                            int8_t requestedThreadPriority) {
  constexpr uint32_t kReportDelayMs = 456;

  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  assertStartScanSuccess(test, appId, kReportDelayMs);

  chreBleScanStatus status;
  test->waitForEvent(CHRE_EVENT_BLE_SCAN_STATUS_CHANGE, &status);
  EXPECT_TRUE(status.enabled);
  EXPECT_EQ(status.reportDelayMs, kReportDelayMs);

  // Disable BLE setting
  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::BLE_AVAILABLE, false /* enabled */);
  test->waitForEvent(CHRE_EVENT_BLE_SCAN_STATUS_CHANGE, &status);
  EXPECT_FALSE(status.enabled);

  // Enable BLE setting
  EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
      Setting::BLE_AVAILABLE, true /* enabled */);
  test->waitForEvent(CHRE_EVENT_BLE_SCAN_STATUS_CHANGE, &status);
  EXPECT_TRUE(status.enabled);
  EXPECT_EQ(status.reportDelayMs, kReportDelayMs);

  assertStopScanSuccess(test, appId);
  test->unloadNanoapp(appId);
}

TEST_F(BleTest, BleScanStatusChangeWithSettingToggle) {
  doBleScanStatusChangeWithSettingToggle(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleScanStatusChangeWithSettingToggle) {
  doBleScanStatusChangeWithSettingToggle(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleScanStatusChangeWithSettingToggleForeground) {
  doBleScanStatusChangeWithSettingToggle(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

void doBleScanStatusChangeWithDelayMsUpdate(TestBase *test,
                                            int8_t requestedThreadPriority) {
  constexpr uint32_t kBaseReportDelayMs = 200;
  constexpr uint32_t kFasterScanReportDelayMs = kBaseReportDelayMs - 100;
  TestNanoappInfo info;
  info.requestedThreadPriority = requestedThreadPriority;
  uint64_t appId = test->loadNanoapp(MakeUnique<BleTestNanoapp>(info));

  assertStartScanSuccess(test, appId, kBaseReportDelayMs);

  chreBleScanStatus status;
  test->waitForEvent(CHRE_EVENT_BLE_SCAN_STATUS_CHANGE, &status);
  EXPECT_TRUE(status.enabled);
  EXPECT_EQ(status.reportDelayMs, kBaseReportDelayMs);

  // request a more frequent scan
  assertStartScanSuccess(test, appId, kFasterScanReportDelayMs);

  test->waitForEvent(CHRE_EVENT_BLE_SCAN_STATUS_CHANGE, &status);
  EXPECT_TRUE(status.enabled);
  EXPECT_EQ(status.reportDelayMs, kFasterScanReportDelayMs);

  assertStopScanSuccess(test, appId);
  test->unloadNanoapp(appId);
}

TEST_F(BleTest, BleScanStatusChangeWithDelayMsUpdate) {
  doBleScanStatusChangeWithDelayMsUpdate(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleScanStatusChangeWithDelayMsUpdate) {
  doBleScanStatusChangeWithDelayMsUpdate(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_NORMAL);
}

TEST_F(BleTestMultiThread, BleScanStatusChangeWithDelayMsUpdateForeground) {
  doBleScanStatusChangeWithDelayMsUpdate(
      this, NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND);
}

}  // namespace
}  // namespace chre
