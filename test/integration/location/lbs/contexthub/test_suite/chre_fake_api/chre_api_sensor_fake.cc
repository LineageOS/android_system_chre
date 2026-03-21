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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_sensor_fake.h"

#include <cstdint>

#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre_api/chre.h"
#include "chre/util/macros.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

using chre::EventLoopManager;
using chre::EventLoopManagerSingleton;
using chre::getSensorModeFromEnum;
using chre::Nanoseconds;
using chre::SensorMode;
using chre::SensorRequest;

using lbs::contexthub::FakeChreApiProvider;
using lbs::contexthub::SamplingStatus;
using lbs::contexthub::SensorConfigureMode;
using lbs::contexthub::ThreeAxisData;

// Export API functions that can be faked using FakeChreSensorApi

DLL_EXPORT bool chreSensorFindDefault(uint8_t sensorType, uint32_t* handle) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreSensorFindDefault(
      sensorType, handle);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiSensorFunctions()
      ->SensorFindDefault(sensorType, handle);
}

DLL_EXPORT bool chreSensorFind(uint8_t sensorType, uint8_t sensorIndex,
                               uint32_t* handle) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreSensorFind(
      sensorType, sensorIndex, handle);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiSensorFunctions()
      ->SensorFind(sensorType, sensorIndex, handle);
}

DLL_EXPORT bool chreGetSensorInfo(uint32_t sensorHandle, chreSensorInfo* info) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreGetSensorInfo(
      sensorHandle, info);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiSensorFunctions()
      ->SensorGetInfo(sensorHandle, info);
}

DLL_EXPORT bool chreGetSensorSamplingStatus(uint32_t sensorHandle,
                                            SamplingStatus* status) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGetSensorSamplingStatus(sensorHandle, status);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiSensorFunctions()
      ->SensorGetSamplingStatus(sensorHandle, status);
}

DLL_EXPORT bool chreSensorConfigure(uint32_t sensorHandle,
                                    SensorConfigureMode mode, uint64_t interval,
                                    uint64_t latency) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreSensorConfigure(
      sensorHandle, mode, interval, latency);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiSensorFunctions()
      ->SensorConfigure(sensorHandle, mode, interval, latency);
}

DLL_EXPORT bool chreSensorConfigureBiasEvents(uint32_t sensorHandle,
                                              bool enable) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreSensorConfigureBiasEvents(sensorHandle, enable);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiSensorFunctions()
      ->SensorConfigureBiasEvents(sensorHandle, enable);
}

DLL_EXPORT bool chreSensorGetThreeAxisBias(uint32_t sensorHandle,
                                           ThreeAxisData* bias) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreSensorGetThreeAxisBias(sensorHandle, bias);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiSensorFunctions()
      ->SensorGetThreeAxisBias(sensorHandle, bias);
}

DLL_EXPORT bool chreSensorFlushAsync(uint32_t sensorHandle,
                                     const void* cookie) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreSensorFlushAsync(
      sensorHandle, cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiSensorFunctions()
      ->SensorFlushAsync(sensorHandle, cookie);
}

namespace lbs {
namespace contexthub {

// Create the functions that perform what would actually be run in the linux
// simulator.

bool ChreApiSensorFunctions::SensorFindDefault(uint8_t sensorType,
                                               uint32_t* handle) {
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getSensorRequestManager()
      .getSensorHandleForNanoapp(sensorType, CHRE_SENSOR_INDEX_DEFAULT,
                                 *nanoapp, handle);
#else
  UNUSED_VAR(sensorType);
  UNUSED_VAR(handle);
  return false;
#endif  // CHRE_SENSORS_SUPPORT_ENABLED
}

bool ChreApiSensorFunctions::SensorFind(uint8_t sensorType, uint8_t sensorIndex,
                                        uint32_t* handle) {
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getSensorRequestManager()
      .getSensorHandleForNanoapp(sensorType, sensorIndex, *nanoapp, handle);
#else
  UNUSED_VAR(sensorType);
  UNUSED_VAR(sensorIndex);
  UNUSED_VAR(handle);
  return false;
#endif  // CHRE_SENSORS_SUPPORT_ENABLED
}

bool ChreApiSensorFunctions::SensorGetInfo(uint32_t sensorHandle,
                                           SensorInfo* info) {
  CHRE_ASSERT(info);
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);

  bool success = false;
  if (info != nullptr) {
    success = EventLoopManagerSingleton::get()
                  ->getSensorRequestManager()
                  .getSensorInfo(sensorHandle, *nanoapp, info);
  }
  return success;
#else
  UNUSED_VAR(sensorHandle);
  UNUSED_VAR(info);
  return false;
#endif  // CHRE_SENSORS_SUPPORT_ENABLED
}

bool ChreApiSensorFunctions::SensorGetSamplingStatus(uint32_t sensorHandle,
                                                     SamplingStatus* status) {
  CHRE_ASSERT(status);
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  bool success = false;
  if (status != nullptr) {
    success = EventLoopManagerSingleton::get()
                  ->getSensorRequestManager()
                  .getSensorSamplingStatus(sensorHandle, status);
  }
  return success;
#else
  UNUSED_VAR(sensorHandle);
  UNUSED_VAR(status);
  return false;
#endif  // CHRE_SENSORS_SUPPORT_ENABLED
}

bool ChreApiSensorFunctions::SensorConfigure(uint32_t sensorHandle,
                                             SensorConfigureMode mode,
                                             uint64_t interval,
                                             uint64_t latency) {
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  SensorMode sensorMode = getSensorModeFromEnum(mode);
  SensorRequest sensorRequest(nanoapp->getInstanceId(), sensorMode,
                              Nanoseconds(interval), Nanoseconds(latency));
  return EventLoopManagerSingleton::get()
      ->getSensorRequestManager()
      .setSensorRequest(nanoapp, sensorHandle, sensorRequest);
#else
  UNUSED_VAR(sensorHandle);
  UNUSED_VAR(mode);
  UNUSED_VAR(interval);
  UNUSED_VAR(latency);
  return false;
#endif  // CHRE_SENSORS_SUPPORT_ENABLED
}

bool ChreApiSensorFunctions::SensorConfigureBiasEvents(uint32_t sensorHandle,
                                                       bool enable) {
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getSensorRequestManager()
      .configureBiasEvents(nanoapp, sensorHandle, enable);
}

bool ChreApiSensorFunctions::SensorGetThreeAxisBias(uint32_t sensorHandle,
                                                    ThreeAxisData* bias) {
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  return EventLoopManagerSingleton::get()
      ->getSensorRequestManager()
      .getThreeAxisBias(sensorHandle, bias);
#else
  UNUSED_VAR(sensorHandle);
  UNUSED_VAR(bias);
  return false;
#endif  // CHRE_SENSORS_SUPPORT_ENABLED
}

bool ChreApiSensorFunctions::SensorFlushAsync(uint32_t sensorHandle,
                                              const void* cookie) {
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()->getSensorRequestManager().flushAsync(
      nanoapp, sensorHandle, cookie);
#else
  UNUSED_VAR(sensorHandle);
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_SENSORS_SUPPORT_ENABLED
}

}  // namespace contexthub
}  // namespace lbs
