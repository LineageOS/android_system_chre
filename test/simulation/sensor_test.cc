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

#include "chre_api/chre/sensor.h"

#include <cstdint>

#include "chre/core/event_loop_manager.h"
#include "chre/core/settings.h"
#include "chre/platform/linux/pal_sensor.h"
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

class SensorTest : public SingleThreadTestBase {};

// Validates that the default accelerometer sensor can be found.
TEST_F(SensorTest, FindDefaultSensor) {
  CREATE_CHRE_TEST_EVENT(FIND, 0);

  struct Configuration {
    uint8_t sensorType;
  };

  class App : public TestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case FIND: {
              auto config = static_cast<const Configuration *>(event->data);
              uint32_t handle;
              const bool success =
                  chreSensorFindDefault(config->sensorType, &handle);
              if (!success) {
                LOGE("Failed to find sensor type %" PRIu8, config->sensorType);
              }
              TestEventQueueSingleton::get()->pushEvent(FIND, success);
              break;
            }
          }
        }
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  Configuration config{.sensorType =
                           CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER};
  sendEventToNanoapp(appId, FIND, config);
  bool success;
  waitForEvent(FIND, &success);
  EXPECT_TRUE(success);
}

TEST_F(SensorTest, SensorCanSubscribeAndUnsubscribeToDataEvents) {
  CREATE_CHRE_TEST_EVENT(CONFIGURE, 0);

  struct Configuration {
    uint32_t sensorHandle;
    uint64_t interval;
    enum chreSensorConfigureMode mode;
  };

  class App : public TestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_SENSOR_SAMPLING_CHANGE: {
          auto *event =
              static_cast<const struct chreSensorSamplingStatusEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_SENSOR_SAMPLING_CHANGE, *event);
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case CONFIGURE: {
              auto config = static_cast<const Configuration *>(event->data);
              const bool success = chreSensorConfigure(
                  config->sensorHandle, config->mode, config->interval, 0);
              TestEventQueueSingleton::get()->pushEvent(CONFIGURE, success);
              break;
            }
          }
        }
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  bool success;

  EXPECT_FALSE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));

  Configuration config{.sensorHandle = 0,
                       .interval = CHRE_NSEC_PER_SEC,
                       .mode = CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS};
  sendEventToNanoapp(appId, CONFIGURE, config);
  waitForEvent(CONFIGURE, &success);
  EXPECT_TRUE(success);
  struct chreSensorSamplingStatusEvent event;
  waitForEvent(CHRE_EVENT_SENSOR_SAMPLING_CHANGE, &event);
  EXPECT_EQ(event.sensorHandle, config.sensorHandle);
  EXPECT_EQ(event.status.interval, config.interval);
  EXPECT_TRUE(event.status.enabled);
  EXPECT_TRUE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));

  config = {.sensorHandle = 0,
            .interval = 50,
            .mode = CHRE_SENSOR_CONFIGURE_MODE_DONE};
  sendEventToNanoapp(appId, CONFIGURE, config);
  waitForEvent(CONFIGURE, &success);
  EXPECT_TRUE(success);
  EXPECT_FALSE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));
}

TEST_F(SensorTest, SensorOneShot) {
  CREATE_CHRE_TEST_EVENT(CONFIGURE, 0);

  struct Configuration {
    uint64_t interval;
    enum chreSensorConfigureMode mode;
  };

  class App : public TestNanoapp {
   public:
    bool start() override {
      bool success = chreSensorFindDefault(CHRE_SENSOR_TYPE_SIGNIFICANT_MOTION,
                                           &mSignificantMotionHandle);
      if (!success) {
        LOGE("Failed to find significant motion sensor");
      }
      return success;
    }

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_SENSOR_SIGNIFICANT_MOTION_DATA: {
          TestEventQueueSingleton::get()->pushEvent(eventType);
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case CONFIGURE: {
              const auto *config =
                  static_cast<const Configuration *>(event->data);
              const bool success = chreSensorConfigure(
                  mSignificantMotionHandle, config->mode, config->interval, 0);
              TestEventQueueSingleton::get()->pushEvent(CONFIGURE, success);
              break;
            }
          }
        }
      }
    }

   private:
    uint32_t mSignificantMotionHandle;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  bool success;
  EXPECT_FALSE(chrePalSensorIsEnabled(/* sensorHandle= */ 1));
  Configuration config{.interval = CHRE_SENSOR_INTERVAL_DEFAULT,
                       .mode = CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT};
  sendEventToNanoapp(appId, CONFIGURE, config);
  waitForEvent(CONFIGURE, &success);
  EXPECT_TRUE(success);
  // It's not possible to guarantee that the sensor is enabled at the PAL in the
  // gTest context, since the one-shot sensor can be disabled when the nanoapp
  // processes the event. Since we know that the sensor is enabled if the
  // nanoapp did receive the event, so we skip the chrePalSensorIsEnabled check
  // here.

  waitForEvent(CHRE_EVENT_SENSOR_SIGNIFICANT_MOTION_DATA);
  EXPECT_FALSE(chrePalSensorIsEnabled(/* sensorHandle= */ 1));
}

