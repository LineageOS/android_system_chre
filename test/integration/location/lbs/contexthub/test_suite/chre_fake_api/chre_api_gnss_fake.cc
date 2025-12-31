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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_gnss_fake.h"

#include "absl/base/nullability.h"
#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/util/macros.h"
#include "chre_api/chre/re.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

using chre::EventLoopManager;
using chre::EventLoopManagerSingleton;
using chre::Milliseconds;
using lbs::contexthub::FakeChreApiProvider;

// Export API functions that can be faked using FakeChreReApi.

DLL_EXPORT uint32_t chreGnssGetCapabilities(void) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGnssGetCapabilities();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiGnssFunctions()
      ->GetCapabilities();
}

DLL_EXPORT bool chreGnssLocationSessionStartAsync(uint32_t minIntervalMs,
                                                  uint32_t minTimeToNextFixMs,
                                                  const void* cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGnssLocationSessionStartAsync(
          /*minIntervalMs=*/minIntervalMs,
          /*minTimeToNextFixMs=*/minTimeToNextFixMs,
          /*cookie=*/cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiGnssFunctions()
      ->LocationSessionStartAsync(minIntervalMs, minTimeToNextFixMs, cookie);
}

DLL_EXPORT bool chreGnssLocationSessionStopAsync(const void* cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGnssLocationSessionStopAsync(/*cookie=*/cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiGnssFunctions()
      ->LocationSessionStopAsync(cookie);
}

DLL_EXPORT bool chreGnssMeasurementSessionStartAsync(
    uint32_t minIntervalMs, const void* absl_nullable cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGnssMeasurementSessionStartAsync(/*minIntervalMs=*/minIntervalMs,
                                             /*cookie=*/cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiGnssFunctions()
      ->MeasurementSessionStartAsync(minIntervalMs, cookie);
}

DLL_EXPORT bool chreGnssMeasurementSessionStopAsync(
    const void* absl_nullable cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGnssMeasurementSessionStopAsync(/*cookie=*/cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiGnssFunctions()
      ->MeasurementSessionStopAsync(cookie);
}

DLL_EXPORT bool chreGnssConfigurePassiveLocationListener(bool enable) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGnssConfigurePassiveLocationListener(/*enable=*/enable);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiGnssFunctions()
      ->ConfigurePassiveLocationListener(enable);
}

namespace lbs {
namespace contexthub {
// Create the functions that perform what would actually be run in the linux
// simulator.

uint32_t ChreApiGnssFunctions::GetCapabilities() {
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  return EventLoopManagerSingleton::get()->getGnssManager().getCapabilities();
#else
  return CHRE_GNSS_CAPABILITIES_NONE;
#endif  // CHRE_GNSS_SUPPORT_ENABLED
}

bool ChreApiGnssFunctions::LocationSessionStartAsync(
    uint32_t minIntervalMs, uint32_t minTimeToNextFixMs, const void* cookie) {
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getGnssManager()
      .getLocationSession()
      .addRequest(nanoapp, Milliseconds(minIntervalMs),
                  Milliseconds(minTimeToNextFixMs), cookie);
#else
  UNUSED_VAR(minIntervalMs);
  UNUSED_VAR(minTimeToNextFixMs);
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_GNSS_SUPPORT_ENABLED
}

bool ChreApiGnssFunctions::LocationSessionStopAsync(const void* cookie) {
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getGnssManager()
      .getLocationSession()
      .removeRequest(nanoapp, cookie);
#else
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_GNSS_SUPPORT_ENABLED
}

bool ChreApiGnssFunctions::MeasurementSessionStartAsync(uint32_t minIntervalMs,
                                                        const void* cookie) {
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getGnssManager()
      .getMeasurementSession()
      .addRequest(nanoapp, Milliseconds(minIntervalMs),
                  /*minTimeToNext=*/Milliseconds(0), cookie);
#else
  UNUSED_VAR(minIntervalMs);
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_GNSS_SUPPORT_ENABLED
}

bool ChreApiGnssFunctions::MeasurementSessionStopAsync(const void* cookie) {
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getGnssManager()
      .getMeasurementSession()
      .removeRequest(nanoapp, cookie);
#else
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_GNSS_SUPPORT_ENABLED
}

bool ChreApiGnssFunctions::ConfigurePassiveLocationListener(bool enable) {
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getGnssManager()
      .configurePassiveLocationListener(nanoapp, enable);
#else
  UNUSED_VAR(enable);
  return false;
#endif  // CHRE_GNSS_SUPPORT_ENABLED
}

}  // namespace contexthub
}  // namespace lbs