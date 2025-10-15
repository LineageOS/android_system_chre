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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_AUDIO_HELPERS_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_AUDIO_HELPERS_H_

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_audio_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

// This file provides helper functions that overrides audio CHRE API calls with
// their common uses in unit tests.
namespace lbs {
namespace contexthub {
namespace helpers {

class ChreAudioEmptyClass : public ChreApiAudioFunctions {
  bool GetSource(uint32_t /* handle */,
                 AudioSource * /* audioSource */) override {
    return false;
  }
  bool ConfigureSource(uint32_t /* handle */, bool /* enable */,
                       uint64_t /* bufferDuration */,
                       uint64_t /* deliveryInterval */) override {
    return false;
  }
  bool GetStatus(uint32_t /* handle */,
                 AudioSourceStatus * /* status */) override {
    return false;
  }
};

// SetEmptyAudio is only intended to be used during shutdown of the nanoapp to
// bypass the loss of context. All functions are unavailable in this class.
inline void SetEmptyAudio() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreAudioEmptyClass>());
}

class ChreAudioSupportedClass : public ChreApiAudioFunctions {
  bool GetSource(uint32_t /* handle */, AudioSource *audioSource) override {
    audioSource->name = "TEST";
    return true;
  }
  bool ConfigureSource(uint32_t /* handle */, bool /* enable */,
                       uint64_t /* bufferDuration */,
                       uint64_t /* deliveryInterval */) override {
    return true;
  }
  bool GetStatus(uint32_t /* handle */,
                 AudioSourceStatus * /* status */) override {
    return true;
  }
};

// SetAudioSupported is only intended to be used in unit test which requires
// CHRE support. Basic functions are unavailable in this class.
inline void SetAudioSupported() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreAudioSupportedClass>());
}

}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_AUDIO_HELPERS_H_