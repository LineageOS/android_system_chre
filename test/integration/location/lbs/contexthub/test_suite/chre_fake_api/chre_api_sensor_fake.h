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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_SENSOR_FAKE_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_SENSOR_FAKE_H_

#include <cstdint>

#include "chre_api/chre.h"
#include "chre_api/chre/sensor.h"

namespace lbs::contexthub {

using ThreeAxisData = chreSensorThreeAxisData;
using SamplingStatus = chreSensorSamplingStatus;
using SensorConfigureMode = chreSensorConfigureMode;
using SensorInfo = chreSensorInfo;

// This class can be used in conjunction with
// FakeChreApiProvider::SetChreApiFunctions to overwrite what the CHRE Sensor
// API functions return in unit tests and when run on the CHRE simulator.
class ChreApiSensorFunctions {
 public:
  virtual ~ChreApiSensorFunctions() = default;

  // These APIs are defined at
  // chre_api/include/chre_api/chre/sensor.h
  virtual bool SensorFindDefault(uint8_t sensorType, uint32_t *handle);
  virtual bool SensorFind(uint8_t sensorType, uint8_t sensorIndex,
                          uint32_t *handle);
  virtual bool SensorGetInfo(uint32_t sensorHandle, SensorInfo *info);
  virtual bool SensorGetSamplingStatus(uint32_t sensorHandle,
                                       SamplingStatus *status);
  virtual bool SensorConfigure(uint32_t sensorHandle, SensorConfigureMode mode,
                               uint64_t interval, uint64_t latency);
  virtual bool SensorConfigureBiasEvents(uint32_t sensorHandle, bool enable);
  virtual bool SensorGetThreeAxisBias(uint32_t sensorHandle,
                                      ThreeAxisData *bias);
  virtual bool SensorFlushAsync(uint32_t sensorHandle, const void *cookie);
};

}  // namespace lbs::contexthub

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_SENSOR_FAKE_H_
