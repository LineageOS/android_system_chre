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
#include "location/lbs/contexthub/test_suite/integration/verify/verification_data.h"

using lbs::contexthub::testing::verify::GetHostMessages;

namespace {

const int kMessageType = 73;
const int kMessageValue = 42;
const int kMessageTime = 123;

class ScenarioOne : public lbs::contexthub::testing::DataFeedBase {
 public:
  explicit ScenarioOne();

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
};

ScenarioOne::ScenarioOne() {
  SafeChreMessageFromHostData host_msg;
  host_msg.appId = 0x12345600000;
  host_msg.messageType = kMessageType;
  host_msg.messageSize = sizeof(int);
  host_msg.hostEndpoint = 1234;

  // TODO: malloc a new int* and set its value to kMsesageValue. // NOLINT
  auto contents = static_cast<int*>(malloc(sizeof(int)));
  *contents = kMessageValue;
  host_msg.message = contents;

  // TODO: Add the host_msg at t = kMessageTime. // NOLINT
  messages_to_chre_[kMessageTime] = host_msg;
}

INTEGRATION_TEST(NanoappTest, ScenarioOne, ScenarioOneTest) {
  // all received host messages can be retrieved by calling GetHostMessages().
  // it returns a vector of pairs, with the first element of the pair being
  // the time the message is received, and the second being a
  // SafeChreMessageToHostData object.

  auto msgs = GetHostMessages();

  ASSERT_GT(msgs.size(), 0);
  EXPECT_EQ(msgs[0].first, kMessageTime);
  auto msg = msgs[0].second;
  ASSERT_EQ(msg.messageSize, sizeof(int));
  EXPECT_EQ(msg.messageType, kMessageType);
  auto msg_val = static_cast<const int*>(msg.message);
  EXPECT_EQ(*msg_val, kMessageValue);
}

}  // namespace
