/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_ANDROID_PLATFORM_SENSOR_MANAGER_BASE_H_
#define CHRE_PLATFORM_ANDROID_PLATFORM_SENSOR_MANAGER_BASE_H_

#include "chre/pal/sensor.h"

#include <android/sensor.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace chre {

/**
 * Platform sensor manager base class.
 */
class PlatformSensorManagerBase {
 protected:
  ASensorManager *mSensorManager = nullptr;
  ASensorEventQueue *mSharedEventQueue = nullptr;
  ALooper *mLooper = nullptr;

  // The context containing all sensor
  struct SensorContext {
    struct chreSensorInfo sensorInfo;  // CHRE sensor info
    const ASensor *androidSensor;      // Android sensor instance
    int32_t androidSensorType;         // Android sensor type
    uint8_t chreSensorHandle;          // CHRE sensor handle
    bool enabled;                      // Is enabled to receive sensor data
    uint64_t intervalNs;  // Interval of processing sensor data in nanoseconds
    uint64_t latencyNs;   // Delay of processing sensor data in nanoseconds
  };
  std::vector<struct SensorContext> mSensorContextArray;

  // Map from Android sensor type to CHRE sensor handle in mSensorContextArray.
  std::unordered_map<int32_t, uint32_t> mSensorTypeToHandleMap;

  // Looper callback function.
  static int looperCallback(int fd, int events, void *data);

  // The struct used to store the CHRE event returned when processing a batch of
  // sensor events.
  struct Event {
    // The real event data.
    union {
      void *data = nullptr;  // Used for accessing void* type of this union.
      chreSensorThreeAxisData *threeAxisData;
      chreSensorFloatData *floatData;
    };
    // Context of the sensor that this event belongs to.
    struct SensorContext *context = nullptr;
    // The data sample size in the chre event.
    size_t dataSize = 0;
    // The index used to fill in the chre event.
    size_t currentIndex = 0;
    // The timestamp of the last events, used to fill timestampDelta in CHRE
    // events.
    uint64_t lastEventTimestamp = 0;
  };

  static void fillAccelerometerEvent(const ASensorEvent &event,
                                     Event &chreEvent);
  static void fillBarometerEvent(const ASensorEvent &event, Event &chreEvent);
};

}  // namespace chre

#endif  // CHRE_PLATFORM_ANDROID_PLATFORM_SENSOR_MANAGER_BASE_H_
