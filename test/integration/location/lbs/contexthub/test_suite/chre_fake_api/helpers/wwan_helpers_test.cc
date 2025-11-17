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

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/wwan_helpers.h"

#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

namespace lbs {
namespace contexthub {
namespace helpers {
namespace {

TEST_NANOAPP(ChreWwanHelpersTest, SanityTest) {
  EXPECT_CALL(*chre_api_fake_detector_, chreWwanGetCapabilities).Times(2);

  SetWwanFullCapabilities();
  EXPECT_EQ(chreWwanGetCapabilities(), CHRE_WWAN_GET_CELL_INFO);
  EXPECT_TRUE(chreWwanGetCellInfoAsync(nullptr));

  SetWwanNoCapabilities();
  EXPECT_EQ(chreWwanGetCapabilities(), CHRE_WWAN_CAPABILITIES_NONE);
  EXPECT_FALSE(chreWwanGetCellInfoAsync(nullptr));
}

}  // namespace
}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs