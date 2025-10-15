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

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/wifi_helpers.h"

#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

namespace lbs {
namespace contexthub {
namespace helpers {
namespace {

TEST_NANOAPP(ChreWifiHelpersTest, FullCapabilitesWorks) {
  SetWifiFullCapabilities();

  EXPECT_EQ(chreWifiGetCapabilities(),
            CHRE_WIFI_CAPABILITIES_SCAN_MONITORING |
                CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN |
                CHRE_WIFI_CAPABILITIES_RADIO_CHAIN_PREF |
                CHRE_WIFI_CAPABILITIES_RTT_RANGING);
}

TEST_NANOAPP(ChreWifiHelpersTest, NoCapabilitesWorks) {
  SetWifiNoCapabilities();

  EXPECT_EQ(chreWifiGetCapabilities(), CHRE_WIFI_CAPABILITIES_NONE);
}

}  // namespace
}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs