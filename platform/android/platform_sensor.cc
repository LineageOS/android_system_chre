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

#include "chre/platform/platform_sensor.h"
#include "include/chre/target_platform/platform_sensor_base.h"

namespace chre {

uint8_t PlatformSensor::getSensorType() const {
  return getSensorInfo()->sensorType;
}

uint64_t PlatformSensor::getMinInterval() const {
  return getSensorInfo()->minInterval;
}

bool PlatformSensor::reportsBiasEvents() const {
  return getSensorInfo()->reportsBiasEvents == 1;
}

bool PlatformSensor::supportsPassiveMode() const {
  return getSensorInfo()->supportsPassiveMode == 1;
}

const char *PlatformSensor::getSensorName() const {
  return getSensorInfo()->sensorName;
}

uint8_t PlatformSensor::getSensorIndex() const {
  return CHRE_SENSOR_INDEX_DEFAULT;
}

uint16_t PlatformSensor::getTargetGroupMask() const {
  return kDefaultTargetGroupMask;
}

void PlatformSensorBase::moveFrom(PlatformSensorBase &other) {
  if (&other == this) {
    return;
  }

  // Note: if this implementation is ever changed to depend on "this" containing
  // initialized values, the move constructor implementation must be updated.
  setSensorHandle(other.getSensorHandle());
  setSensorInfo(other.getSensorInfo());
  other.setSensorInfo(nullptr);
}

PlatformSensor::PlatformSensor(PlatformSensor &&other) {
  moveFrom(other);
}

PlatformSensor &PlatformSensor::operator=(PlatformSensor &&other) {
  moveFrom(other);
  return *this;
}

}  // namespace chre
