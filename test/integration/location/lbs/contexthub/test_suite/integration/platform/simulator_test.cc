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

#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

#include <chre.h>
#include <stdlib.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "absl/base/casts.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "chre/pal/ble.h"
#include "chre/pal/gnss.h"
#include "chre/pal/sensor.h"
#include "chre/pal/wifi.h"
#include "chre/pal/wwan.h"
#include "chre/platform/shared/pal_system_api.h"
#include "chre/platform/system_timer.h"
#include "chre/util/time.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/data_feed_base.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/verify/verification_data.h"

namespace lbs::contexthub::testing {

namespace {

class MessageToHostDataClass : public DataFeedBase {
 public:
  explicit MessageToHostDataClass(int capabilities_setting) {
    if (capabilities_setting == 1) {
      skip_initial_message_from_host_ = true;
    } else if (capabilities_setting == 2) {
      SafeChreMessageFromHostData msg;
      msg.message = nullptr;
      msg.messageSize = 0;
      msg.messageType = 0;
      msg.hostEndpoint = 1234;
      messages_to_chre_[0] = msg;
    }
  }

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

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }
};

size_t CountOccurrences(std::string_view text, std::string_view sub) {
  if (sub.empty()) {
    return 0;
  }
  size_t count = 0;
  size_t pos = 0;
  while ((pos = text.find(sub, pos)) != std::string::npos) {
    ++count;
    pos += sub.length();
  }
  return count;
}

TEST(SimulatorVerifyTest, VerifyData_RequiresMessagesToSend) {
  std::stringstream buffer;
  std::cerr.rdbuf(buffer.rdbuf());

  MessageToHostDataClass no_message_data(0);
  EXPECT_FALSE(Simulator::VerifyValidData(&no_message_data));
  auto text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 1);
  EXPECT_GT(CountOccurrences(text, kVerifyDataMessageToSendError), 0);
  buffer.str("");

  MessageToHostDataClass skip_initial_message_data(1);
  EXPECT_TRUE(Simulator::VerifyValidData(&skip_initial_message_data));
  text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 0);
  buffer.str("");

  MessageToHostDataClass initial_message_data(2);
  EXPECT_TRUE(Simulator::VerifyValidData(&initial_message_data));
  text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 0);
  buffer.str("");
}

class BleDataClass : public DataFeedBase {
 public:
  explicit BleDataClass(int capabilities_setting)
      : capabilities_setting_(capabilities_setting) {
    skip_initial_message_from_host_ = true;
  }
  int capabilities_setting_;

  uint32_t GetCapabilitiesBle() override { return CHRE_BLE_CAPABILITIES_SCAN; }

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

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }

  SafeChreBleAdvertisementEvent* ReceivedBleAdvertisementEventRequestAtTime(
      uint64_t t, uint64_t /* latency */,
      const SafeChreBleScanFilter& /* filter */) override {
    if (capabilities_setting_ != 1) {
      return nullptr;
    }
    auto reports = new chreBleAdvertisingReport[1];
    reports[0].timestamp = t;
    SafeChreBleAdvertisementEvent* ret = EmptyChreBleAdvertisementEvent();
    ret->numReports = 1;
    ret->reports = reports;
    return ret;
  }
};

TEST(SimulatorVerifyTest, VerifyData_BleAllCasesTest) {
  std::stringstream buffer;
  std::cerr.rdbuf(buffer.rdbuf());

  BleDataClass data(0);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));
  std::string text = buffer.str();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 1);
  EXPECT_GT(
      CountOccurrences(
          text, kVerifyDataReceivedBleAdvertisementEventRequestAtTimeError),
      0);
  buffer.str("");

  data = BleDataClass(1);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));
  text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 0);
  buffer.str("");
}

class GnssDataClass : public DataFeedBase {
 public:
  explicit GnssDataClass(int capabilities_setting)
      : capabilities_setting_(capabilities_setting) {
    skip_initial_message_from_host_ = true;
  }
  int capabilities_setting_;

  uint32_t GetCapabilitiesBle() override { return CHRE_BLE_CAPABILITIES_NONE; }

  uint32_t GetFilterCapabilitiesBle() override {
    return CHRE_BLE_FILTER_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesGnss() override {
    if (capabilities_setting_ == 0)
      return CHRE_GNSS_CAPABILITIES_NONE;
    else if (capabilities_setting_ == 1)
      return CHRE_GNSS_CAPABILITIES_LOCATION;
    else if (capabilities_setting_ == 2)
      return CHRE_GNSS_CAPABILITIES_MEASUREMENTS;
    else
      return CHRE_GNSS_CAPABILITIES_LOCATION |
             CHRE_GNSS_CAPABILITIES_MEASUREMENTS;
  }

  SafeChreGnssLocationEvent* ReceivedGnssLocationEventRequestAtTime(
      uint64_t t_ns, uint32_t min_interval_ms,
      uint32_t /* min_time_to_next_fix_ms */) override {
    if (capabilities_setting_ < 4)
      return EmptyChreGnssLocationEvent(t_ns + min_interval_ms * kMillisToNano);
    else
      return nullptr;
  }

  uint32_t GetCapabilitiesWwan() override {
    return CHRE_WWAN_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }
};

TEST(SimulatorVerifyTest, VerifyData_GnssValidCasesTest) {
  GnssDataClass data(0);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));

  data = GnssDataClass(1);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));
}

TEST(SimulatorVerifyTest, VerifyData_GnssInvalidCasesTest) {
  std::stringstream buffer;
  std::cerr.rdbuf(buffer.rdbuf());

  GnssDataClass data(2);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));
  auto text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 1);
  EXPECT_GT(CountOccurrences(
                text, kVerifyDataReceivedGnssDataEventRequestAtTimeError),
            0);
  buffer.str("");

  data = GnssDataClass(3);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));
  text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 1);
  EXPECT_GT(CountOccurrences(
                text, kVerifyDataReceivedGnssDataEventRequestAtTimeError),
            0);
  buffer.str("");

  data = GnssDataClass(10);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));
  text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 2);
  EXPECT_GT(CountOccurrences(
                text, kVerifyDataReceivedGnssLocationEventRequestAtTimeError),
            0);
  EXPECT_GT(CountOccurrences(
                text, kVerifyDataReceivedGnssDataEventRequestAtTimeError),
            0);
  buffer.str("");
}

class WwanDataClass : public DataFeedBase {
 public:
  explicit WwanDataClass(int capabilities_setting)
      : capabilities_setting_(capabilities_setting) {
    skip_initial_message_from_host_ = true;
  }
  int capabilities_setting_;

  uint32_t GetCapabilitiesBle() override { return CHRE_BLE_CAPABILITIES_NONE; }

  uint32_t GetFilterCapabilitiesBle() override {
    return CHRE_BLE_FILTER_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWwan() override { return CHRE_WWAN_GET_CELL_INFO; }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }

  SafeChreWwanCellInfoResult* ReceivedWwanCallInfoResultRequestAtTime(
      uint64_t t) override {
    if (capabilities_setting_ == 1) {
      auto cells = new chreWwanCellInfo[2];
      cells[0].timeStamp = t;
      cells[1].timeStamp = t;

      auto ret = EmptyChreWwanCellInfoResult();
      ret->cellInfoCount = 2;
      ret->cells = cells;

      return ret;
    } else {
      return nullptr;
    }
  }
};

TEST(SimulatorVerifyTest, VerifyData_WwanAllCasesTest) {
  std::stringstream buffer;
  std::cerr.rdbuf(buffer.rdbuf());

  WwanDataClass data(1);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));
  auto text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 0);
  buffer.str("");

  data = WwanDataClass(2);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));
  text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 1);
  EXPECT_GT(CountOccurrences(
                text, kVerifyDataReceivedWwanCallInfoResultRequestAtTimeError),
            0);
  buffer.str("");
}

enum WifiCapabilities { kNone, kScanOnly, kRangingOnly, kScanAndRanging };

class WifiDataClass : public DataFeedBase {
 public:
  explicit WifiDataClass(WifiCapabilities capabilities_setting,
                         bool scan_function_defined,
                         bool ranging_function_defined)
      : capabilities_setting_(capabilities_setting),
        scan_function_defined_(scan_function_defined),
        ranging_function_defined_(ranging_function_defined) {
    skip_initial_message_from_host_ = true;
  }
  WifiCapabilities capabilities_setting_;
  bool scan_function_defined_;
  bool ranging_function_defined_;

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
    if (capabilities_setting_ == kNone)
      return CHRE_WIFI_CAPABILITIES_NONE;
    else if (capabilities_setting_ == kScanOnly)
      return CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN;
    else if (capabilities_setting_ == kRangingOnly)
      return CHRE_WIFI_CAPABILITIES_RTT_RANGING;
    else
      return CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN |
             CHRE_WIFI_CAPABILITIES_RTT_RANGING;
  }

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }

  SafeChreWifiScanEvent* ReceivedWifiScanEventRequestAtTime(
      uint64_t t, const SafeChreWifiScanParams& /* params */) override {
    if (scan_function_defined_) {
      return EmptyChreWifiScanEvent(t);
    } else {
      return nullptr;
    }
  }

  SafeChreWifiRangingEvent* ReceivedWifiRangingEventRequestAtTime(
      uint64_t /* t */,
      const SafeChreWifiRangingParams& /* params */) override {
    if (ranging_function_defined_) {
      return EmptyChreWifiRangingEvent();
    } else {
      return nullptr;
    }
  }
};

TEST(SimulatorVerifyTest, VerifyData_WifiFailingCasesTest) {
  std::string ranging_error =
      absl::StrCat(kVerifyDataReceivedWifiRangingEventRequestAtTime, "\n");

  std::stringstream buffer;
  std::cerr.rdbuf(buffer.rdbuf());

  // all capabilities are set, but none of the functions are defined
  auto data = WifiDataClass(kScanAndRanging, /*scan_function_defined=*/false,
                            /*ranging_function_defined=*/false);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));
  auto text = buffer.view();
  EXPECT_EQ(text, ranging_error);
  buffer.str("");

  // individual capabilities are set, but none of the functions are defined
  data = WifiDataClass(kScanOnly, /*scan_function_defined=*/false,
                       /*ranging_function_defined=*/false);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));
  text = buffer.view();
  EXPECT_EQ(text, "");
  buffer.str("");

  data = WifiDataClass(kRangingOnly, /*scan_function_defined=*/false,
                       /*ranging_function_defined=*/false);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));
  text = buffer.view();
  EXPECT_EQ(text, ranging_error);
  buffer.str("");

  // capability is set, but the other function is defined
  data = WifiDataClass(kScanOnly, /*scan_function_defined=*/false,
                       /*ranging_function_defined=*/true);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));

  data = WifiDataClass(kRangingOnly, /*scan_function_defined=*/true,
                       /*ranging_function_defined=*/false);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));

  // both capabilities are set, but only one function is defined
  data = WifiDataClass(kScanAndRanging, /*scan_function_defined=*/true,
                       /*ranging_function_defined=*/false);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));

  data = WifiDataClass(kScanAndRanging, /*scan_function_defined=*/false,
                       /*ranging_function_defined=*/true);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));
}

