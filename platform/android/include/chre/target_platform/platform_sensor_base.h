/*
 * Copyright (C) 2025 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_ANDROID_PLATFORM_SENSOR_BASE_H_
#define CHRE_PLATFORM_ANDROID_PLATFORM_SENSOR_BASE_H_

#include "chre_api/chre/sensor.h"

#include <cstdint>

namespace chre {

/**
 * CHRE AP platform sensor base class.
 */
class PlatformSensorBase {
 public:
  /**
   * Initializes the members of PlatformSensorBase.
   */
  void initBase(const struct chreSensorInfo *sensorInfo,
                uint32_t sensorHandle) {
    mSensorInfo = sensorInfo;
    mSensorHandle = sensorHandle;
  }

  /**
   * Sets the sensor information of this sensor in the CHRE API format.
   *
   * @param sensorInfo Information about this sensor.
   */
  void setSensorInfo(const struct chreSensorInfo *sensorInfo) {
    mSensorInfo = sensorInfo;
  }

  const struct chreSensorInfo *getSensorInfo() const {
    return mSensorInfo;
  }

  void setSensorHandle(uint32_t sensorHandle) {
    mSensorHandle = sensorHandle;
  }

  uint32_t getSensorHandle() const {
    return mSensorHandle;
  }

 protected:
  void moveFrom(PlatformSensorBase &other);

 private:
  //! The sensor information for this sensor.
  const struct chreSensorInfo *mSensorInfo = nullptr;

  //! The sensor handle for this sensor.
  uint32_t mSensorHandle = 0;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_ANDROID_PLATFORM_SENSOR_BASE_H_
