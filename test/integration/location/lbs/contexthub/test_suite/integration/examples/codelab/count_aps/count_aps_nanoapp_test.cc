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

#include <cstdint>

#include <gtest/gtest.h>
#include "absl/flags/flag.h"
#include "chre_api/chre.h"
#include "location/lbs/contexthub/test_suite/integration/chre_integration_lib.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/data_feed_base.h"
#include "location/lbs/contexthub/test_suite/integration/verify/verification_data.h"

using lbs::contexthub::testing::kSecsToNano;
using lbs::contexthub::testing::verify::GetHostMessages;

namespace {

class ScenarioThree : public lbs::contexthub::testing::DataFeedBase {
 public:
  explicit ScenarioThree() {
    skip_initial_message_from_host_ = true;
    AddPassiveWifiScanAtTime(2 * kSecsToNano, 3);
    AddPassiveWifiScanAtTime(4 * kSecsToNano, 4);
    AddPassiveWifiScanAtTime(7 * kSecsToNano, 5);
    AddPassiveWifiScanAtTime(9 * kSecsToNano, 6);
    AddPassiveWifiScanAtTime(12 * kSecsToNano, 7);
  }

  uint32_t GetCapabilitiesBle() override {
    return CHRE_BLE_CAPABILITIES_NONE;
  }

  uint32_t GetFilterCapabilitiesBle() override {
    return CHRE_BLE_FILTER_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWwan() override {
    return CHRE_WWAN_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_SCAN_MONITORING;
  }

  uint32_t GetSensorCount() override {
    return 0;
  }

  uint32_t GetAudioSourceCount() override {
    return 0;
  }

 private:
  void AddPassiveWifiScanAtTime(uint64_t t_ns, int nb_of_aps);
};

void ScenarioThree::AddPassiveWifiScanAtTime(uint64_t t_ns, int nb_of_aps) {
  auto scan_event = EmptyChreWifiScanEvent(t_ns);
  scan_event->resultTotal = nb_of_aps;
  auto results = new chreWifiScanResult[nb_of_aps];
  scan_event->results = results;
  wifi_scan_events_[t_ns] = scan_event;
}

INTEGRATION_TEST(NanoappTest, ScenarioThree, ScenarioTwoTest) {
  auto msgs = GetHostMessages();

  ASSERT_GT(msgs.size(), 0);

  // simply verify that the message first arrives when we send 6 APs.
  EXPECT_EQ(msgs[0].first, 9 * kSecsToNano);
}

}  // namespace
