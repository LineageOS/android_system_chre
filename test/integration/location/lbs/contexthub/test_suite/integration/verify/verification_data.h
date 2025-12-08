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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_VERIFY_VERIFICATION_DATA_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_VERIFY_VERIFICATION_DATA_H_

#include <chre.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"

typedef chreMessageFromHostData chreMessageToHostData;

namespace lbs::contexthub::testing::verify {

// exports all messages received from the nanoapps. In the resulting vector,
// each element is a pair containing the time when the message was received,
// along with the message itself.
std::vector<std::pair<uint64_t, chreMessageToHostData>> GetHostMessages();

// vector that identifies which PAL Apis were called by the nanoapps and when.
std::vector<std::pair<uint64_t, NanoappRequestType>>
GetReceivedNanoappRequests();

}  // namespace lbs::contexthub::testing::verify

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_VERIFY_VERIFICATION_DATA_H_