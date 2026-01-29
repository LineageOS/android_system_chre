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
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "absl/flags/flag.h"
#include "chre_api/chre.h"
#include "location/lbs/contexthub/test_suite/integration/chre_integration_lib.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/data_feed_base.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/verify/verification_data.h"

using lbs::contexthub::testing::kMillisToNano;
using lbs::contexthub::testing::verify::GetHostMessages;

namespace {

const int kExpectedMessageArrivalTime = 1000 * kMillisToNano;

class ScenarioTwo : public lbs::contexthub::testing::DataFeedBase {
 public:
  explicit ScenarioTwo() { skip_initial_message_from_host_ = true; }

  uint32_t GetCapabilitiesBle() override { return CHRE_BLE_CAPABILITIES_NONE; }

  uint32_t GetFilterCapabilitiesBle() override {
    return CHRE_BLE_FILTER_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_LOCATION;
  }

  uint32_t GetCapabilitiesWwan() override {
    return CHRE_WWAN_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  const std::vector<chreSensorInfo> GetSensors() override { return {}; }

  SafeChreGnssLocationEvent* ReceivedGnssLocationEventRequestAtTime(
      uint64_t t_ns, uint32_t min_interval_ms,
      uint32_t min_time_to_next_fix_ms) override;
};

SafeChreGnssLocationEvent* ScenarioTwo::ReceivedGnssLocationEventRequestAtTime(
    uint64_t t_ns, uint32_t min_interval_ms,
    uint32_t /* min_time_to_next_fix_ms */)

{
  // at t = 0 lat, lng = 50. They change by one degree per second, lat
  // increasing and lng decreasing. Assume we always get a fix within
  // min_interval_ms of the request.
  auto time_of_fix = t_ns + min_interval_ms * kMillisToNano;
  auto curr_lat = 50 + time_of_fix / (1000 * kMillisToNano);
  auto curr_lng = 50 - time_of_fix / (1000 * kMillisToNano);
  auto ret = EmptyChreGnssLocationEvent(time_of_fix);
  ret->latitude_deg_e7 = curr_lat * 1e7;
  ret->longitude_deg_e7 = curr_lng * 1e7;

  return ret;
}

INTEGRATION_TEST(NanoappTest, ScenarioTwo, ScenarioTwoTest) {
  auto msgs = GetHostMessages();

  ASSERT_GT(msgs.size(), 0);

  // 10 fixes, each 100ms from each other => message received at 1sec.
  EXPECT_EQ(msgs[0].first, kExpectedMessageArrivalTime);
  auto msg = msgs[0].second;
  ASSERT_EQ(msg.messageSize, sizeof(std::pair<int32_t, int32_t>));
  auto msg_val = static_cast<const std::pair<int32_t, int32_t>*>(msg.message);
  EXPECT_EQ(msg_val->first, 1);    // change in lat is correct at 1deg/sec.
  EXPECT_EQ(msg_val->second, -1);  // change in lng is correct at -1deg/sec.
}

}  // namespace