TEST(SimulatorVerifyTest, VerifyData_WifiNoCapabilitiesTest) {
  // everything should pass with no capabilities, regardless of which functions
  // are defined.

  WifiDataClass data(kNone, /*scan_function_defined=*/false,
                     /*ranging_function_defined=*/false);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));

  data = WifiDataClass(kNone, /*scan_function_defined=*/true,
                       /*ranging_function_defined=*/false);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));

  data = WifiDataClass(kNone, /*scan_function_defined=*/false,
                       /*ranging_function_defined=*/true);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));
}

TEST(SimulatorVerifyTest, VerifyData_WifiPassingCasesTest) {
  WifiDataClass data(kScanOnly, /*scan_function_defined=*/true,
                     /*ranging_function_defined=*/false);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));

  data = WifiDataClass(kScanOnly, /*scan_function_defined=*/true,
                       /*ranging_function_defined=*/true);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));

  data = WifiDataClass(kRangingOnly, /*scan_function_defined=*/false,
                       /*ranging_function_defined=*/true);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));

  data = WifiDataClass(kRangingOnly, /*scan_function_defined=*/true,
                       /*ranging_function_defined=*/true);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));

  data = WifiDataClass(kScanAndRanging, /*scan_function_defined=*/true,
                       /*ranging_function_defined=*/true);
}

class SensorDataClass : public DataFeedBase {
 public:
  explicit SensorDataClass(bool define_all_functions, int bias_status)
      : define_all_functions_(define_all_functions) {
    skip_initial_message_from_host_ = true;
    // if bias_status = 0, don't create a bias vector.
    // if bias is 1, create a wrong size bias vector.
    if (bias_status == 1) {
      sensor_bias_events_ =
          std::vector<std::map<uint64_t, SafeChreBiasEvent*>>(3);
    }
    // if bias is 2, create a correct sized one.
    if (bias_status == 2) {
      sensor_bias_events_ =
          std::vector<std::map<uint64_t, SafeChreBiasEvent*>>(1);
    }
  }

  bool define_all_functions_;

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

  uint32_t GetSensorCount() override { return 1; }

  uint32_t GetAudioSourceCount() override { return 0; }

  std::vector<chreSensorInfo> GetSensors() override {
    if (!define_all_functions_) return std::vector<chreSensorInfo>();
    std::vector<chreSensorInfo> ret = {chreSensorInfo{
        .sensorName = "sensor",
        .sensorType = CHRE_SENSOR_TYPE_ACCELEROMETER,
        .isOnChange = true,
        .isOneShot = false,
        .reportsBiasEvents = false,
        .minInterval = CHRE_SENSOR_INTERVAL_DEFAULT,
    }};

    return ret;
  }

  SafeChreSensorSamplingStatus* GetSamplingStatusUpdate(
      uint64_t /* t */, uint32_t /* sensor_info_index */,
      uint64_t requested_interval_ns, uint64_t requested_latency_ns) override {
    if (!define_all_functions_) return nullptr;
    auto ret = new SafeChreSensorSamplingStatus();
    ret->interval = requested_interval_ns;
    ret->latency = requested_latency_ns;
    ret->enabled = true;
    return ret;
  }

  SafeChreSensorData* ConfigureSensor(uint64_t t,
                                      uint32_t /* sensor_info_index */,
                                      bool /* is_oneshot */,
                                      uint64_t interval_ns,
                                      uint64_t /* latency_ns */) override {
    if (!define_all_functions_) return nullptr;
    auto ret = new SafeChreSensorData(kSensorThreeAxisData);
    ret->header.baseTimestamp = t;
    ret->header.readingCount = 1;
    ret->sample_data.push_back(chreSensorThreeAxisSampleData{
        .timestampDelta = static_cast<uint32_t>(interval_ns / 2),
        .values =
            {
                1,
                1,
                1,
            },
    });
    return ret;
  }
};

TEST(SimulatorVerifyTest, VerifyData_SensorVerifyTest) {
  std::stringstream buffer;
  std::cerr.rdbuf(buffer.rdbuf());

  SensorDataClass data(false, 2);
  EXPECT_FALSE(Simulator::VerifyValidData(&data));
  auto text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 3);
  EXPECT_GT(CountOccurrences(text, kVerifyDataReceivedSensorGetSensorsAtTime),
            0);
  EXPECT_GT(CountOccurrences(
                text, kVerifyDataReceivedSensorGetSamplingStatusUpdateAtTime),
            0);
  EXPECT_GT(
      CountOccurrences(text, kVerifyDataReceivedSensorConfigureSensorAtTime),
      0);
  buffer.str("");

  SensorDataClass data2(true, 2);
  EXPECT_TRUE(Simulator::VerifyValidData(&data2));
  text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 0);
  buffer.str("");
}

TEST(SimulatorVerifyTest, VerifyData_SensorBiasVerifyTest) {
  std::stringstream buffer;
  std::cerr.rdbuf(buffer.rdbuf());

  SensorDataClass data(true, 0);
  EXPECT_TRUE(Simulator::VerifyValidData(&data));
  auto text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 0);
  buffer.str("");

  SensorDataClass data2(true, 1);
  EXPECT_FALSE(Simulator::VerifyValidData(&data2));
  text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 1);
  EXPECT_GT(CountOccurrences(text, kVerifyBiasVectorInitializedCorrectly), 0);
  buffer.str("");

  SensorDataClass data3(true, 2);
  EXPECT_TRUE(Simulator::VerifyValidData(&data3));
  text = buffer.view();
  EXPECT_EQ(CountOccurrences(text, kVerifyDataInvalidData), 0);
  buffer.str("");
}

/************************* Simulator Core Tests *******************************/

struct VerificationData {
  int event_type;
  uint64_t time;
  uint64_t payload;
  SensorDataType sensor_type;
};

class SimulatorCoreTest : public ::testing::Test {
 public:
  Simulator* sim_;
  static std::vector<VerificationData>* data_;
  // pair of sensor index and flush id
  static std::vector<std::pair<uint32_t, uint32_t>>* flush_responses_;
  static uint64_t time_since_epoch_;
  static bool wifi_response_callback_called_;
  static bool wifi_scan_monitor_callback_called_;

 protected:
  SimulatorCoreTest() {
    Simulator::ResetInstance();
    sim_ = Simulator::GetInstance();
    data_ = new std::vector<VerificationData>();
    data_->clear();

    flush_responses_ = new std::vector<std::pair<uint32_t, uint32_t>>();
    flush_responses_->clear();

    time_since_epoch_ = sim_->time_since_epoch_;

    wifi_scan_monitor_callback_called_ = false;

    std::cerr.rdbuf(buffer_.rdbuf());
  }
  std::stringstream buffer_;

  ~SimulatorCoreTest() override {
    delete data_;
    delete flush_responses_;
    buffer_.str("");
  }
};

chrePalGnssCallbacks* GetGnssCallbacks() {
  auto callbacks = new chrePalGnssCallbacks;
  callbacks->locationEventCallback = [](struct chreGnssLocationEvent* event) {
    SimulatorCoreTest::data_->push_back(VerificationData{
        kGnssLocation,                                 // event_type
        event->timestamp,                              // time
        static_cast<uint64_t>(event->latitude_deg_e7)  // payload
    });
  };
  callbacks->measurementEventCallback = [](struct chreGnssDataEvent* event) {
    SimulatorCoreTest::data_->push_back(VerificationData{
        kGnssMeasurement,                             // data_type
        static_cast<uint64_t>(event->clock.time_ns),  // time
        static_cast<uint64_t>(event->measurements[0].received_sv_time_in_ns /
                              kMillisToNano)  // paylaod
    });
  };
  callbacks->locationStatusChangeCallback = [](bool, uint8_t) {};
  callbacks->measurementStatusChangeCallback = [](bool, uint8_t) {};

  return callbacks;
}

bool GnssVerify(VerificationData d, int e_t, uint64_t t_ms) {
  if (d.event_type != e_t || d.payload != t_ms) return false;
  if (e_t == kGnssLocation)
    return d.time == t_ms + SimulatorCoreTest::time_since_epoch_;
  else
    return d.time == t_ms * kMillisToNano;
}

class GnssData : public DataFeedBase {
 public:
  GnssData() { skip_initial_message_from_host_ = true; }

  uint32_t GetCapabilitiesBle() override { return CHRE_BLE_CAPABILITIES_NONE; }

  uint32_t GetFilterCapabilitiesBle() override {
    return CHRE_BLE_FILTER_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_LOCATION |
           CHRE_GNSS_CAPABILITIES_MEASUREMENTS |
           CHRE_GNSS_CAPABILITIES_GNSS_ENGINE_BASED_PASSIVE_LISTENER;
  }

  SafeChreGnssLocationEvent* ReceivedGnssLocationEventRequestAtTime(
      uint64_t t_ns, uint32_t min_interval_ms,
      uint32_t /*min_time_to_next_fix_ms*/) override {
    auto ptr =
        EmptyChreGnssLocationEvent(t_ns + min_interval_ms * kMillisToNano);
    ptr->latitude_deg_e7 = t_ns / kMillisToNano + min_interval_ms;
    return ptr;
  }

  SafeChreGnssDataEvent* ReceivedGnssDataEventRequestAtTime(
      uint64_t t_ns, uint32_t min_interval_ms) override {
    auto ptr = EmptyChreGnssDataEvent(t_ns + min_interval_ms * kMillisToNano);
    auto new_measures = new chreGnssMeasurement[1];
    new_measures[0].received_sv_time_in_ns =
        t_ns + min_interval_ms * kMillisToNano;
    ptr->measurements = new_measures;
    return ptr;
  }

  uint32_t GetCapabilitiesWwan() override {
    return CHRE_WWAN_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }
};

// static
std::vector<VerificationData>* SimulatorCoreTest::data_;
std::vector<std::pair<uint32_t, uint32_t>>* SimulatorCoreTest::flush_responses_;
uint64_t SimulatorCoreTest::time_since_epoch_;
bool SimulatorCoreTest::wifi_response_callback_called_ = false;
bool SimulatorCoreTest::wifi_scan_monitor_callback_called_ = false;

TEST_F(SimulatorCoreTest, SimulatorCore_InitialTimeIsCorrect) {
  auto current_time = chre::gChrePalSystemApi.getCurrentTime();
  EXPECT_EQ(current_time, 0);

  // now wait a second. The time shouldn't change.
  absl::SleepFor(absl::Seconds(1));
  current_time = chre::gChrePalSystemApi.getCurrentTime();
  EXPECT_EQ(current_time, 0);
}

