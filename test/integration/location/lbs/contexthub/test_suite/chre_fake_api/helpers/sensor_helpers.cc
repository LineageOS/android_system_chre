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

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/sensor_helpers.h"

#include <cstdint>
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_sensor_fake.h"

namespace lbs::contexthub::helpers {

ChreSensorFunctions::ChreSensorFunctions(
    const absl::flat_hash_map<uint8_t, std::optional<uint32_t>>
        &sensor_type_to_handle,
    const bool *sensor_configure_result, const bool *sensor_flush_async_result)
    : sensor_type_to_handle_(sensor_type_to_handle),
      sensor_configure_result_(sensor_configure_result),
      sensor_flush_async_result_(sensor_flush_async_result) {}

bool ChreSensorFunctions::SensorFindDefault(uint8_t sensor_type,
                                            uint32_t *handle) {
  if (!sensor_type_to_handle_.contains(sensor_type)) {
    return false;
  }

  std::optional<uint32_t> sensor_handle =
      sensor_type_to_handle_.at(sensor_type);

  if (sensor_handle.has_value()) {
    *handle = sensor_handle.value();
    return true;
  }

  return false;
}

bool ChreSensorFunctions::SensorConfigure(uint32_t /* sensorHandle */,
                                          SensorConfigureMode /* mode */,
                                          uint64_t /* interval */,
                                          uint64_t /* latency */) {
  return *sensor_configure_result_;
}

bool ChreSensorFunctions::SensorFlushAsync(uint32_t /* sensorHandle */,
                                           const void * /* cookie */) {
  return *sensor_flush_async_result_;
}

}  // namespace lbs::contexthub::helpers