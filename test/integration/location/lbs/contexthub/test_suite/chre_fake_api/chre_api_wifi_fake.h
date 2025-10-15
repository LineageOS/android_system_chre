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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_WIFI_FAKE_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_WIFI_FAKE_H_

#include <chre.h>

#include <cstdint>

namespace lbs {
namespace contexthub {

// This class can be used in conjunction with
// FakeChreApiProvider::SetChreApiFunctions to overwrite what the CHRE WIFI API
// functions return in unit tests and when run on the CHRE simulator.
class ChreApiWifiFunctions {
 public:
  virtual ~ChreApiWifiFunctions() = default;

  // These APIs are defined at
  virtual uint32_t GetCapabilities();
  virtual bool ConfigureScanMonitorAsync(bool enable, const void *cookie);
  virtual bool RequestScanAsync(const chreWifiScanParams *params,
                                const void *cookie);
  virtual bool RequestRangingAsync(const chreWifiRangingParams *params,
                                   const void *cookie);
};

}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_WIFI_FAKE_H_