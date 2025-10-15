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

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/ble_helpers.h"

#include "chre_api/chre.h"
#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

namespace lbs {
namespace contexthub {
namespace helpers {
namespace {

TEST_NANOAPP(ChreBleHelpersTest, NoCapabilitesWorks) {
  SetBleNoCapabilities();

  EXPECT_EQ(chreBleGetCapabilities(), CHRE_BLE_CAPABILITIES_NONE);
  EXPECT_EQ(chreBleGetFilterCapabilities(), CHRE_BLE_FILTER_CAPABILITIES_NONE);
  EXPECT_FALSE(chreBleStartScanAsync(BleScanMode::CHRE_BLE_SCAN_MODE_BACKGROUND,
                                     0 /* reportDelayMs */,
                                     nullptr /* filter */));
  EXPECT_FALSE(chreBleStopScanAsync());
}

TEST_NANOAPP(ChreBleHelpersTest, FullCapabilitesWorks) {
  SetBleFullCapabilities();

  EXPECT_EQ(chreBleGetCapabilities(),
            CHRE_BLE_CAPABILITIES_SCAN |
                CHRE_BLE_CAPABILITIES_SCAN_RESULT_BATCHING |
                CHRE_BLE_CAPABILITIES_SCAN_FILTER_BEST_EFFORT |
                CHRE_BLE_CAPABILITIES_READ_RSSI);
  EXPECT_EQ(chreBleGetFilterCapabilities(),
            CHRE_BLE_FILTER_CAPABILITIES_RSSI |
                CHRE_BLE_FILTER_CAPABILITIES_SERVICE_DATA |
                CHRE_BLE_FILTER_CAPABILITIES_MANUFACTURER_DATA |
                CHRE_BLE_FILTER_CAPABILITIES_BROADCASTER_ADDRESS);
  EXPECT_TRUE(chreBleStartScanAsync(BleScanMode::CHRE_BLE_SCAN_MODE_BACKGROUND,
                                    0 /* reportDelayMs */,
                                    nullptr /* filter */));
  EXPECT_TRUE(chreBleStopScanAsync());
}

}  // namespace
}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs