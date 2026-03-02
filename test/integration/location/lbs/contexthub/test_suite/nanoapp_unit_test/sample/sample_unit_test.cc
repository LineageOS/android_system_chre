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

// Defines a test case named TestCaseOne that belongs to the SomeNanoappTest
// suite, automatically handling the CHRE test fixture setup. This Macro
// TEST_NANOAPP is for your unit test it is specialized to ensure your test run
// with the correct CHRE simulation environment.
TEST_NANOAPP(SomeNanoappTest, TestCaseOne) {
  SetConstantTime();

  // Verifies get time is called twice. Function needs to be under the test to
  // verify.
  EXPECT_CALL(*chre_api_fake_detector_, chreGetTime()).Times(2);
  callGetTimeTwice();          // Function under test.
  SetGnssFullCapabilities();   // Set GNSS to be available.
  EXPECT_TRUE(canRunGnss());   // Check result to expect true.
  SetGnssNoCapabilities();     // Change GNSS functionality (Make GNSS
                               // unavailable).
  EXPECT_FALSE(canRunGnss());  // Check the result to expect false.
}