TEST_F(SimulatorCoreTest, SimulatorCore_SystemTimerFunctionsWork) {
  auto sys_timer = new chre::SystemTimer;

  bool timer_triggered = false;
  chre::SystemTimerCallback* callback = [](void* data) {
    *static_cast<bool*>(data) = true;
  };

  MessageToHostDataClass data(1);
  ASSERT_TRUE(sim_->InitializeDataFeed(&data));

  EXPECT_FALSE(sys_timer->isActive());
  EXPECT_TRUE(sys_timer->init());
  EXPECT_FALSE(sys_timer->isActive());
  EXPECT_TRUE(sys_timer->set(callback, (void*)&timer_triggered,
                             chre::Nanoseconds(120)));
  EXPECT_TRUE(sys_timer->isActive());
  EXPECT_FALSE(timer_triggered);

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 120);
  EXPECT_TRUE(timer_triggered);
  EXPECT_FALSE(sys_timer->isActive());

  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  delete sys_timer;
}

TEST_F(SimulatorCoreTest, SimulatorCore_MultipleSystemTimers) {
  chre::SystemTimer s1, s2, s3, s4;
  s1.init();
  s2.init();
  s3.init();
  s4.init();

  bool triggered1 = false;
  bool triggered2 = false;
  bool triggered3 = false;
  chre::SystemTimerCallback* callback = [](void* data) {
    *static_cast<bool*>(data) = true;
  };

  EXPECT_TRUE(s1.set(callback, (void*)&triggered1, chre::Nanoseconds(200)));
  EXPECT_TRUE(s2.set(callback, (void*)&triggered2, chre::Nanoseconds(100)));

  EXPECT_TRUE(s1.isActive());
  EXPECT_TRUE(s2.isActive());
  EXPECT_FALSE(s3.isActive());

  MessageToHostDataClass data(1);
  ASSERT_TRUE(sim_->InitializeDataFeed(&data));

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 100);
  EXPECT_FALSE(triggered1);
  EXPECT_TRUE(triggered2);
  EXPECT_TRUE(s1.isActive());
  EXPECT_FALSE(s2.isActive());

  EXPECT_TRUE(s3.set(callback, (void*)&triggered3, chre::Nanoseconds(60)));
  EXPECT_TRUE(s4.set([](void*) {}, nullptr, chre::Nanoseconds(50)));

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 150);

  // overwrite s3
  EXPECT_TRUE(s3.set(callback, (void*)&triggered3, chre::Nanoseconds(50)));

  EXPECT_NE(sim_->current_time_, 200);
  EXPECT_FALSE(triggered1 || triggered3);

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 200);
  EXPECT_TRUE(triggered1 && triggered3);
  EXPECT_FALSE(sim_->dying_);

  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);
}

TEST_F(SimulatorCoreTest, SimulatorCore_GnssCallbacksWork) {
  auto callbacks = GetGnssCallbacks();
  GnssData gnss_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&gnss_data));

  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);

  gnss_api->controlLocationSession(true, 100, 100);
  sim_->AllEventsProcessed();

  ASSERT_EQ(data_->size(), 1);  // did the callback trigger?
  EXPECT_EQ((*data_)[0].event_type, kGnssLocation);
  EXPECT_EQ((*data_)[0].payload, 100);
  gnss_api->controlLocationSession(false, 100, 0);
  gnss_api->controlMeasurementSession(true, 1000);
  sim_->AllEventsProcessed();

  ASSERT_EQ(data_->size(), 2);  // did the callback trigger?
  EXPECT_EQ((*data_)[1].event_type,
            kGnssMeasurement);  // now it's a
                                // measurement data
  EXPECT_EQ((*data_)[1].payload, 1100);

  gnss_api->close();
  delete callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_TimeFreezesUntilAllEventsProcessed) {
  auto callbacks = GetGnssCallbacks();
  GnssData gnss_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&gnss_data));

  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);

  gnss_api->controlLocationSession(true, 100, 100);
  auto time = chre::gChrePalSystemApi.getCurrentTime();
  sim_->AllEventsProcessed();

  // we should now be at the next point in time
  EXPECT_NE(time, chre::gChrePalSystemApi.getCurrentTime());
  time = chre::gChrePalSystemApi.getCurrentTime();

  absl::SleepFor(absl::Seconds(1));  // confirm that time doesn't change
  EXPECT_EQ(time, chre::gChrePalSystemApi.getCurrentTime());
  sim_->AllEventsProcessed();  // now time can change again
  EXPECT_NE(time, chre::gChrePalSystemApi.getCurrentTime());

  gnss_api->close();
  delete callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_CorrectFlow) {
  auto callbacks = GetGnssCallbacks();
  GnssData gnss_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&gnss_data));

  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);
  auto control_loc = gnss_api->controlLocationSession;

  auto now = chre::gChrePalSystemApi.getCurrentTime() + sim_->time_since_epoch_;

  control_loc(true, 100, 100);
  sim_->AllEventsProcessed();

  ASSERT_EQ(data_->size(), 1);             // did the callback trigger?
  EXPECT_EQ((*data_)[0].time, now + 100);  // after minIntervalMs.
  EXPECT_EQ((*data_)[0].payload, 100);
  sim_->AllEventsProcessed();  // trigger that we've processed the message.
  ASSERT_EQ(data_->size(), 2);
  EXPECT_EQ((*data_)[1].time, now + 100 + 100);  // after minIntervalMs.
  control_loc(true, 200, 200);                   // change the parameters.
  sim_->AllEventsProcessed();  // trigger that we've processed the message.
  ASSERT_EQ(data_->size(), 3);
  EXPECT_EQ((*data_)[2].time,
            now + 200 + 200);  // make sure the old one was discontinued.
  sim_->AllEventsProcessed();  // trigger that we've processed the message.
  ASSERT_EQ(data_->size(), 4);
  EXPECT_EQ((*data_)[3].time,
            now + 400 + 200);  // make sure the old one was discontinued.
  EXPECT_EQ((*data_)[3].payload, 400 + 200);
  EXPECT_FALSE(sim_->dying_);
  control_loc(false, 200, 200);   // cancel the location request.
  sim_->AllEventsProcessed();     // trigger that we've processed the message.
  EXPECT_EQ(sim_->dying_, true);  // the simulator should now be dying.
  EXPECT_EQ(data_->size(), 4);    // size should not increase.

  gnss_api->close();
  delete callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_MultiGnssCorrectFlow) {
  auto callbacks = GetGnssCallbacks();
  GnssData gnss_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&gnss_data));

  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);
  auto control_loc = gnss_api->controlLocationSession;
  auto control_measure = gnss_api->controlMeasurementSession;

  control_loc(true, 150, 150);
  control_measure(true, 200);
  sim_->AllEventsProcessed();  // moves to t = 150
  sim_->AllEventsProcessed();  // 200
  sim_->AllEventsProcessed();  // 300
  EXPECT_EQ(sim_->current_time_, 300 * kMillisToNano);
  control_measure(true, 250);
  sim_->AllEventsProcessed();  // 450
  sim_->AllEventsProcessed();  // 550
  EXPECT_EQ(sim_->current_time_, 550 * kMillisToNano);
  control_loc(false, 0, 0);
  sim_->AllEventsProcessed();  // 800
  EXPECT_EQ(sim_->current_time_, 800 * kMillisToNano);
  control_measure(false, 0);
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto ds = (*data_);
  EXPECT_TRUE(sim_->dying_);
  ASSERT_GE(ds.size(), 6);
  EXPECT_TRUE(GnssVerify(ds[0], kGnssLocation, 150));
  EXPECT_TRUE(GnssVerify(ds[1], kGnssMeasurement, 200));
  EXPECT_TRUE(GnssVerify(ds[2], kGnssLocation, 300));
  EXPECT_TRUE(GnssVerify(ds[3], kGnssLocation, 450));
  EXPECT_TRUE(GnssVerify(ds[4], kGnssMeasurement, 550));
  EXPECT_TRUE(GnssVerify(ds[5], kGnssMeasurement, 800));

  delete callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_GnssPalRequestsMonitoringWorks) {
  auto callbacks = GetGnssCallbacks();
  GnssData gnss_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&gnss_data));
  sim_->SetNanoappLoadedForTest(true);

  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);

  gnss_api->controlLocationSession(true, 100, 100);
  sim_->AllEventsProcessed();
  gnss_api->controlMeasurementSession(true, 50);
  sim_->AllEventsProcessed();
  gnss_api->configurePassiveLocationListener(true);
  gnss_api->controlLocationSession(false, 100, 0);
  gnss_api->controlMeasurementSession(false, 50);
  sim_->AllEventsProcessed();
  sim_->AllEventsProcessed();

  auto pal_requests = verify::GetReceivedNanoappRequests();
  EXPECT_EQ(pal_requests.size(), 5);
  EXPECT_EQ(pal_requests[0].first, 0);
  EXPECT_EQ(pal_requests[0].second, kRequestTypeControlLocationSessionGnss);
  EXPECT_EQ(pal_requests[1].first, 100 * kMillisToNano);
  EXPECT_EQ(pal_requests[1].second, kRequestTypeControlMeasurementSessionGnss);
  EXPECT_EQ(pal_requests[2].first, 150 * kMillisToNano);
  EXPECT_EQ(pal_requests[2].second,
            kRequestTypeConfigurePassiveLocationListenerGnss);
  EXPECT_EQ(pal_requests[3].first, 150 * kMillisToNano);
  EXPECT_EQ(pal_requests[3].second, kRequestTypeControlLocationSessionGnss);
  EXPECT_EQ(pal_requests[4].first, 150 * kMillisToNano);
  EXPECT_EQ(pal_requests[4].second, kRequestTypeControlMeasurementSessionGnss);

  delete callbacks;
}

class QuickGnss : public DataFeedBase {
 public:
  QuickGnss() { skip_initial_message_from_host_ = true; }

  uint32_t GetCapabilitiesBle() override { return CHRE_BLE_CAPABILITIES_NONE; }

  uint32_t GetFilterCapabilitiesBle() override {
    return CHRE_BLE_FILTER_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_LOCATION &
           CHRE_GNSS_CAPABILITIES_MEASUREMENTS;
  }

  SafeChreGnssLocationEvent* ReceivedGnssLocationEventRequestAtTime(
      uint64_t t_ns, uint32_t min_interval_ms,
      uint32_t /*min_time_to_next_fix_ms*/) override {
    auto ptr =
        EmptyChreGnssLocationEvent(t_ns + min_interval_ms * kMillisToNano / 2);
    ptr->latitude_deg_e7 = t_ns / kMillisToNano + min_interval_ms / 2;
    return ptr;
  }

  SafeChreGnssDataEvent* ReceivedGnssDataEventRequestAtTime(
      uint64_t t_ns, uint32_t min_interval_ms) override {
    auto ptr =
        EmptyChreGnssDataEvent(t_ns + min_interval_ms * kMillisToNano / 3);
    auto new_measures = new chreGnssMeasurement[1];
    new_measures[0].received_sv_time_in_ns =
        t_ns + min_interval_ms * kMillisToNano / 3;
    ptr->measurements = new_measures;
    return ptr;
  }

  uint32_t GetCapabilitiesWwan() override {
    return CHRE_WWAN_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }
};

TEST_F(SimulatorCoreTest, SimulatorCore_GnssTimeManipFeaturesWork) {
  auto callbacks = GetGnssCallbacks();
  QuickGnss gnss_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&gnss_data));

  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);
  auto control_loc = gnss_api->controlLocationSession;
  auto control_measure = gnss_api->controlMeasurementSession;

  control_loc(true, 300, 0);
  control_measure(true, 300);
  sim_->AllEventsProcessed();  // movest to t = 100
  sim_->AllEventsProcessed();  // 150
  sim_->AllEventsProcessed();  // 200
  control_loc(false, 0, 0);
  control_measure(false, 0);
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto ds = (*data_);
  EXPECT_TRUE(sim_->dying_);
  ASSERT_GE(ds.size(), 3);
  EXPECT_TRUE(GnssVerify(ds[0], kGnssMeasurement, 100));
  EXPECT_TRUE(GnssVerify(ds[1], kGnssLocation, 150));
  EXPECT_TRUE(GnssVerify(ds[2], kGnssMeasurement, 200));

  delete callbacks;
}

chrePalBleCallbacks* GetBleCallbacks() {
  auto callbacks = new chrePalBleCallbacks;
  callbacks->advertisingEventCallback = [](chreBleAdvertisementEvent* event) {
    uint64_t max_time = 0;
    for (size_t i = 0; i < event->numReports; i++) {
      max_time = fmax(max_time, event->reports[i].timestamp);
    }
    SimulatorCoreTest::data_->push_back(
        VerificationData{kBle, max_time, 0 /* payload */});
  };
  callbacks->requestStateResync = []() {};
  callbacks->scanStatusChangeCallback = [](bool, uint8_t) {};
  callbacks->readRssiCallback = [](uint8_t /* errorCode */,
                                   uint16_t /* handle */, int8_t rssi) {
    SimulatorCoreTest::data_->push_back(
        VerificationData{kBleRssi, 0 /* no time present in callback */,
                         absl::bit_cast<uint64_t>(static_cast<int64_t>(rssi))});
  };
  return callbacks;
}

class BleData : public DataFeedBase {
 public:
  explicit BleData(int8_t rssi = -50) : rssi_(rssi) {
    skip_initial_message_from_host_ = true;
  }

  uint32_t GetCapabilitiesBle() override {
    return CHRE_BLE_CAPABILITIES_SCAN | CHRE_BLE_CAPABILITIES_READ_RSSI;
  }

  uint32_t GetFilterCapabilitiesBle() override {
    return CHRE_BLE_FILTER_CAPABILITIES_RSSI |
           CHRE_BLE_FILTER_CAPABILITIES_SERVICE_DATA;
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_NONE;
  }

  SafeChreBleAdvertisementEvent* ReceivedBleAdvertisementEventRequestAtTime(
      uint64_t t_ns, uint64_t /* latency */,
      const SafeChreBleScanFilter& /* filter */) override {
    auto reports = new chreBleAdvertisingReport[1];
    reports[0].timestamp = t_ns + kSecsToNano;

    SafeChreBleAdvertisementEvent* ret = EmptyChreBleAdvertisementEvent();
    ret->numReports = 1;
    ret->reports = reports;

    return ret;
  }

  std::optional<chreBleReadRssiEvent> ReceivedBleRssiRequestAtTime(
      uint64_t /* t_ns */, uint16_t /* connectionHandle */) override {
    return chreBleReadRssiEvent{.result = {.errorCode = CHRE_ERROR_NONE},
                                .rssi = rssi_};
  }

  uint32_t GetCapabilitiesWwan() override {
    return CHRE_WWAN_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }

 private:
  int8_t rssi_;
};

TEST_F(SimulatorCoreTest, SimulatorCore_BleCorrectFlow) {
  chrePalBleCallbacks* callbacks = GetBleCallbacks();
  BleData ble_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&ble_data));

  const chrePalBleApi* ble_api = chrePalBleGetApi(1);
  EXPECT_EQ(ble_api->open(nullptr, callbacks), true);

  chreBleGenericFilter generic_filter = {
      .type = CHRE_BLE_AD_TYPE_SERVICE_DATA_WITH_UUID_16_LE,
      .len = 2,
      .data = {0x2C, 0xFE},
      .dataMask = {0xFF, 0xFF}};
  chreBleBroadcasterAddressFilter broadcaster_address_filter = {
      .broadcasterAddress = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  chreBleScanFilterV1_9 filter = {
      .rssiThreshold = CHRE_BLE_RSSI_THRESHOLD_NONE,
      .genericFilterCount = 1,
      .genericFilters = &generic_filter,
      .broadcasterAddressFilterCount = 1,
      .broadcasterAddressFilters = &broadcaster_address_filter};

  EXPECT_TRUE(ble_api->startScan(CHRE_BLE_SCAN_MODE_BACKGROUND, 0, &filter));
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, kSecsToNano);

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, kSecsToNano * 2);

  ASSERT_EQ(data_->size(), 2);
  ASSERT_EQ((*data_)[0].event_type, kBle);
  ASSERT_EQ((*data_)[0].time, kSecsToNano);
  ASSERT_EQ((*data_)[1].event_type, kBle);
  ASSERT_EQ((*data_)[1].time, kSecsToNano * 2);

  ble_api->stopScan();
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->dying_, true);
  EXPECT_EQ(data_->size(), 2);

  ble_api->close();
  delete callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_BleReadRssiSuccess) {
  // constants
  uint16_t kConnectionHandle = -23;
  int8_t kRssi = -50;  // as defined in BleData

  // start simulation
  chrePalBleCallbacks* callbacks = GetBleCallbacks();
  BleData ble_data{kRssi};
  sim_->InitializeDataFeed(&ble_data);
  const chrePalBleApi* ble_api = chrePalBleGetApi(1);
  ble_api->open(nullptr, callbacks);

  // act
  auto ok = ble_api->readRssi(kConnectionHandle);

  // assert result was received
  EXPECT_TRUE(ok);
  EXPECT_EQ(data_->size(), 1);
  EXPECT_EQ((*data_)[0].event_type, kBleRssi);
  EXPECT_EQ(absl::bit_cast<int64_t>((*data_)[0].payload), kRssi);

  // no events should be sent, so we should immediately die
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->dying_, true);

  // cleanup
  ble_api->close();
  delete callbacks;
}

class WwanData : public DataFeedBase {
 public:
  WwanData() { skip_initial_message_from_host_ = true; }

  uint32_t GetCapabilitiesBle() override { return CHRE_BLE_CAPABILITIES_NONE; }

  uint32_t GetFilterCapabilitiesBle() override {
    return CHRE_BLE_FILTER_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWwan() override { return CHRE_WWAN_GET_CELL_INFO; }

  SafeChreWwanCellInfoResult* ReceivedWwanCallInfoResultRequestAtTime(
      uint64_t t) override {
    auto cells = new chreWwanCellInfo[1];
    cells[0].timeStamp = t;

    auto ret = EmptyChreWwanCellInfoResult();
    ret->cellInfoCount = 1;
    ret->cells = cells;

    return ret;
  }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }
};

chrePalWwanCallbacks* GetWwanCallbacks() {
  auto callbacks = new chrePalWwanCallbacks();
  callbacks->cellInfoResultCallback = [](struct chreWwanCellInfoResult* res) {
    auto actual_timestamp = res->cells[0].timeStamp;
    for (int i = 0; i < res->cellInfoCount; i++)
      actual_timestamp = fmax(actual_timestamp, res->cells[0].timeStamp);
    SimulatorCoreTest::data_->push_back(VerificationData{
        kWwanCellInfo,                                     // event_type
        actual_timestamp,                                  // time
        static_cast<uint64_t>(res->cells[0].cellInfoType)  // payload
    });
  };

  return callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_WwanCallbacksWork) {
  auto callbacks = GetWwanCallbacks();
  WwanData wwan_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&wwan_data));
  sim_->SetNanoappLoadedForTest(true);

  auto wwan_api = chrePalWwanGetApi(12);
  EXPECT_TRUE(wwan_api->open(nullptr, callbacks));

  EXPECT_EQ(wwan_api->getCapabilities(), CHRE_WWAN_GET_CELL_INFO);
  EXPECT_TRUE(wwan_api->requestCellInfo());
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 0);

  auto ds = (*data_);
  ASSERT_EQ(ds.size(), 1);
  EXPECT_EQ(ds[0].event_type, kWwanCellInfo);
  EXPECT_EQ(ds[0].time, 0);

  auto pal_requests = verify::GetReceivedNanoappRequests();
  EXPECT_EQ(pal_requests.size(), 1);
  EXPECT_EQ(pal_requests[0].first, 0);
  EXPECT_EQ(pal_requests[0].second, kRequestTypeRequestCellInfoWwan);

  delete callbacks;
}

class DelayedWwan : public WwanData {
  SafeChreWwanCellInfoResult* ReceivedWwanCallInfoResultRequestAtTime(
      uint64_t t) override {
    auto cells = new chreWwanCellInfo[3];
    cells[0].timeStamp = t + 0;
    cells[1].timeStamp = t + 5;
    cells[2].timeStamp = t + 10;

    auto ret = EmptyChreWwanCellInfoResult();
    ret->cellInfoCount = 3;
    ret->cells = cells;

    return ret;
  }
};

TEST_F(SimulatorCoreTest, SimulatorCore_WwanTimeManipWorks) {
  auto callbacks = GetWwanCallbacks();
  DelayedWwan wwan_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&wwan_data));

  auto wwan_api = chrePalWwanGetApi(12);
  EXPECT_TRUE(wwan_api->open(nullptr, callbacks));

  EXPECT_EQ(wwan_api->getCapabilities(), CHRE_WWAN_GET_CELL_INFO);
  EXPECT_TRUE(wwan_api->requestCellInfo());
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 10);

  delete callbacks;
}

class WifiDataFlow : public DataFeedBase {
 public:
  explicit WifiDataFlow(bool init = true) {
    if (init) {
      SafeChreMessageFromHostData msg;
      msg.appId = 0;
      msg.message = nullptr;
      msg.messageSize = 0;
      msg.messageType = 0;
      msg.hostEndpoint = 1234;
      messages_to_chre_[100] = msg;
    }
  }

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
    return CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN |
           CHRE_WIFI_CAPABILITIES_RTT_RANGING;
  }

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }

  SafeChreWifiScanEvent* ReceivedWifiScanEventRequestAtTime(
      uint64_t t, const SafeChreWifiScanParams& /* params */) override {
    return EmptyChreWifiScanEvent(t);
  }

  SafeChreWifiRangingEvent* ReceivedWifiRangingEventRequestAtTime(
      uint64_t t, const SafeChreWifiRangingParams& /* params */) override {
    auto ret = EmptyChreWifiRangingEvent();
    auto results = new chreWifiRangingResult[1];
    results[0].timestamp = t;
    ret->results = results;
    ret->resultCount = 1;
    return ret;
  }
};

chrePalWifiCallbacks* GetWifiCallbacks(
    bool enable_response_callback_check = true) {
  auto callbacks = new chrePalWifiCallbacks();
  callbacks->scanMonitorStatusChangeCallback = [](bool /* pending */,
                                                  uint8_t /* errorCode */) {
    SimulatorCoreTest::wifi_scan_monitor_callback_called_ = true;
  };
  callbacks->scanResponseCallback = [](bool /* pending */,
                                       uint8_t /* errorCode */) {
    SimulatorCoreTest::wifi_response_callback_called_ = true;
  };

  if (enable_response_callback_check) {
    callbacks->scanEventCallback = [](struct chreWifiScanEvent* event) {
      // only log this if we had a response callback first.
      if (SimulatorCoreTest::wifi_response_callback_called_) {
        SimulatorCoreTest::data_->push_back(VerificationData{
            .event_type = kWifiScan,
            .time = event->referenceTime,
            .payload = event->referenceTime,
        });
      }
      SimulatorCoreTest::wifi_response_callback_called_ = false;
    };
  } else {
    callbacks->scanEventCallback = [](struct chreWifiScanEvent* event) {
      // always log. Useful for passive testing.
      SimulatorCoreTest::data_->push_back(VerificationData{
          .event_type = kWifiScan,
          .time = event->referenceTime,
          .payload = event->referenceTime,
      });
    };
  }

  callbacks->rangingEventCallback = [](uint8_t /* errorCode */,
                                       struct chreWifiRangingEvent* event) {
    auto best_time = event->results[0].timestamp;
    for (int i = 0; i < event->resultCount; i++)
      best_time = fmax(best_time, event->results[i].timestamp);
    SimulatorCoreTest::data_->push_back(VerificationData{
        .event_type = kWifiRanging,
        .time = best_time,
        .payload = event->results[0].timestamp,
    });
  };

  return callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_WifiCallbacksWork) {
  auto callbacks = GetWifiCallbacks();
  WifiDataFlow wifi_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&wifi_data));

  auto wifi_api = chrePalWifiGetApi(12);
  EXPECT_TRUE(wifi_api->open(nullptr, callbacks));

  EXPECT_EQ(wifi_api->getCapabilities(),
            CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN |
                CHRE_WIFI_CAPABILITIES_RTT_RANGING);

  SafeChreWifiScanParams wifi_scan_params;
  wifi_scan_params.scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE;
  wifi_scan_params.maxScanAgeMs = 0;
  wifi_scan_params.frequencyListLen = 0;
  wifi_scan_params.frequencyList = nullptr;
  wifi_scan_params.ssidListLen = 0;
  wifi_scan_params.ssidList = nullptr;
  wifi_scan_params.radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT;
  EXPECT_TRUE(wifi_api->requestScan(wifi_scan_params.GetUnsafe()));
  sim_->AllEventsProcessed();

  sim_->current_time_ = 10;
  SafeChreWifiRangingParams wifi_ranging_params;
  wifi_ranging_params.targetListLen = 1;
  wifi_ranging_params.targetList = new chreWifiRangingTarget[1];
  EXPECT_TRUE(wifi_api->requestRanging(wifi_ranging_params.GetUnsafe()));
  sim_->AllEventsProcessed();
  sim_->AllEventsProcessed();  // once more for the passive data workaround.

  auto ds = (*data_);
  ASSERT_EQ(ds.size(), 2);
  ASSERT_EQ(ds[0].event_type, kWifiScan);
  ASSERT_EQ(ds[0].payload, 0);
  ASSERT_EQ(ds[1].event_type, kWifiRanging);
  ASSERT_EQ(ds[1].payload, 10);

  delete callbacks;
}

class DelayedWifi : public WifiDataFlow {
  SafeChreWifiScanEvent* ReceivedWifiScanEventRequestAtTime(
      uint64_t t, const SafeChreWifiScanParams& /* params */) override {
    return EmptyChreWifiScanEvent(t + 20);
  }

  SafeChreWifiRangingEvent* ReceivedWifiRangingEventRequestAtTime(
      uint64_t t, const SafeChreWifiRangingParams& /* params */) override {
    auto ret = EmptyChreWifiRangingEvent();
    auto results = new chreWifiRangingResult[3];
    results[0].timestamp = t;
    results[1].timestamp = t + 10;
    results[2].timestamp = t + 5;
    ret->results = results;
    ret->resultCount = 3;
    return ret;
  }
};

TEST_F(SimulatorCoreTest, SimulatorCore_WifiTimeManipWorks) {
  auto callbacks = GetWifiCallbacks();
  DelayedWifi wifi_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&wifi_data));

  auto wifi_api = chrePalWifiGetApi(12);
  EXPECT_TRUE(wifi_api->open(nullptr, callbacks));

  SafeChreWifiScanParams wifi_scan_params;
  wifi_scan_params.scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE;
  wifi_scan_params.maxScanAgeMs = 0;
  wifi_scan_params.frequencyListLen = 0;
  wifi_scan_params.frequencyList = nullptr;
  wifi_scan_params.ssidListLen = 0;
  wifi_scan_params.ssidList = nullptr;
  wifi_scan_params.radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT;
  EXPECT_TRUE(wifi_api->requestScan(wifi_scan_params.GetUnsafe()));
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 20);

  SafeChreWifiRangingParams wifi_ranging_params;
  wifi_ranging_params.targetListLen = 1;
  wifi_ranging_params.targetList = new chreWifiRangingTarget[1];
  EXPECT_TRUE(wifi_api->requestRanging(wifi_ranging_params.GetUnsafe()));
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 20 + 10);
  sim_->AllEventsProcessed();  // once more for the passive data workaround.

  auto ds = (*data_);
  ASSERT_EQ(ds.size(), 2);
  ASSERT_EQ(ds[0].event_type, kWifiScan);
  ASSERT_EQ(ds[0].time, 20);
  ASSERT_EQ(ds[1].event_type, kWifiRanging);
  ASSERT_EQ(ds[1].time, 30);

  delete callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_WifiPalRequestsMonitoringWorks) {
  auto callbacks = GetWifiCallbacks();
  DelayedWifi wifi_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&wifi_data));
  sim_->SetNanoappLoadedForTest(true);

  auto wifi_api = chrePalWifiGetApi(12);
  EXPECT_TRUE(wifi_api->open(nullptr, callbacks));

  SafeChreWifiScanParams wifi_scan_params;
  wifi_scan_params.scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE;
  wifi_scan_params.maxScanAgeMs = 0;
  wifi_scan_params.frequencyListLen = 0;
  wifi_scan_params.frequencyList = nullptr;
  wifi_scan_params.ssidListLen = 0;
  wifi_scan_params.ssidList = nullptr;
  wifi_scan_params.radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT;
  EXPECT_TRUE(wifi_api->requestScan(wifi_scan_params.GetUnsafe()));
  sim_->AllEventsProcessed();

  SafeChreWifiRangingParams wifi_ranging_params;
  wifi_ranging_params.targetListLen = 1;
  wifi_ranging_params.targetList = new chreWifiRangingTarget[1];
  EXPECT_TRUE(wifi_api->requestRanging(wifi_ranging_params.GetUnsafe()));
  EXPECT_TRUE(wifi_api->configureScanMonitor(true));
  EXPECT_TRUE(SimulatorCoreTest::wifi_scan_monitor_callback_called_);
  sim_->AllEventsProcessed();

  auto pal_requests = verify::GetReceivedNanoappRequests();

  EXPECT_EQ(pal_requests.size(), 3);
  EXPECT_EQ(pal_requests[0].first, 0);
  EXPECT_EQ(pal_requests[0].second, kRequestTypeRequestScanWifi);
  EXPECT_EQ(pal_requests[1].first, 20);
  EXPECT_EQ(pal_requests[1].second, kRequestTypeRequestRangingWifi);
  EXPECT_EQ(pal_requests[2].first, 20);
  EXPECT_EQ(pal_requests[2].second, kRequestTypeConfigureScanMonitorWifi);

  delete callbacks;
}

class WifiDataWithPassive : public WifiDataFlow {
 public:
  WifiDataWithPassive() : WifiDataFlow(false) {
    skip_initial_message_from_host_ = true;
    AddScanEventAtTime(100);
    AddScanEventAtTime(300);
    AddScanEventAtTime(500);
    AddScanEventAtTime(700);
  }

  void AddScanEventAtTime(uint64_t t) {
    wifi_scan_events_[t] = EmptyChreWifiScanEvent(t);
  }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN |
           CHRE_WIFI_CAPABILITIES_SCAN_MONITORING;
  }
};

TEST_F(SimulatorCoreTest, SimulatorCore_WifiWithPassiveWorks) {
  // Request oneshot with passive off. Should not return passive.
  // Now turn on passive. Should get passive. Request new one-shot. After it
  // finishes, we should still return a passive. Disable passive listening. We
  // should not get the last one.

  chre::SystemTimer timer;  // used to stop at particular times.
  timer.init();

  auto callbacks = GetWifiCallbacks(/*enable_response_callback_check=*/false);
  WifiDataWithPassive wifi_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&wifi_data));

  auto wifi_api = chrePalWifiGetApi(12);
  EXPECT_TRUE(wifi_api->open(nullptr, callbacks));

  SafeChreWifiScanParams wifi_scan_params;
  wifi_scan_params.scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE;
  wifi_scan_params.maxScanAgeMs = 0;
  wifi_scan_params.frequencyListLen = 0;
  wifi_scan_params.frequencyList = nullptr;
  wifi_scan_params.ssidListLen = 0;
  wifi_scan_params.ssidList = nullptr;
  wifi_scan_params.radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT;

  EXPECT_TRUE(timer.set([](void*) {}, nullptr, chre::Nanoseconds(200)));

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 200);  // skipped passive at 100.
  EXPECT_TRUE(wifi_api->configureScanMonitor(true));
  EXPECT_TRUE(SimulatorCoreTest::wifi_scan_monitor_callback_called_);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300);  // returned passive at 300
  EXPECT_TRUE(wifi_api->requestScan(wifi_scan_params.GetUnsafe()));
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300);  // returned active at 300
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 500);  // returned passive at 500

  EXPECT_TRUE(wifi_api->configureScanMonitor(false));
  EXPECT_TRUE(timer.set([](void*) {}, nullptr,
                        chre::Nanoseconds(300)));  // timer at 800
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 800);  // skipped passive ative at 700
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto ds = (*data_);
  ASSERT_EQ(ds.size(), 3);
  ASSERT_EQ(ds[0].payload, 300);
  ASSERT_EQ(ds[1].payload, 300);
  ASSERT_EQ(ds[2].payload, 500);

  delete callbacks;
}

