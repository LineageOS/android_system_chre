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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_WWAN_HELPERS_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_WWAN_HELPERS_H_

#include <memory>

#include "absl/memory/memory.h"
#include "chre_api/chre/wwan.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_wwan_fake.h"

// This file provides helper functions that overrides wwan CHRE API calls with
// their common uses in unit tests.
namespace lbs {
namespace contexthub {
namespace helpers {

class ChreWwanFullCapabilities : public ChreApiWwanFunctions {
  uint32_t GetCapabilities() override {
    return CHRE_WWAN_GET_CELL_INFO;
  }
  bool GetCellInfoAsync(const void * /* cookie */) override {
    return true;
  }
};

class ChreWwanNoCapabilities : public ChreApiWwanFunctions {
  uint32_t GetCapabilities() override {
    return CHRE_WWAN_CAPABILITIES_NONE;
  }
  bool GetCellInfoAsync(const void * /* cookie */) override {
    return false;
  }
};

inline void SetWwanFullCapabilities() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreWwanFullCapabilities>());
}
inline void SetWwanNoCapabilities() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreWwanNoCapabilities>());
}

}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_WWAN_HELPERS_H_