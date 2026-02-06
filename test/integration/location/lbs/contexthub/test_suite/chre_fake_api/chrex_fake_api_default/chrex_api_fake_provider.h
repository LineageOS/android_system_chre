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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHREX_API_FAKE_PROVIDER_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHREX_API_FAKE_PROVIDER_H_

#include <iostream>
#include <memory>
#include <utility>

#include <gmock/gmock.h>
#include "absl/base/no_destructor.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

namespace lbs {
namespace contexthub {

class FakeChrexApiProvider {
 public:
  static FakeChrexApiProvider *GetInstance() {
    static FakeChrexApiProvider instance;
    return &instance;
  }

  static void ResetInstance() {}

  ChrexApiDetector *GetFakeDetector() {
    return &detector_;
  }

 private:
  FakeChrexApiProvider() = default;
  ~FakeChrexApiProvider() = default;

  ChrexApiDetector detector_;
};

}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHREX_API_FAKE_PROVIDER_H_
