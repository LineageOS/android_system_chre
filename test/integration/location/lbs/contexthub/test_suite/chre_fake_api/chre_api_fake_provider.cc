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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

#include <memory>

#include <gmock/gmock.h>
#include "absl/base/no_destructor.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_audio_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_ble_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_event_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_detector.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_gnss_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_re_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_sensor_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_wifi_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_wwan_fake.h"

namespace lbs {
namespace contexthub {

// static
FakeChreApiProvider *FakeChreApiProvider::GetInstance() {
  static absl::NoDestructor<FakeChreApiProvider> api;
  return api.get();
}

ChreApiReFunctions *FakeChreApiProvider::GetChreApiReFunctions() {
  ChreApiReFunctions *overridden_functions = api_re_calls_.get();
  if (overridden_functions) {
    return overridden_functions;
  }
  return &default_api_re_calls_;
}

ChreApiGnssFunctions *FakeChreApiProvider::GetChreApiGnssFunctions() {
  ChreApiGnssFunctions *overridden_functions = api_gnss_calls_.get();
  if (overridden_functions) {
    return overridden_functions;
  }
  return &default_api_gnss_calls_;
}

ChreApiWifiFunctions *FakeChreApiProvider::GetChreApiWifiFunctions() {
  ChreApiWifiFunctions *overridden_functions = api_wifi_calls_.get();
  if (overridden_functions) {
    return overridden_functions;
  }
  return &default_api_wifi_calls_;
}

ChreApiAudioFunctions *FakeChreApiProvider::GetChreApiAudioFunctions() {
  ChreApiAudioFunctions *overridden_functions = api_audio_calls_.get();
  if (overridden_functions) {
    return overridden_functions;
  }
  return &default_api_audio_calls_;
}

ChreApiBleFunctions *FakeChreApiProvider::GetChreApiBleFunctions() {
  ChreApiBleFunctions *overridden_functions = api_ble_calls_.get();
  if (overridden_functions) {
    return overridden_functions;
  }
  return &default_api_ble_calls_;
}

ChreApiWwanFunctions *FakeChreApiProvider::GetChreApiWwanFunctions() {
  ChreApiWwanFunctions *overridden_functions = api_wwan_calls_.get();
  if (overridden_functions) {
    return overridden_functions;
  }
  return &default_api_wwan_calls_;
}

ChreApiSensorFunctions *FakeChreApiProvider::GetChreApiSensorFunctions() {
  ChreApiSensorFunctions *overridden_functions = api_sensor_calls_.get();
  if (overridden_functions) {
    return overridden_functions;
  }
  return &default_api_sensor_calls_;
}

ChreApiEventFunctions *FakeChreApiProvider::GetChreApiEventFunctions() {
  ChreApiEventFunctions *overridden_functions = api_event_calls_.get();
  if (overridden_functions) {
    return overridden_functions;
  }
  return &default_api_event_calls_;
}

testing::NiceMock<ChreApiDetector> *FakeChreApiProvider::GetFakeDetector() {
  if (chre_api_fake_detector_ == nullptr) {
    chre_api_fake_detector_ =
        std::make_unique<testing::NiceMock<ChreApiDetector>>();
  }

  return chre_api_fake_detector_.get();
}

// static
void FakeChreApiProvider::ResetInstance() {
  GetInstance()->chre_api_fake_detector_ = nullptr;
  ResetReInstance();
  ResetGnssInstance();
  ResetWifiInstance();
  ResetAudioInstance();
  ResetBleInstance();
  ResetWwanInstance();
  ResetSensorInstance();
  ResetEventInstance();
}

// static
void FakeChreApiProvider::ResetReInstance() {
  GetInstance()->SetChreApiFunctions(std::unique_ptr<ChreApiReFunctions>());
}

// static
void FakeChreApiProvider::ResetGnssInstance() {
  GetInstance()->SetChreApiFunctions(std::unique_ptr<ChreApiGnssFunctions>());
}

// static
void FakeChreApiProvider::ResetWifiInstance() {
  GetInstance()->SetChreApiFunctions(std::unique_ptr<ChreApiWifiFunctions>());
}

// static
void FakeChreApiProvider::ResetAudioInstance() {
  GetInstance()->SetChreApiFunctions(std::unique_ptr<ChreApiAudioFunctions>());
}

// static
void FakeChreApiProvider::ResetBleInstance() {
  GetInstance()->SetChreApiFunctions(std::unique_ptr<ChreApiBleFunctions>());
}

// static
void FakeChreApiProvider::ResetWwanInstance() {
  GetInstance()->SetChreApiFunctions(std::unique_ptr<ChreApiWwanFunctions>());
}

// static
void FakeChreApiProvider::ResetSensorInstance() {
  GetInstance()->SetChreApiFunctions(std::unique_ptr<ChreApiSensorFunctions>());
}

// static
void FakeChreApiProvider::ResetEventInstance() {
  GetInstance()->SetChreApiFunctions(std::unique_ptr<ChreApiEventFunctions>());
}

}  // namespace contexthub
}  // namespace lbs