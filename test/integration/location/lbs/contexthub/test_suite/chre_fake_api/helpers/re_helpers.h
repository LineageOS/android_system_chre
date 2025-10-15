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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_RE_HELPERS_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_RE_HELPERS_H_

#include "absl/memory/memory.h"
#include "chre_api/chre/re.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_re_fake.h"
// #include "util/gtl/flat_set.h"

// This file provides helper time-related functions that overrides re CHRE API
// calls with their common uses in unit tests.
namespace lbs {
namespace contexthub {
namespace helpers {

// The expected return value from chreGetTime(). In nanoseconds.
// Value was chosen arbitrarily to match values currently used in tests.
constexpr uint64_t kDefaultCurrentTime = 555;
// The expected return value from GetEstimatedHostTimeOffset(). In nanoseconds.
// Value was chosen arbitrarily to match values currently used in tests.
constexpr int64_t kDefaultHostOffset = 444;

void SetConstantTime(uint64_t current_time = kDefaultCurrentTime,
                     int64_t host_offset = kDefaultHostOffset);

void SetHostMessageSize(uint32_t host_message_size);

// SetEmptyRe is only intended to be used during shutdown of the nanoapp to
// bypass the loss of context. All functions are unavailable in this class.
void SetEmptyRe();

}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_RE_HELPERS_H_