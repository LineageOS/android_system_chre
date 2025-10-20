/*
 * Copyright (C) 2016 The Android Open Source Project
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

namespace chre {

uint8_t PlatformSensor::getSensorType() const {
  return mSensorInfo->sensorType;
}

uint64_t PlatformSensor::getMinInterval() const {
  return mSensorInfo->minInterval;
}

bool PlatformSensor::reportsBiasEvents() const {
  return mSensorInfo->reportsBiasEvents == 1;
}

bool PlatformSensor::supportsPassiveMode() const {
  return mSensorInfo->supportsPassiveMode == 1;
}

const char *PlatformSensor::getSensorName() const {
  return mSensorInfo->sensorName;
}

uint8_t PlatformSensor::getSensorIndex() const {
  return CHRE_SENSOR_INDEX_DEFAULT;
}

uint16_t PlatformSensor::getTargetGroupMask() const {
  return kDefaultTargetGroupMask;
}

void PlatformSensorBase::moveFrom(PlatformSensorBase &other) {
  // Don't call base class's move assignment operator in this class. It is not
  // safe because of the possibly duplication moves. The move assignment
  // operator may do things more than object moves, e.g. call base class's move
  // assignment operator.
  if (&other == this) {
    return;
  }

  mSensorHandle = other.mSensorHandle;
  mSensorInfo = other.mSensorInfo;
  other.mSensorInfo = nullptr;
}

PlatformSensor::PlatformSensor(PlatformSensor &&other) {
  moveFrom(other);
}

PlatformSensor &PlatformSensor::operator=(PlatformSensor &&other) {
  moveFrom(other);
  return *this;
}

}  // namespace chre
