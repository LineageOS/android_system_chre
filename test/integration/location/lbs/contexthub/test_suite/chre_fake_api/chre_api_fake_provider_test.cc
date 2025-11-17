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
#include <gtest/gtest.h>
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_gnss_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_re_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_wifi_fake.h"
#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

namespace {

using ::lbs::contexthub::ChreApiGnssFunctions;
using ::lbs::contexthub::ChreApiReFunctions;
using ::lbs::contexthub::ChreApiWifiFunctions;
using ::lbs::contexthub::FakeChreApiProvider;

class ChreReFunction : public ChreApiReFunctions {
 public:
  uint64_t GetTime() override {
    return 5000UL;
  }
};

class ChreGnssFunction : public ChreApiGnssFunctions {
 public:
  uint32_t GetCapabilities() override {
    return 1 << 20;
  }
};

class ChreWifiFunction : public ChreApiWifiFunctions {
 public:
  uint32_t GetCapabilities() override {
    return 1 << 20;
  }
};

TEST_NANOAPP(ChreApiReFakeTest, OverridesChreGetTime) {
  FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreReFunction>());

  EXPECT_EQ(chreGetTime(), ChreReFunction().GetTime());
}

TEST_NANOAPP(ChreApiReFakeTest, ResetsChreGetTime) {
  FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreReFunction>());

  FakeChreApiProvider::ResetReInstance();

  EXPECT_NE(chreGetTime(), ChreReFunction().GetTime());
}

TEST_NANOAPP(ChreApiReFakeTest, OverrideGetCapabilities) {
  FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreGnssFunction>());

  EXPECT_EQ(chreGnssGetCapabilities(), ChreGnssFunction().GetCapabilities());
}

TEST_NANOAPP(ChreApiReFakeTest, ResetGetCapabilities) {
  FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreGnssFunction>());

  FakeChreApiProvider::ResetGnssInstance();

  EXPECT_NE(chreGnssGetCapabilities(), ChreGnssFunction().GetCapabilities());
}

TEST_NANOAPP(ChreApiWifiFakeTest, TestWifiFunctions) {
  EXPECT_NE(chreWifiGetCapabilities(), ChreWifiFunction().GetCapabilities());

  FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreWifiFunction>());

  EXPECT_EQ(chreWifiGetCapabilities(), ChreWifiFunction().GetCapabilities());

  FakeChreApiProvider::ResetInstance();

  EXPECT_NE(chreWifiGetCapabilities(), ChreWifiFunction().GetCapabilities());
}

}  // namespace