class PassiveData : public DataFeedBase {
 public:
  explicit PassiveData(bool init = true) {
    if (init) {
      AddMessageAtT(100);
      AddMessageAtT(200);
      AddMessageAtT(300);
    }
  }

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

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }

  void AddMessageAtT(uint64_t t) {
    SafeChreMessageFromHostData msg;
    msg.appId = 0;
    msg.message = nullptr;
    msg.messageSize = 0;
    msg.hostEndpoint = 1234;
    msg.messageType = t;
    messages_to_chre_[t] = msg;
  }
};

TEST_F(SimulatorCoreTest, SimulatorCore_PassiveDataWorks) {
  PassiveData p_data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&p_data));

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 100);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 200);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300);
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);
}

class MixedPassiveData : public PassiveData {
 public:
  explicit MixedPassiveData(bool init = true) : PassiveData(false) {
    if (init) {
      AddMessageAtT(100 * kMillisToNano);
      AddGnssAtT(200 * kMillisToNano);
      AddMessageAtT(300 * kMillisToNano);
      AddGnssAtT(300 * kMillisToNano);
    }
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_GNSS_ENGINE_BASED_PASSIVE_LISTENER;
  }

  void AddGnssAtT(uint64_t t_ns) {
    auto gnss = new SafeChreGnssLocationEvent();
    gnss->latitude_deg_e7 = t_ns / kMillisToNano;
    gnss->longitude_deg_e7 = t_ns / kMillisToNano;
    gnss->timestamp = t_ns;
    gnss_location_events_[t_ns] = gnss;
  }
};

TEST_F(SimulatorCoreTest, SimulatorCore_MixedPassiveDataTest) {
  MixedPassiveData p_data;
  auto callbacks = GetGnssCallbacks();
  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);

  ASSERT_TRUE(sim_->InitializeDataFeed(&p_data));
  EXPECT_TRUE(gnss_api->configurePassiveLocationListener(true));

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 100 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 200 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300 * kMillisToNano);
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto ds = (*data_);
  ASSERT_GE(ds.size(), 2);
  EXPECT_EQ(ds[0].event_type, kGnssLocation);
  EXPECT_EQ(ds[0].time, 200 + sim_->time_since_epoch_);
  EXPECT_EQ(ds[0].payload, 200);
  EXPECT_EQ(ds[1].event_type, kGnssLocation);
  EXPECT_EQ(ds[1].time, 300 + sim_->time_since_epoch_);
  EXPECT_EQ(ds[1].payload, 300);

  delete callbacks;
}

class GnssPassiveDataActivation : public MixedPassiveData {
 public:
  GnssPassiveDataActivation() : MixedPassiveData(false) {
    AddGnssAtT(100 * kMillisToNano);
    AddMessageAtT(150 * kMillisToNano);
    AddGnssAtT(200 * kMillisToNano);
    AddGnssAtT(300 * kMillisToNano);
    AddGnssAtT(400 * kMillisToNano);
    AddGnssAtT(500 * kMillisToNano);
  }
};

TEST_F(SimulatorCoreTest, SimulatorCore_GnssPassiveDataActivationTest) {
  GnssPassiveDataActivation p_data;
  auto callbacks = GetGnssCallbacks();
  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);

  ASSERT_TRUE(sim_->InitializeDataFeed(&p_data));

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 150 * kMillisToNano);
  EXPECT_TRUE(gnss_api->configurePassiveLocationListener(true));
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 200 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300 * kMillisToNano);
  EXPECT_TRUE(gnss_api->configurePassiveLocationListener(false));
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto ds = (*data_);
  ASSERT_EQ(ds.size(), 2);
  EXPECT_EQ(ds[0].event_type, kGnssLocation);
  EXPECT_EQ(ds[0].payload, 200);
  EXPECT_EQ(ds[1].event_type, kGnssLocation);
  EXPECT_EQ(ds[1].payload, 300);

  delete callbacks;
}

class GnssMixedSources : public MixedPassiveData {
 public:
  GnssMixedSources() : MixedPassiveData(false) {
    AddGnssAtT(150 * kMillisToNano);
    AddGnssAtT(250 * kMillisToNano);
    AddGnssAtT(350 * kMillisToNano);
    AddGnssAtT(450 * kMillisToNano);
    AddMessageAtT(500 * kMillisToNano);
    AddGnssAtT(550 * kMillisToNano);
    AddGnssAtT(650 * kMillisToNano);
    AddGnssAtT(750 * kMillisToNano);
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_LOCATION |
           CHRE_GNSS_CAPABILITIES_GNSS_ENGINE_BASED_PASSIVE_LISTENER;
  }

  SafeChreGnssLocationEvent* ReceivedGnssLocationEventRequestAtTime(
      uint64_t t_ns, uint32_t min_interval_ms,
      uint32_t /* min_time_to_next_fix_ms */) override {
    auto ptr =
        EmptyChreGnssLocationEvent(t_ns + min_interval_ms * kMillisToNano);
    ptr->latitude_deg_e7 = t_ns / kMillisToNano + min_interval_ms;
    return ptr;
  }
};

TEST_F(SimulatorCoreTest, SimulatorCore_GnssMixedSourcesTest) {
  GnssMixedSources p_data;
  auto callbacks = GetGnssCallbacks();
  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);

  ASSERT_TRUE(sim_->InitializeDataFeed(&p_data));
  EXPECT_TRUE(gnss_api->controlLocationSession(true, 100, 100));

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 100 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 150 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 200 * kMillisToNano);
  EXPECT_TRUE(gnss_api->configurePassiveLocationListener(true));
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 250 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300 * kMillisToNano);
  EXPECT_TRUE(gnss_api->controlLocationSession(false, 100, 0));
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 350 * kMillisToNano);
  EXPECT_TRUE(gnss_api->configurePassiveLocationListener(false));
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 500 * kMillisToNano);
  EXPECT_TRUE(gnss_api->controlLocationSession(true, 100, 0));
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 550 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 600 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 650 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 700 * kMillisToNano);
  EXPECT_TRUE(gnss_api->controlLocationSession(false, 100, 0));
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto ds = (*data_);
  ASSERT_EQ(ds.size(), 10);

  delete callbacks;
}

class GnssMeasurementsPassiveData : public PassiveData {
 public:
  explicit GnssMeasurementsPassiveData(bool init = true) : PassiveData(false) {
    if (init) {
      AddDataEventAtT(150 * kMillisToNano);
      AddDataEventAtT(250 * kMillisToNano);
      AddMessageAtT(300 * kMillisToNano);
      AddDataEventAtT(350 * kMillisToNano);
      AddDataEventAtT(450 * kMillisToNano);
      AddDataEventAtT(550 * kMillisToNano);
    }
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_MEASUREMENTS;
  }

  SafeChreGnssDataEvent* ReceivedGnssDataEventRequestAtTime(
      uint64_t t_ns, uint32_t min_interval_ms) override {
    auto ptr = EmptyChreGnssDataEvent(t_ns + min_interval_ms * kMillisToNano);
    auto new_measures = new chreGnssMeasurement[1];
    new_measures[0].received_sv_time_in_ns =
        t_ns + min_interval_ms * kMillisToNano;
    ptr->measurements = new_measures;
    return ptr;
  }

  void AddDataEventAtT(uint64_t t_ns) {
    auto ptr = EmptyChreGnssDataEvent(t_ns);
    auto new_measures = new chreGnssMeasurement[1];
    new_measures[0].received_sv_time_in_ns = t_ns;
    ptr->measurements = new_measures;
    gnss_data_events_[t_ns] = ptr;
  }
};

TEST_F(SimulatorCoreTest, GnssMeasurementsPassiveDataWorks) {
  GnssMeasurementsPassiveData p_data;
  auto callbacks = GetGnssCallbacks();
  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);

  ASSERT_TRUE(sim_->InitializeDataFeed(&p_data));

  sim_->AllEventsProcessed();
  // nothing should happen until the host message
  EXPECT_EQ(sim_->current_time_, 300 * kMillisToNano);
  gnss_api->controlMeasurementSession(true, 100);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 350 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 400 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 450 * kMillisToNano);
  gnss_api->controlMeasurementSession(false, 0);
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto ds = (*data_);
  ASSERT_GE(ds.size(), 3);
  EXPECT_EQ(ds[0].event_type, kGnssMeasurement);
  EXPECT_EQ(ds[0].time, 350 * kMillisToNano);
  EXPECT_EQ(ds[0].payload, 350);
  EXPECT_EQ(ds[1].event_type, kGnssMeasurement);
  EXPECT_EQ(ds[1].time, 400 * kMillisToNano);
  EXPECT_EQ(ds[2].event_type, kGnssMeasurement);
  EXPECT_EQ(ds[2].time, 450 * kMillisToNano);

  delete callbacks;
}

class MixedData : public DataFeedBase {
 public:
  MixedData() {
    AddMessageAtT(100 * kMillisToNano);
    AddMessageAtT(200 * kMillisToNano);
    AddMessageAtT(300 * kMillisToNano);
    AddMessageAtT(1000 * kMillisToNano);
  }

  uint32_t GetCapabilitiesBle() override { return CHRE_BLE_CAPABILITIES_NONE; }

  uint32_t GetFilterCapabilitiesBle() override {
    return CHRE_BLE_FILTER_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesGnss() override {
    return CHRE_GNSS_CAPABILITIES_LOCATION;
  }

  SafeChreGnssLocationEvent* ReceivedGnssLocationEventRequestAtTime(
      uint64_t t_ns, uint32_t min_interval_ms,
      uint32_t /* min_time_to_next_fix_ms */) override {
    auto ptr =
        EmptyChreGnssLocationEvent(t_ns + min_interval_ms * kMillisToNano);
    ptr->latitude_deg_e7 = t_ns / kMillisToNano + min_interval_ms;
    return ptr;
  }

  uint32_t GetCapabilitiesWwan() override {
    return CHRE_WWAN_CAPABILITIES_NONE;
  }

  uint32_t GetCapabilitiesWifi() override {
    return CHRE_WIFI_CAPABILITIES_NONE;
  }

  uint32_t GetSensorCount() override { return 0; }

  uint32_t GetAudioSourceCount() override { return 0; }

  void AddMessageAtT(uint64_t t) {
    SafeChreMessageFromHostData msg;
    msg.message = nullptr;
    msg.messageSize = 0;
    msg.messageType = t;
    msg.appId = 0;
    msg.hostEndpoint = 1234;
    messages_to_chre_[t] = msg;
  }
};

TEST_F(SimulatorCoreTest, SimulatorCore_MixedDataWorks) {
  auto callbacks = GetGnssCallbacks();
  MixedData data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&data));

  auto gnss_api = chrePalGnssGetApi(12);
  EXPECT_EQ(gnss_api->open(nullptr, callbacks), true);
  auto control_loc = gnss_api->controlLocationSession;

  control_loc(true, 150, 150);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 100 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 150 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 200 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300 * kMillisToNano);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 450 * kMillisToNano);
  control_loc(false, 0, 0);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 1000 * kMillisToNano);
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  delete callbacks;
}

