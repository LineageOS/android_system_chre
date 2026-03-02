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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_FAKE_PROVIDER_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_FAKE_PROVIDER_H_

#include <iostream>
#include <memory>
#include <utility>

#include <gmock/gmock.h>
#include "absl/base/no_destructor.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_audio_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_ble_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_event_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_detector.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_gnss_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_msg_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_re_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_sensor_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_wifi_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_wwan_fake.h"

namespace lbs {
namespace contexthub {

// Class that allows overriding the default behaviour of all CHRE APIs.
class FakeChreApiProvider {
 public:
  // Allows overriding the default behaviour when a CHRE API function is called.
  // By default, it invokes real code.
  void SetChreApiFunctions(std::unique_ptr<ChreApiReFunctions> api_re_calls) {
    api_re_calls_ = std::move(api_re_calls);
  }
  void SetChreApiFunctions(
      std::unique_ptr<ChreApiGnssFunctions> api_gnss_calls) {
    api_gnss_calls_ = std::move(api_gnss_calls);
  }
  void SetChreApiFunctions(
      std::unique_ptr<ChreApiWifiFunctions> api_wifi_calls) {
    api_wifi_calls_ = std::move(api_wifi_calls);
  }
  void SetChreApiFunctions(
      std::unique_ptr<ChreApiAudioFunctions> api_audio_calls) {
    api_audio_calls_ = std::move(api_audio_calls);
  }
  void SetChreApiFunctions(std::unique_ptr<ChreApiBleFunctions> api_ble_calls) {
    api_ble_calls_ = std::move(api_ble_calls);
  }
  void SetChreApiFunctions(
      std::unique_ptr<ChreApiWwanFunctions> api_wwan_calls) {
    api_wwan_calls_ = std::move(api_wwan_calls);
  }
  void SetChreApiFunctions(
      std::unique_ptr<ChreApiSensorFunctions> api_sensor_calls) {
    api_sensor_calls_ = std::move(api_sensor_calls);
  }
  void SetChreApiFunctions(
      std::unique_ptr<ChreApiEventFunctions> api_event_calls) {
    api_event_calls_ = std::move(api_event_calls);
  }
  void SetChreApiFunctions(
      std::unique_ptr<ChreApiMsgFunctions> api_msg_calls) {
    api_msg_calls_ = std::move(api_msg_calls);
  }

  // Sets, resets, and gets the detector instance.
  ::testing::NiceMock<ChreApiDetector>* GetFakeDetector();

  // Retrieves the current static instance.
  static FakeChreApiProvider* GetInstance();

  // Retrieves the current set of functions that will be called when code
  // interacts with the CHRE API.
  ChreApiReFunctions* GetChreApiReFunctions();
  ChreApiGnssFunctions* GetChreApiGnssFunctions();
  ChreApiWifiFunctions* GetChreApiWifiFunctions();
  ChreApiAudioFunctions* GetChreApiAudioFunctions();
  ChreApiBleFunctions* GetChreApiBleFunctions();
  ChreApiWwanFunctions* GetChreApiWwanFunctions();
  ChreApiSensorFunctions* GetChreApiSensorFunctions();
  ChreApiEventFunctions* GetChreApiEventFunctions();
  ChreApiMsgFunctions* GetChreApiMsgFunctions();

  // Resets the implementations of the static instance to the default.
  static void ResetInstance();

  // Resets the implementations of the sub-APIs to the default ones.
  static void ResetReInstance();
  static void ResetGnssInstance();
  static void ResetWifiInstance();
  static void ResetAudioInstance();
  static void ResetBleInstance();
  static void ResetWwanInstance();
  static void ResetSensorInstance();
  static void ResetEventInstance();
  static void ResetMsgInstance();

 private:
  friend absl::NoDestructor<FakeChreApiProvider>;

  // Require using GetInstance to interact with the class.
  FakeChreApiProvider() = default;

  // Contains the various overwritten functions that CHRE can call.
  std::unique_ptr<ChreApiReFunctions> api_re_calls_;
  std::unique_ptr<ChreApiGnssFunctions> api_gnss_calls_;
  std::unique_ptr<ChreApiWifiFunctions> api_wifi_calls_;
  std::unique_ptr<ChreApiAudioFunctions> api_audio_calls_;
  std::unique_ptr<ChreApiBleFunctions> api_ble_calls_;
  std::unique_ptr<ChreApiWwanFunctions> api_wwan_calls_;
  std::unique_ptr<ChreApiSensorFunctions> api_sensor_calls_;
  std::unique_ptr<ChreApiEventFunctions> api_event_calls_;
  std::unique_ptr<ChreApiMsgFunctions> api_msg_calls_;

  // Contains the detector that allows for EXPECT_CALL to execute correctly
  std::unique_ptr<::testing::NiceMock<ChreApiDetector>> chre_api_fake_detector_;

  // Contains the various default functions that CHRE can call.
  ChreApiReFunctions default_api_re_calls_;
  ChreApiGnssFunctions default_api_gnss_calls_;
  ChreApiWifiFunctions default_api_wifi_calls_;
  ChreApiAudioFunctions default_api_audio_calls_;
  ChreApiBleFunctions default_api_ble_calls_;
  ChreApiWwanFunctions default_api_wwan_calls_;
  ChreApiSensorFunctions default_api_sensor_calls_;
  ChreApiEventFunctions default_api_event_calls_;
  ChreApiMsgFunctions default_api_msg_calls_;
};

}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_FAKE_PROVIDER_H_
