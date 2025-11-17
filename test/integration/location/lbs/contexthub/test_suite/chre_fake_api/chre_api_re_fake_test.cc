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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_re_fake.h"

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/memory/memory.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

namespace {

using ::lbs::contexthub::FakeChreApiProvider;

TEST_NANOAPP(ChreApiReFakeTest, DefaultNotOverridden) {
  // chreGetTime() uses the current nanoseconds by default which
  // means the first call will be less than the second.
  EXPECT_LT(
      chreGetTime(),
      FakeChreApiProvider::GetInstance()->GetChreApiReFunctions()->GetTime());
}

}  // namespace
