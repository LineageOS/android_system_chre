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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_audio_fake.h"

#include <cstdint>
#include <cstdlib>

#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/assert.h"
#include "chre/platform/memory.h"
#include "chre/platform/system_time.h"
#include "chre/util/macros.h"
#include "chre_api/chre/re.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_re_fake.h"

using chre::EventLoopManager;
using chre::EventLoopManagerSingleton;
using lbs::contexthub::AudioSource;
using lbs::contexthub::AudioSourceStatus;
using lbs::contexthub::FakeChreApiProvider;

// Export API functions that can be faked using FakeChreAudioApi

DLL_EXPORT bool chreAudioGetSource(uint32_t handle, AudioSource* audioSource) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreAudioGetSource(
      handle, audioSource);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiAudioFunctions()
      ->GetSource(handle, audioSource);
}

DLL_EXPORT bool chreAudioConfigureSource(uint32_t handle, bool enable,
                                         uint64_t bufferDuration,
                                         uint64_t deliveryInterval) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreAudioConfigureSource(handle, enable, bufferDuration,
                                 deliveryInterval);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiAudioFunctions()
      ->ConfigureSource(handle, enable, bufferDuration, deliveryInterval);
}

DLL_EXPORT bool chreAudioGetStatus(uint32_t handle, AudioSourceStatus* status) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreAudioGetStatus(
      handle, status);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiAudioFunctions()
      ->GetStatus(handle, status);
}

namespace lbs {
namespace contexthub {

// Create the functions that perform what would actually be run in the linux
// simulator.

bool ChreApiAudioFunctions::GetSource(uint32_t handle,
                                      AudioSource* audioSource) {
#ifdef CHRE_AUDIO_SUPPORT_ENABLED
  bool success = false;
  if (audioSource != nullptr) {
    success = EventLoopManagerSingleton::get()
                  ->getAudioRequestManager()
                  .getAudioSource(handle, audioSource);
  }
  return success;
#else
  UNUSED_VAR(handle);
  UNUSED_VAR(audioSource);
  return false;
#endif  // CHRE_AUDIO_SUPPORT_ENABLED
}

bool ChreApiAudioFunctions::ConfigureSource(uint32_t handle, bool enable,
                                            uint64_t bufferDuration,
                                            uint64_t deliveryInterval) {
#ifdef CHRE_AUDIO_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getAudioRequestManager()
      .configureSource(nanoapp, handle, enable, bufferDuration,
                       deliveryInterval);
#else
  UNUSED_VAR(handle);
  UNUSED_VAR(enable);
  UNUSED_VAR(bufferDuration);
  UNUSED_VAR(deliveryInterval);
  return false;
#endif  // CHRE_AUDIO_SUPPORT_ENABLED
}

bool ChreApiAudioFunctions::GetStatus(uint32_t /* handle */,
                                      AudioSourceStatus* /* status */) {
  return false;
}

}  // namespace contexthub
}  // namespace lbs