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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/gnss_helpers.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/re_helpers.h"
#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

// Helper functions for setting up the test environment.
using ::lbs::contexthub::helpers::SetConstantTime;
using ::lbs::contexthub::helpers::SetGnssFullCapabilities;
using ::lbs::contexthub::helpers::SetGnssNoCapabilities;

// Forward declarations for the C-style CHRE API functions.
extern "C" {
uint64_t chreGetTime();
uint32_t chreGnssGetCapabilities();
}

void callGetTimeTwice() {
  chreGetTime();
  chreGetTime();
}

bool canRunGnss() {
  return chreGnssGetCapabilities() != 0;
}

TEST_NANOAPP(SomeNanoappTest, TestCaseOne) {
  SetConstantTime();

  EXPECT_CALL(*chre_api_fake_detector_, chreGetTime()).Times(2);
  callGetTimeTwice();
  SetGnssFullCapabilities();
  EXPECT_TRUE(canRunGnss());
  SetGnssNoCapabilities();
  EXPECT_FALSE(canRunGnss());
}