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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_BLE_HELPERS_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_BLE_HELPERS_H_

#include <cstdint>
#include <memory>

#include "chre_api/chre/ble.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_ble_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

// This file provides helper functions that overrides ble CHRE API calls with
// their common uses in unit tests.
namespace lbs {
namespace contexthub {
namespace helpers {

class ChreBleNoCapabilities : public ChreApiBleFunctions {
  uint32_t GetCapabilities() override {
    return CHRE_BLE_CAPABILITIES_NONE;
  }

  uint32_t GetFilterCapabilities() override {
    return CHRE_BLE_FILTER_CAPABILITIES_NONE;
  }

  bool StartScanAsync(BleScanMode /* mode */, uint32_t /* reportDelayMs */,
                      const BleScanFilter * /* filter */) override {
    return false;
  }

  bool StopScanAsync() override {
    return false;
  }
};

inline void SetBleNoCapabilities() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreBleNoCapabilities>());
}

class ChreBleFullCapabilities : public ChreApiBleFunctions {
  uint32_t GetCapabilities() override {
    return CHRE_BLE_CAPABILITIES_SCAN |
           CHRE_BLE_CAPABILITIES_SCAN_RESULT_BATCHING |
           CHRE_BLE_CAPABILITIES_SCAN_FILTER_BEST_EFFORT |
           CHRE_BLE_CAPABILITIES_READ_RSSI;
  }

  uint32_t GetFilterCapabilities() override {
    return CHRE_BLE_FILTER_CAPABILITIES_RSSI |
           CHRE_BLE_FILTER_CAPABILITIES_SERVICE_DATA |
           CHRE_BLE_FILTER_CAPABILITIES_MANUFACTURER_DATA |
           CHRE_BLE_FILTER_CAPABILITIES_BROADCASTER_ADDRESS;
  }

  bool StartScanAsync(BleScanMode /* mode */, uint32_t /* reportDelayMs */,
                      const BleScanFilter * /* filter */) override {
    return true;
  }

  bool StopScanAsync() override {
    return true;
  }

  bool StartScanAsyncV1_9(BleScanMode /* mode */, uint32_t /* reportDelayMs */,
                          const chreBleScanFilterV1_9 * /* filter */,
                          const void * /* cookie */) override {
    return true;
  }

  bool StopScanAsyncV1_9(const void * /* cookie */) override {
    return true;
  }
};

inline void SetBleFullCapabilities() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreBleFullCapabilities>());
}

}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_BLE_HELPERS_H_