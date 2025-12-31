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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_wwan_fake.h"

#include <cstdint>

#include "absl/base/nullability.h"
#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/core/wwan_request_manager.h"
#include "chre/util/macros.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

using chre::EventLoopManager;
using chre::EventLoopManagerSingleton;
using lbs::contexthub::FakeChreApiProvider;

// Export API functions that can be faked using FakeChreWwanApi

DLL_EXPORT uint32_t chreWwanGetCapabilities() {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreWwanGetCapabilities();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiWwanFunctions()
      ->GetCapabilities();
}

DLL_EXPORT bool chreWwanGetCellInfoAsync(const void* absl_nullable cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreWwanGetCellInfoAsync(cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiWwanFunctions()
      ->GetCellInfoAsync(cookie);
}

namespace lbs {
namespace contexthub {

// Create the functions that perform what would actually be run in the linux
// simulator.

uint32_t ChreApiWwanFunctions::GetCapabilities() {
#ifdef CHRE_WWAN_SUPPORT_ENABLED
  return chre::EventLoopManagerSingleton::get()
      ->getWwanRequestManager()
      .getCapabilities();
#else
  return CHRE_WWAN_CAPABILITIES_NONE;
#endif  // CHRE_WWAN_SUPPORT_ENABLED
}

bool ChreApiWwanFunctions::GetCellInfoAsync(const void* cookie) {
#ifdef CHRE_WWAN_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return chre::EventLoopManagerSingleton::get()
      ->getWwanRequestManager()
      .requestCellInfo(nanoapp, cookie);
#else
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_WWAN_SUPPORT_ENABLED
}

}  // namespace contexthub
}  // namespace lbs