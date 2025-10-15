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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_SENSOR_HELPERS_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_SENSOR_HELPERS_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_sensor_fake.h"

// This file provides helper functions that overrides wwan CHRE API calls with
// their common uses in unit tests.
namespace lbs::contexthub::helpers {

class ChreSensorEmptyClass : public ChreApiSensorFunctions {
  bool SensorFindDefault(uint8_t /* sensorType */,
                         uint32_t * /* handle */) override {
    return false;
  }
  bool SensorFind(uint8_t /* sensorType */, uint8_t /* sensorIndex */,
                  uint32_t * /* handle */) override {
    return false;
  }
  bool SensorGetInfo(uint32_t /* sensorHandle */,
                     SensorInfo * /* info */) override {
    return false;
  }
  bool SensorGetSamplingStatus(uint32_t /* sensorHandle */,
                               SamplingStatus * /* status */) override {
    return false;
  }
  bool SensorConfigure(uint32_t /* sensorHandle */,
                       SensorConfigureMode /* mode */, uint64_t /* interval */,
                       uint64_t /* latency */) override {
    return false;
  }
  bool SensorConfigureBiasEvents(uint32_t /* sensorHandle */,
                                 bool /* enable */) override {
    return false;
  }
  bool SensorGetThreeAxisBias(uint32_t /* sensorHandle */,
                              ThreeAxisData * /* bias */) override {
    return false;
  }
  bool SensorFlushAsync(uint32_t /* sensorHandle */,
                        const void * /* cookie */) override {
    return false;
  }
};

// SetEmptySensor is only intended to be used during shutdown of the nanoapp to
// bypass the loss of context. All functions return false in this class.
inline void SetEmptySensor() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreSensorEmptyClass>());
}

class ChreSensorTestClass : public ChreApiSensorFunctions {
  bool SensorFindDefault(uint8_t /* sensorType */, uint32_t *handle) override {
    *handle = 0;
    return true;
  }
  bool SensorFind(uint8_t /* sensorType */, uint8_t /* sensorIndex */,
                  uint32_t * /* handle */) override {
    return true;
  }
  bool SensorConfigure(uint32_t /* sensorHandle */,
                       SensorConfigureMode /* mode */, uint64_t /* interval */,
                       uint64_t /* latency */) override {
    return true;
  }
  bool SensorGetSamplingStatus(uint32_t /* sensorHandle*/,
                               SamplingStatus * /* status*/) override {
    return true;
  }
};

// SetTestSensor is only intended to be used in unit test. All functions return
// true in this class.
inline void SetTestSensor() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreSensorTestClass>());
}

// A fully extensible class that allows for setting the return values of the
// sensor functions. This currently supports SensorFindDefault,
// SensorConfigure, and SensorFlushAsync.
class ChreSensorFunctions : public ChreApiSensorFunctions {
 public:
  // The constructor takes in a map of sensor type to the desired handle and two
  // pointers to bools. The first bool pointer is used to set the return value
  // of SensorConfigure and the second bool pointer is used to set the return
  // value of SensorFlushAsync. These pointers are not owned by the class and
  // must outlive it.
  explicit ChreSensorFunctions(
      const absl::flat_hash_map<uint8_t, std::optional<uint32_t>>
          &sensor_type_to_handle,
      const bool *sensor_configure_result,
      const bool *sensor_flush_async_result);

  bool SensorFindDefault(uint8_t sensor_type, uint32_t *handle) override;

  bool SensorConfigure(uint32_t sensorHandle, SensorConfigureMode mode,
                       uint64_t interval, uint64_t latency) override;

  bool SensorFlushAsync(uint32_t sensorHandle, const void *cookie) override;

 private:
  absl::flat_hash_map<uint8_t, std::optional<uint32_t>> sensor_type_to_handle_;
  const bool *sensor_configure_result_;
  const bool *sensor_flush_async_result_;
};

}  // namespace lbs::contexthub::helpers

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_SENSOR_HELPERS_H_