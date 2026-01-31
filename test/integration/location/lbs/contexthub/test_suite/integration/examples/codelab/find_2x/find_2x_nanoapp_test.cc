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
#include <cstdlib>
#include <vector>

#include <gtest/gtest.h>
#include "absl/flags/flag.h"
#include "chre_api/chre.h"
#include "location/lbs/contexthub/test_suite/integration/chre_integration_lib.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/data_feed_base.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/examples/codelab/find_2x/find_2x_common.h"
#include "location/lbs/contexthub/test_suite/integration/verify/verification_data.h"

using lbs::contexthub::testing::verify::GetHostMessages;

namespace {

class ScenarioFour : public lbs::contexthub::testing::DataFeedBase {
 public:
  explicit ScenarioFour() { skip_initial_message_from_host_ = true; }

  uint32_t GetCapabilitiesBle() override { return CHRE_BLE_CAPABILITIES_NONE; }

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
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  const std::vector<chreSensorInfo> GetSensors() override { return {}; }

  void ReceivedMessageFromNanoapp(
      uint64_t t_ns, const SafeChreMessageToHostData& message) override;
};

void ScenarioFour::ReceivedMessageFromNanoapp(
    uint64_t t_ns, const SafeChreMessageToHostData& message) {
  if (message.messageType == kFind2xRequestType) {
    const int* to_double = static_cast<const int*>(message.message);
    int* doubled_int = static_cast<int*>(malloc(sizeof(int)));
    *doubled_int = *to_double * 2;

    SafeChreMessageFromHostData response;
    response.appId = 0x12345600000;
    response.messageType = kFind2xResponseType;
    response.messageSize = sizeof(int);
    response.message = doubled_int;
    response.should_fragment = false;
    response.hostEndpoint = 1234;

    messages_to_chre_[t_ns + 3] = response;
  }
}

INTEGRATION_TEST(NanoappTest, ScenarioFour, ScenarioFourTest) {
  auto msgs = GetHostMessages();

  ASSERT_GT(msgs.size(), 1);

  // verify message types and that they didn't arrive at the same time.
  EXPECT_EQ(msgs[0].second.messageType, kFind2xRequestType);
  EXPECT_EQ(msgs[1].second.messageType, kFind2xResponseSuccessStatusType);
  EXPECT_LT(msgs[0].first, msgs[1].first);

  // verify that the second message says true, not false.
  const bool* verify_result = static_cast<const bool*>(msgs[1].second.message);
  EXPECT_TRUE(*verify_result);
}

}  // namespace