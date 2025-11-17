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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_GNSS_HELPERS_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_GNSS_HELPERS_H_

#include <memory>

#include "absl/memory/memory.h"
#include "chre_api/chre/gnss.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_gnss_fake.h"

// This file provides helper functions that overrides gnss CHRE API calls with
// their common uses in unit tests.
namespace lbs {
namespace contexthub {
namespace helpers {

class ChreGnssFullCapabilities : public ChreApiGnssFunctions {
  uint32_t GetCapabilities() override {
    return CHRE_GNSS_CAPABILITIES_LOCATION |
           CHRE_GNSS_CAPABILITIES_MEASUREMENTS |
           CHRE_GNSS_CAPABILITIES_GNSS_ENGINE_BASED_PASSIVE_LISTENER;
  }

  bool LocationSessionStartAsync(uint32_t /* minIntervalMs */,
                                 uint32_t /* minTimeToNextFixMs */,
                                 const void * /* cookie */) override {
    return true;
  }

  bool LocationSessionStopAsync(const void * /* cookie */) override {
    return true;
  }

  bool MeasurementSessionStartAsync(uint32_t /* minIntervalMs */,
                                    const void * /* cookie */) override {
    return true;
  }

  bool MeasurementSessionStopAsync(const void * /* cookie */) override {
    return true;
  }

  bool ConfigurePassiveLocationListener(bool /* enable */) override {
    return true;
  }
};

class ChreGnssNoCapabilities : public ::lbs::contexthub::ChreApiGnssFunctions {
  uint32_t GetCapabilities() override {
    return CHRE_GNSS_CAPABILITIES_NONE;
  }

  bool LocationSessionStartAsync(uint32_t /* minIntervalMs */,
                                 uint32_t /* minTimeToNextFixMs */,
                                 const void * /* cookie */) override {
    return false;
  }

  bool LocationSessionStopAsync(const void * /* cookie */) override {
    return false;
  }

  bool MeasurementSessionStartAsync(uint32_t /* minIntervalMcls */,
                                    const void * /* cookie */) override {
    return false;
  }

  bool MeasurementSessionStopAsync(const void * /* cookie */) override {
    return false;
  }

  bool ConfigurePassiveLocationListener(bool /* enable */) override {
    return false;
  }
};

inline void SetGnssFullCapabilities() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreGnssFullCapabilities>());
}
inline void SetGnssNoCapabilities() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreGnssNoCapabilities>());
}

}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_GNSS_HELPERS_H_