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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_CHRE_INTEGRATION_LIB_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_CHRE_INTEGRATION_LIB_H_

#include <string>

#include <gtest/gtest.h>
#include "absl/flags/declare.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/data_feed_base.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

ABSL_DECLARE_FLAG(std::string, nanoapps);

#define INTEGRATION_TEST(test_suite, data_feed, test_name)                     \
  void RunTest##test_name();                                                   \
  TEST(test_suite, test_name) {                                                \
    auto data = data_feed();                                                   \
    auto sim = lbs::contexthub::testing::Simulator::GetInstance();             \
    ASSERT_TRUE(sim->InitializeDataFeed(&data))                                \
        << "Your data feed is invalid. Please check the above error messages " \
           "to know how to fix this.";                                         \
    if (!sim->dying_) sim->Run(absl::GetFlag(FLAGS_nanoapps));                 \
    RunTest##test_name();                                                      \
    sim->ResetInstance();                                                      \
  }                                                                            \
  void RunTest##test_name()

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_CHRE_INTEGRATION_LIB_H_