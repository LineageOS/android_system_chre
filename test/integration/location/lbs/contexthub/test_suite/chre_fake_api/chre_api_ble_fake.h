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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_BLE_FAKE_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_BLE_FAKE_H_
#include <cstdint>

#include "chre_api/chre/ble.h"

namespace lbs {
namespace contexthub {

using BleScanMode = chreBleScanMode;
using BleScanFilter = chreBleScanFilter;

// This class can be used in conjunction with
// FakeChreApiProvider::SetChreApiFunctions to overwrite what the CHRE ble API
// functions return in unit tests and when run on the CHRE simulator.
class ChreApiBleFunctions {
 public:
  virtual ~ChreApiBleFunctions() = default;

  // These APIs are defined at
  // chre_api/include/chre_api/chre/ble.h
  virtual uint32_t GetCapabilities();
  virtual uint32_t GetFilterCapabilities();
  virtual bool StartScanAsync(BleScanMode mode, uint32_t reportDelayMs,
                              const BleScanFilter *filter);
  virtual bool StartScanAsyncV1_9(BleScanMode mode, uint32_t reportDelayMs,
                                  const chreBleScanFilterV1_9 *filter,
                                  const void *cookie);
  virtual bool StopScanAsync();
  virtual bool StopScanAsyncV1_9(const void *cookie);
  virtual bool FlushAsync(const void *cookie);
  virtual bool ReadRssiAsync(uint16_t connectionHandle, const void *cookie);
  virtual bool GetScanStatus(struct chreBleScanStatus *status);
  virtual bool SocketAccept(uint64_t socketId);
  virtual int32_t SocketSend(uint64_t socketId, const void *data,
                             uint16_t length,
                             chreBleSocketPacketFreeFunction *freeCallback);
};

}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_BLE_FAKE_H_