class SimpleSensorData : public DataFeedBase {
 public:
  SimpleSensorData() {
    skip_initial_message_from_host_ = true;
    types_ = {
        CHRE_SENSOR_TYPE_ACCELEROMETER,          // ThreeAxisData
        CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT,  // OccurrenceData
        CHRE_SENSOR_TYPE_LIGHT,                  // FloatData
        CHRE_SENSOR_TYPE_PROXIMITY,              // ByteData
    };
  }

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

  uint32_t GetAudioSourceCount() override { return 0; }

  uint32_t GetSensorCount() override { return 4; }

  std::vector<uint8_t> types_;

  std::vector<chreSensorInfo> GetSensors() override {
    std::vector<chreSensorInfo> ret(4);

    for (int i = 0; i < 4; i++) {
      ret[i] = chreSensorInfo{
          .sensorName = std::to_string(i).c_str(),
          .sensorType = types_[i],
          .isOnChange = true,
          .isOneShot = static_cast<uint8_t>(i % 2),
          .reportsBiasEvents = false,
          .minInterval = CHRE_SENSOR_INTERVAL_DEFAULT,
      };
    }

    return ret;
  }

  SafeChreSensorSamplingStatus* GetSamplingStatusUpdate(
      uint64_t /* t */, uint32_t /* sensor_info_index */,
      uint64_t requested_interval_ns, uint64_t requested_latency_ns) override {
    auto ret = new SafeChreSensorSamplingStatus();
    ret->interval = requested_interval_ns;
    ret->latency = requested_latency_ns;
    ret->enabled = true;
    return ret;
  }

  SafeChreSensorData* ConfigureSensor(uint64_t t, uint32_t sensor_info_index,
                                      bool is_oneshot, uint64_t interval_ns,
                                      uint64_t /* latency_ns */) override {
    std::vector<SensorDataType> converted_types = {
        kSensorThreeAxisData, kSensorOccurrenceData, kSensorFloatData,
        kSensorByteData};
    auto ret = new SafeChreSensorData(converted_types[sensor_info_index]);
    ret->header.baseTimestamp = t;
    ret->header.readingCount = is_oneshot ? 1 : 3;
    if (sensor_info_index == 0) {
      for (int i = 0; i < ret->header.readingCount; i++) {
        ret->sample_data.push_back(chreSensorThreeAxisSampleData{
            .timestampDelta = static_cast<uint32_t>(interval_ns / 2),
            .values =
                {
                    1,
                    1,
                    1,
                },
        });
      }
    } else if (sensor_info_index == 1) {
      for (int i = 0; i < ret->header.readingCount; i++) {
        ret->sample_data.push_back(chreSensorOccurrenceSampleData{
            .timestampDelta = static_cast<uint32_t>(interval_ns / 2),
        });
      }
    } else if (sensor_info_index == 2) {
      for (int i = 0; i < ret->header.readingCount; i++) {
        ret->sample_data.push_back(chreSensorFloatSampleData{
            .timestampDelta = static_cast<uint32_t>(interval_ns / 2),
            .value = 1000,
        });
      }
    } else if (sensor_info_index == 3) {
      for (int i = 0; i < ret->header.readingCount; i++) {
        ret->sample_data.push_back(chreSensorByteSampleData{
            .timestampDelta = static_cast<uint32_t>(interval_ns / 2),
            .value = 3,
        });
      }
    }
    return ret;
  }
};

chrePalSensorCallbacks* GetSensorCallbacks() {
  auto callbacks = new chrePalSensorCallbacks;
  callbacks->dataEventCallback = [](uint32_t sensorInfoIndex, void* data) {
    if (sensorInfoIndex == 0) {
      auto new_data = static_cast<chreSensorThreeAxisData*>(data);
      auto final_timestamp = new_data->header.baseTimestamp;
      for (int i = 0; i < new_data->header.readingCount; i++)
        final_timestamp += new_data->readings->timestampDelta;
      SimulatorCoreTest::data_->push_back(VerificationData{
          .event_type = kSensor,
          .time = final_timestamp,
          .payload = static_cast<uint64_t>(new_data->header.readingCount),
          .sensor_type = kSensorThreeAxisData,
      });
    } else if (sensorInfoIndex == 1) {
      auto new_data = static_cast<chreSensorOccurrenceData*>(data);
      auto final_timestamp = new_data->header.baseTimestamp;
      for (int i = 0; i < new_data->header.readingCount; i++)
        final_timestamp += new_data->readings->timestampDelta;
      SimulatorCoreTest::data_->push_back(VerificationData{
          .event_type = kSensor,
          .time = final_timestamp,
          .payload = static_cast<uint64_t>(new_data->header.readingCount),
          .sensor_type = kSensorOccurrenceData,
      });
    } else if (sensorInfoIndex == 2) {
      auto new_data = static_cast<chreSensorFloatData*>(data);
      auto final_timestamp = new_data->header.baseTimestamp;
      for (int i = 0; i < new_data->header.readingCount; i++)
        final_timestamp += new_data->readings->timestampDelta;
      SimulatorCoreTest::data_->push_back(VerificationData{
          .event_type = kSensor,
          .time = final_timestamp,
          .payload = static_cast<uint64_t>(new_data->header.readingCount),
          .sensor_type = kSensorFloatData,
      });
    } else if (sensorInfoIndex == 3) {
      auto new_data = static_cast<chreSensorByteData*>(data);
      auto final_timestamp = new_data->header.baseTimestamp;
      for (int i = 0; i < new_data->header.readingCount; i++)
        final_timestamp += new_data->readings->timestampDelta;
      SimulatorCoreTest::data_->push_back(VerificationData{
          .event_type = kSensor,
          .time = final_timestamp,
          .payload = static_cast<uint64_t>(new_data->header.readingCount),
          .sensor_type = kSensorByteData,
      });
    }
  };

  callbacks->biasEventCallback = [](uint32_t sensorInfoIndex, void* data) {
    if (sensorInfoIndex == 0) {
      auto new_data = static_cast<chreSensorThreeAxisData*>(data);
      SimulatorCoreTest::data_->push_back(VerificationData{
          .event_type = kBiasEvent,
          .time = new_data->header.baseTimestamp,
          .payload = 0,
          .sensor_type = kSensorThreeAxisData,
      });
    } else if (sensorInfoIndex == 1) {
      auto new_data = static_cast<chreSensorOccurrenceData*>(data);
      SimulatorCoreTest::data_->push_back(VerificationData{
          .event_type = kBiasEvent,
          .time = new_data->header.baseTimestamp,
          .payload = 0,
          .sensor_type = kSensorOccurrenceData,
      });
    } else if (sensorInfoIndex == 2) {
      auto new_data = static_cast<chreSensorFloatData*>(data);
      SimulatorCoreTest::data_->push_back(VerificationData{
          .event_type = kBiasEvent,
          .time = new_data->header.baseTimestamp,
          .payload = 0,
          .sensor_type = kSensorFloatData,
      });
    } else if (sensorInfoIndex == 3) {
      auto new_data = static_cast<chreSensorByteData*>(data);
      SimulatorCoreTest::data_->push_back(VerificationData{
          .event_type = kBiasEvent,
          .time = new_data->header.baseTimestamp,
          .payload = 0,
          .sensor_type = kSensorByteData,
      });
    }
  };

  callbacks->samplingStatusUpdateCallback =
      [](uint32_t /* sensorInfoIndex */,
         struct chreSensorSamplingStatus* /* status */) {};

  callbacks->flushCompleteCallback = [](uint32_t sensorInfoIndex,
                                        uint32_t flushRequestId,
                                        uint8_t /* errorCode */) {
    SimulatorCoreTest::flush_responses_->push_back(
        std::make_pair(sensorInfoIndex, flushRequestId));
  };

  return callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_SimpleSensorWorks) {
  auto callbacks = GetSensorCallbacks();
  SimpleSensorData data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&data));

  auto sensor_api = chrePalSensorGetApi(12);
  EXPECT_EQ(sensor_api->open(nullptr, callbacks), true);

  const chreSensorInfo* sensors = nullptr;
  uint32_t count;
  EXPECT_TRUE(sensor_api->getSensors(&sensors, &count));
  ASSERT_EQ(count, 4);
  ASSERT_NE(sensors, nullptr);
  for (int i = 0; i < count; i++) {
    EXPECT_EQ(sensors[i].isOneShot, i % 2);
    EXPECT_EQ(sensors[i].sensorType, data.types_[i]);
  }

  sensor_api->configureSensor(0, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 100,
                              1000);
  sensor_api->configureSensor(1, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 200,
                              1000);
  sensor_api->configureSensor(2, CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT, 800,
                              1000);
  sensor_api->configureSensor(3, CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT, 900,
                              1000);

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 150);
  sensor_api->configureSensor(0, CHRE_SENSOR_CONFIGURE_MODE_DONE, 100, 1000);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300);
  sensor_api->configureSensor(1, CHRE_SENSOR_CONFIGURE_MODE_DONE, 100, 1000);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 400);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 450);
  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto ds = (*data_);
  ASSERT_GE(ds.size(), 4);
  for (int i = 0; i < 4; i++) EXPECT_EQ(ds[i].event_type, kSensor);

  EXPECT_EQ(ds[0].sensor_type, kSensorThreeAxisData);
  EXPECT_EQ(ds[0].time, 150);
  EXPECT_EQ(ds[0].payload, 3);

  EXPECT_EQ(ds[1].sensor_type, kSensorOccurrenceData);
  EXPECT_EQ(ds[1].time, 300);
  EXPECT_EQ(ds[1].payload, 3);

  EXPECT_EQ(ds[2].sensor_type, kSensorFloatData);
  EXPECT_EQ(ds[2].time, 400);
  EXPECT_EQ(ds[2].payload, 1);

  EXPECT_EQ(ds[3].sensor_type, kSensorByteData);
  EXPECT_EQ(ds[3].time, 450);
  EXPECT_EQ(ds[3].payload, 1);

  delete callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_SensorFlowWorks) {
  auto callbacks = GetSensorCallbacks();
  SimpleSensorData data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&data));

  auto sensor_api = chrePalSensorGetApi(12);
  EXPECT_EQ(sensor_api->open(nullptr, callbacks), true);

  const chreSensorInfo* sensors = nullptr;
  uint32_t count;
  EXPECT_TRUE(sensor_api->getSensors(&sensors, &count));

  sensor_api->configureSensor(0, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 100,
                              1000);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 150);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 300);
  sensor_api->configureSensor(2, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 110,
                              1000);
  sensor_api->configureSensor(1, CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT, 200,
                              1000);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 400);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 450);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 465);
  sensor_api->configureSensor(1, CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT, 200,
                              1000);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 565);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 600);
  sensor_api->configureSensor(0, CHRE_SENSOR_CONFIGURE_MODE_DONE, 100, 1000);
  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 465 + 165);
  sensor_api->configureSensor(2, CHRE_SENSOR_CONFIGURE_MODE_DONE, 100, 1000);

  auto ds = (*data_);
  ASSERT_GE(ds.size(), 8);

  EXPECT_EQ(ds[0].sensor_type, kSensorThreeAxisData);
  EXPECT_EQ(ds[1].sensor_type, kSensorThreeAxisData);
  EXPECT_EQ(ds[2].sensor_type, kSensorOccurrenceData);
  EXPECT_EQ(ds[3].sensor_type, kSensorThreeAxisData);
  EXPECT_EQ(ds[4].sensor_type, kSensorFloatData);
  EXPECT_EQ(ds[5].sensor_type, kSensorOccurrenceData);
  EXPECT_EQ(ds[6].sensor_type, kSensorThreeAxisData);
  EXPECT_EQ(ds[7].sensor_type, kSensorFloatData);

  delete callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_SensorPalRequestsMonitoringWorks) {
  auto callbacks = GetSensorCallbacks();
  SimpleSensorData data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&data));
  sim_->SetNanoappLoadedForTest(true);

  auto sensor_api = chrePalSensorGetApi(12);
  EXPECT_EQ(sensor_api->open(nullptr, callbacks), true);

  const chreSensorInfo* sensors = nullptr;
  uint32_t count;
  EXPECT_TRUE(sensor_api->getSensors(&sensors, &count));
  ASSERT_EQ(count, 4);
  ASSERT_NE(sensors, nullptr);

  sensor_api->configureSensor(0, CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT, 100,
                              1000);
  sim_->AllEventsProcessed();
  sensor_api->configureSensor(1, CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT, 200,
                              1000);

  auto pal_requests = verify::GetReceivedNanoappRequests();
  ASSERT_EQ(pal_requests.size(), 3);
  EXPECT_EQ(pal_requests[0].first, 0);
  EXPECT_EQ(pal_requests[0].second, kRequestTypeGetSensors);
  EXPECT_EQ(pal_requests[1].first, 0);
  EXPECT_EQ(pal_requests[1].second, kRequestTypeConfigureSensor);
  EXPECT_EQ(pal_requests[2].first, 50);
  EXPECT_EQ(pal_requests[2].second, kRequestTypeConfigureSensor);

  delete callbacks;
}

