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

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/gnss_helpers.h"

#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

namespace lbs {
namespace contexthub {
namespace helpers {
namespace {

TEST_NANOAPP(ChreGnssHelpersTest, FullCapabilitesWorks) {
  SetGnssFullCapabilities();

  EXPECT_EQ(chreGnssGetCapabilities(),
            CHRE_GNSS_CAPABILITIES_LOCATION |
                CHRE_GNSS_CAPABILITIES_MEASUREMENTS |
                CHRE_GNSS_CAPABILITIES_GNSS_ENGINE_BASED_PASSIVE_LISTENER);
  EXPECT_TRUE(chreGnssLocationSessionStartAsync(
      /*minIntervalMs=*/0, /*minTimeToNextFixMs=*/0, /*cookie=*/nullptr));
  EXPECT_TRUE(chreGnssMeasurementSessionStartAsync(/*minIntervalMs=*/0,
                                                   /*cookie=*/nullptr));
}

TEST_NANOAPP(ChreGnssHelpersTest, NoCapabilitesWorks) {
  SetGnssNoCapabilities();

  EXPECT_EQ(chreGnssGetCapabilities(), CHRE_GNSS_CAPABILITIES_NONE);
  EXPECT_FALSE(chreGnssLocationSessionStartAsync(
      /*minIntervalMs=*/0, /*minTimeToNextFixMs=*/0, /*cookie=*/nullptr));
  EXPECT_FALSE(chreGnssMeasurementSessionStartAsync(/*minIntervalMs=*/0,
                                                    /*cookie=*/nullptr));
}

}  // namespace
}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs