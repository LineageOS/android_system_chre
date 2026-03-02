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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_wifi_fake.h"

#include <cstdint>
#include <cstdlib>

#include "absl/base/nullability.h"
#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/platform/assert.h"
#include "chre/platform/memory.h"
#include "chre/platform/system_time.h"
#include "chre/util/macros.h"
#include "chre_api/chre/wifi.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

using chre::EventLoopManager;
using chre::EventLoopManagerSingleton;
using lbs::contexthub::FakeChreApiProvider;

// Export API functions that can be faked using FakeChreWifiApi

DLL_EXPORT uint32_t chreWifiGetCapabilities() {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreWifiGetCapabilities();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiWifiFunctions()
      ->GetCapabilities();
}

DLL_EXPORT bool chreWifiConfigureScanMonitorAsync(
    bool enable, const void* absl_nullable cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreWifiConfigureScanMonitorAsync(enable, cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiWifiFunctions()
      ->ConfigureScanMonitorAsync(enable, cookie);
}

DLL_EXPORT bool chreWifiRequestScanAsync(const chreWifiScanParams* params,
                                         const void* absl_nullable cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreWifiRequestScanAsync(
          /*params=*/params, cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiWifiFunctions()
      ->RequestScanAsync(params, cookie);
}

DLL_EXPORT bool chreWifiRequestRangingAsync(const chreWifiRangingParams* params,
                                            const void* cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreWifiRequestRangingAsync(params, cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiWifiFunctions()
      ->RequestRangingAsync(params, cookie);
}

namespace lbs {
namespace contexthub {

// Create the functions that perform what would actually be run in the linux
// simulator.

uint32_t ChreApiWifiFunctions::GetCapabilities() {
#ifdef CHRE_WIFI_SUPPORT_ENABLED
  return chre::EventLoopManagerSingleton::get()
      ->getWifiRequestManager()
      .getCapabilities();
#else
  return CHRE_WIFI_CAPABILITIES_NONE;
#endif  // CHRE_WIFI_SUPPORT_ENABLED
}

bool ChreApiWifiFunctions::ConfigureScanMonitorAsync(bool enable,
                                                     const void* cookie) {
#ifdef CHRE_WIFI_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getWifiRequestManager()
      .configureScanMonitor(nanoapp, enable, cookie);
#else
  UNUSED_VAR(enable);
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_WIFI_SUPPORT_ENABLED
}

bool ChreApiWifiFunctions::RequestScanAsync(const chreWifiScanParams* params,
                                            const void* cookie) {
#ifdef CHRE_WIFI_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return (params == nullptr) ? false
                             : EventLoopManagerSingleton::get()
                                   ->getWifiRequestManager()
                                   .requestScan(nanoapp, params, cookie);
#else
  UNUSED_VAR(params);
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_WIFI_SUPPORT_ENABLED
}

bool ChreApiWifiFunctions::RequestRangingAsync(
    const chreWifiRangingParams* params, const void* cookie) {
#ifdef CHRE_WIFI_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getWifiRequestManager()
      .requestRanging(chre::WifiRequestManager::RangingType::WIFI_AP, nanoapp,
                      params, cookie);
#else
  UNUSED_VAR(params);
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_WIFI_SUPPORT_ENABLED
}

}  // namespace contexthub
}  // namespace lbs