TEST_F(SimulatorCoreTest, SimulatorCore_SensorFlushTest) {
  auto callbacks = GetSensorCallbacks();
  SimpleSensorData data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&data));

  auto sensor_api = chrePalSensorGetApi(12);
  EXPECT_EQ(sensor_api->open(nullptr, callbacks), true);

  const chreSensorInfo* sensors = nullptr;
  uint32_t count;
  EXPECT_TRUE(sensor_api->getSensors(&sensors, &count));

  sensor_api->configureSensor(0, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 100,
                              1000);
  sensor_api->configureSensor(1, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 100,
                              1000);
  sensor_api->configureSensor(2, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 180,
                              1000);
  sensor_api->configureSensor(3, CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT, 200,
                              1000);

  // flush sensor with handle 5. Since it doesn't exist, we should get an error.
  uint32_t request_id;
  sensor_api->flush(5, &request_id);
  EXPECT_EQ(request_id, 1);
  // flush sensor with handle 0 immediately. Since we just made the configure
  // call, we don't expect any data to have been collected, so we should get a
  // response with 0 readings.
  sensor_api->flush(0, &request_id);
  sim_->AllEventsProcessed();
  EXPECT_EQ(request_id, 2);

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 100);  // one shot finished.
  // flush sensor with handle 1. Flush is at t = 100, with a reading every 50ms,
  // so expect 2 readings.
  sensor_api->flush(1, &request_id);
  EXPECT_EQ(request_id, 3);
  sim_->AllEventsProcessed();

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 150);  // sensor with handle 0's second round.
  sensor_api->configureSensor(0, CHRE_SENSOR_CONFIGURE_MODE_DONE, 100, 1000);

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_, 250);  // sensor with handle 1's second round.
  sensor_api->configureSensor(1, CHRE_SENSOR_CONFIGURE_MODE_DONE, 100, 1000);

  sim_->current_time_ = 270;          // sensor with handle 2 should finsih now.
  sensor_api->flush(2, &request_id);  // we should get the full 3 readings.
  EXPECT_EQ(request_id, 4);
  sim_->AllEventsProcessed();

  sim_->AllEventsProcessed();
  EXPECT_EQ(sim_->current_time_,
            270 * 2);  // sensor with handle 2's second round.
  sensor_api->configureSensor(2, CHRE_SENSOR_CONFIGURE_MODE_DONE, 100, 1000);

  EXPECT_FALSE(sim_->dying_);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto ds = (*data_);
  ASSERT_GE(ds.size(), 7);

  EXPECT_EQ(ds[0].sensor_type, kSensorThreeAxisData);
  EXPECT_EQ(ds[0].payload, 0);
  EXPECT_EQ(ds[0].time, 0);
  EXPECT_EQ(ds[1].sensor_type, kSensorByteData);
  EXPECT_EQ(ds[1].payload, 1);
  EXPECT_EQ(ds[1].time, 100);
  EXPECT_EQ(ds[2].sensor_type, kSensorOccurrenceData);
  EXPECT_EQ(ds[2].payload, 2);
  EXPECT_EQ(ds[2].time, 100);
  EXPECT_EQ(ds[3].sensor_type, kSensorThreeAxisData);
  EXPECT_EQ(ds[3].payload, 3);
  EXPECT_EQ(ds[3].time, 150);
  EXPECT_EQ(ds[4].sensor_type, kSensorOccurrenceData);
  EXPECT_EQ(ds[4].payload, 3);
  EXPECT_EQ(ds[4].time, 250);
  EXPECT_EQ(ds[5].sensor_type, kSensorFloatData);
  EXPECT_EQ(ds[5].payload, 3);
  EXPECT_EQ(ds[5].time, 270);
  EXPECT_EQ(ds[6].sensor_type, kSensorFloatData);
  EXPECT_EQ(ds[6].payload, 3);
  EXPECT_EQ(ds[6].time, 270 * 2);

  auto fr = (*flush_responses_);

  ASSERT_EQ(fr.size(), 4);
  EXPECT_EQ(fr[0].first, 5);
  EXPECT_EQ(fr[0].second, 1);
  EXPECT_EQ(fr[1].first, 0);
  EXPECT_EQ(fr[1].second, 2);
  EXPECT_EQ(fr[2].first, 1);
  EXPECT_EQ(fr[2].second, 3);
  EXPECT_EQ(fr[3].first, 2);
  EXPECT_EQ(fr[3].second, 4);

  delete callbacks;
}

class BiasTestOne : public SimpleSensorData {
 public:
  BiasTestOne() {
    sensor_bias_events_ =
        std::vector<std::map<uint64_t, SafeChreBiasEvent*>>(4);
    CreateEventWithTypeAt(0, 20);
    CreateEventWithTypeAt(0, 80);
    CreateEventWithTypeAt(0, 170);
    CreateEventWithTypeAt(1, 20);
    CreateEventWithTypeAt(1, 100);
    CreateEventWithTypeAt(1, 160);
  }

 private:
  void CreateEventWithTypeAt(int sensor_data_type, uint32_t time) {
    auto bias =
        new SafeChreBiasEvent(static_cast<SensorDataType>(sensor_data_type),
                              CHRE_SENSOR_ACCURACY_HIGH);
    if (sensor_data_type == 0) {
      bias->bias_data = chreSensorThreeAxisSampleData{
          .timestampDelta = time,
      };
    } else if (sensor_data_type == 1) {
      bias->bias_data = chreSensorOccurrenceSampleData{
          .timestampDelta = time,
      };
    }

    sensor_bias_events_[sensor_data_type][time] = bias;
  }
};

TEST_F(SimulatorCoreTest, SimulatorCore_SensorBiasTest) {
  auto callbacks = GetSensorCallbacks();
  BiasTestOne data;
  ASSERT_TRUE(sim_->InitializeDataFeed(&data));

  auto sensor_api = chrePalSensorGetApi(12);
  EXPECT_EQ(sensor_api->open(nullptr, callbacks), true);

  // activate sensor 0 but not 1.
  sensor_api->configureSensor(0, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 20,
                              200);

  sim_->AllEventsProcessed();  // t = 30, bias for 0

  // bias for 0 should turn on, but not 1 since sensor 1 isn't active.
  sensor_api->configureBiasEvents(0, true, 10);
  sensor_api->configureBiasEvents(1, true, 10);

  sim_->AllEventsProcessed();  // t= 60, bias for 0 sent at 30.
  sim_->AllEventsProcessed();  // t = 80, bias for 0
  sim_->AllEventsProcessed();  // t = 90
  sim_->AllEventsProcessed();  // t = 120, skip bias for 1

  // activate sensor 1, and disable bias events for 0, enable for 1.
  sensor_api->configureSensor(1, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, 1000,
                              10000);
  sensor_api->configureBiasEvents(0, false, 10);
  sensor_api->configureBiasEvents(1, true, 10);

  sim_->AllEventsProcessed();  // t = 150. bias for 1 sent at 120.
  sim_->AllEventsProcessed();  // t = 160, bias event for 1.
  sim_->AllEventsProcessed();  // t = 180, skip bias event for 0.

  sensor_api->configureSensor(0, CHRE_SENSOR_CONFIGURE_MODE_DONE, 0, 0);
  sensor_api->configureSensor(1, CHRE_SENSOR_CONFIGURE_MODE_DONE, 0, 0);
  sim_->AllEventsProcessed();
  EXPECT_TRUE(sim_->dying_);

  auto resp_data = (*data_);
  std::vector<uint32_t> bias_times = {30, 80, 120, 160};
  std::vector<int> bias_locs = {1, 3, 6, 8};
  std::vector<SensorDataType> bias_types = {
      kSensorThreeAxisData, kSensorThreeAxisData, kSensorOccurrenceData,
      kSensorOccurrenceData};
  int bias_index = 0;
  for (int i = 0; i < resp_data.size(); i++) {
    auto d = resp_data[i];
    if (d.event_type == kBiasEvent) {
      ASSERT_LT(bias_index, bias_times.size());
      EXPECT_EQ(d.time, bias_times[bias_index]);
      EXPECT_EQ(d.sensor_type, bias_types[bias_index]);
      EXPECT_EQ(bias_locs[bias_index], i);
      bias_index++;
    }
  }

  delete callbacks;
}

}  // namespace

}  // namespace lbs::contexthub::testing