TEST_F(SensorTest, SensorUnsubscribeToDataEventsOnUnload) {
  CREATE_CHRE_TEST_EVENT(CONFIGURE, 0);

  struct Configuration {
    uint32_t sensorHandle;
    uint64_t interval;
    enum chreSensorConfigureMode mode;
  };

  class App : public TestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_SENSOR_SAMPLING_CHANGE: {
          auto *event =
              static_cast<const struct chreSensorSamplingStatusEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_SENSOR_SAMPLING_CHANGE, *event);
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case CONFIGURE: {
              auto config = static_cast<const Configuration *>(event->data);
              const bool success = chreSensorConfigure(
                  config->sensorHandle, config->mode, config->interval, 0);
              TestEventQueueSingleton::get()->pushEvent(CONFIGURE, success);
              break;
            }
          }
        }
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  EXPECT_FALSE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));

  Configuration config{.sensorHandle = 0,
                       .interval = 10 * 1000 * 1000,  // 10 ms aka 100 Hz
                       .mode = CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS};
  sendEventToNanoapp(appId, CONFIGURE, config);
  bool success;
  waitForEvent(CONFIGURE, &success);
  EXPECT_TRUE(success);
  struct chreSensorSamplingStatusEvent event;
  waitForEvent(CHRE_EVENT_SENSOR_SAMPLING_CHANGE, &event);
  EXPECT_EQ(event.sensorHandle, config.sensorHandle);
  EXPECT_EQ(event.status.interval, config.interval);
  EXPECT_TRUE(event.status.enabled);
  EXPECT_TRUE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));

  unloadNanoapp(appId);
  EXPECT_FALSE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));
}

TEST_F(MultiThreadTestBase, MultiThreadedSensorTest) {
  CREATE_CHRE_TEST_EVENT(CONFIGURE, 0);

  struct Configuration {
    uint32_t sensorHandle;
    uint64_t interval;
    enum chreSensorConfigureMode mode;
  };

  class App : public TestNanoapp {
   public:
    App() = default;
    explicit App(TestNanoappInfo info) : TestNanoapp(info) {}

    bool start() {
      LOGI("Start: my id = 0x%" PRIx64, chreGetAppId());
      return true;
    }

    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_SENSOR_UNCALIBRATED_ACCELEROMETER_DATA: {
          break;
        }
        case CHRE_EVENT_SENSOR_SAMPLING_CHANGE: {
          auto *event =
              static_cast<const struct chreSensorSamplingStatusEvent *>(
                  eventData);
          LOGI("Got sampling intvl=%" PRIu64 ", my instance=%" PRIu16,
               event->status.interval, chreGetInstanceId());
          if (event->status.interval == mMyInterval) {
            TestEventQueueSingleton::get()->pushEvent(
                CHRE_EVENT_SENSOR_SAMPLING_CHANGE, *event);
          }
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case CONFIGURE: {
              auto config = static_cast<const Configuration *>(event->data);
              const bool success = chreSensorConfigure(
                  config->sensorHandle, config->mode, config->interval, 0);
              if (success) {
                mMyInterval = config->interval;
              }
              TestEventQueueSingleton::get()->pushEvent(CONFIGURE, success);
              break;
            }
          }
        }
      }
    }

   private:
    uint64_t mMyInterval = 0;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());
  TestNanoappInfo info;
  info.id = 0xfdceba987654321;
  info.requestedThreadPriority = NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND;
  uint64_t appId2 = loadNanoapp(MakeUnique<App>(info));

  EXPECT_FALSE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));

  bool success;
  Configuration config{.sensorHandle = 0,
                       .interval = 500 * chre::kOneMillisecondInNanoseconds,
                       .mode = CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS};
  sendEventToNanoapp(appId, CONFIGURE, config);
  waitForEvent(CONFIGURE, &success);
  EXPECT_TRUE(success);
  struct chreSensorSamplingStatusEvent event;
  waitForEvent(CHRE_EVENT_SENSOR_SAMPLING_CHANGE, &event);
  EXPECT_EQ(event.sensorHandle, config.sensorHandle);
  EXPECT_EQ(event.status.interval, config.interval);
  EXPECT_TRUE(event.status.enabled);
  EXPECT_TRUE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));

  Configuration config2{.sensorHandle = 0,
                        .interval = 100 * chre::kOneMillisecondInNanoseconds,
                        .mode = CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS};
  sendEventToNanoapp(appId2, CONFIGURE, config2);
  waitForEvent(CONFIGURE, &success);
  EXPECT_TRUE(success);
  waitForEvent(CHRE_EVENT_SENSOR_SAMPLING_CHANGE, &event);
  EXPECT_EQ(event.sensorHandle, config2.sensorHandle);
  EXPECT_EQ(event.status.interval, config2.interval);
  EXPECT_TRUE(event.status.enabled);
  EXPECT_TRUE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));

  config = {.sensorHandle = 0,
            .interval = 50,
            .mode = CHRE_SENSOR_CONFIGURE_MODE_DONE};
  sendEventToNanoapp(appId, CONFIGURE, config);
  waitForEvent(CONFIGURE, &success);
  EXPECT_TRUE(success);
  sendEventToNanoapp(appId2, CONFIGURE, config);
  waitForEvent(CONFIGURE, &success);
  EXPECT_TRUE(success);
  EXPECT_FALSE(chrePalSensorIsEnabled(/* sensorHandle= */ 0));
}

}  // namespace
}  // namespace chre
