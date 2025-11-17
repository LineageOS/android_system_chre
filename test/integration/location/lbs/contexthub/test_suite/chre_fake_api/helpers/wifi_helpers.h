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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_WIFI_HELPERS_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_WIFI_HELPERS_H_

#include <memory>

#include "absl/memory/memory.h"
#include "chre_api/chre/wifi.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_wifi_fake.h"

// This file provides helper functions that overrides gnss CHRE API calls with
// their common uses in unit tests.
namespace lbs {
namespace contexthub {
namespace helpers {

class ChreWifiFullCapabilities : public ChreApiWifiFunctions {
  uint32_t GetCapabilities() override {
    return CHRE_WIFI_CAPABILITIES_SCAN_MONITORING |
           CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN |
           CHRE_WIFI_CAPABILITIES_RADIO_CHAIN_PREF |
           CHRE_WIFI_CAPABILITIES_RTT_RANGING;
  }

  bool ConfigureScanMonitorAsync(bool /* enable */,
                                 const void * /* cookie */) override {
    return true;
  }

  bool RequestScanAsync(const struct chreWifiScanParams * /* params */,
                        const void * /* cookie */) override {
    return true;
  }

  bool RequestRangingAsync(const struct chreWifiRangingParams * /* params */,
                           const void * /* cookie */) override {
    return true;
  }
};

class ChreWifiNoCapabilities : public ChreApiWifiFunctions {
  uint32_t GetCapabilities() override {
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  bool ConfigureScanMonitorAsync(bool /* enable */,
                                 const void * /* cookie */) override {
    return false;
  }

  bool RequestScanAsync(const struct chreWifiScanParams * /* params */,
                        const void * /* cookie */) override {
    return false;
  }

  bool RequestRangingAsync(const struct chreWifiRangingParams * /* params */,
                           const void * /* cookie */) override {
    return false;
  }
};

inline void SetWifiFullCapabilities() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreWifiFullCapabilities>());
}
inline void SetWifiNoCapabilities() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreWifiNoCapabilities>());
}

}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_WIFI_HELPERS_H_