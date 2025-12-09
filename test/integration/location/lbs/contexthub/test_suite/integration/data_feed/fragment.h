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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_DATA_FEED_FRAGMENT_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_DATA_FEED_FRAGMENT_H_

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"

namespace lbs {
namespace contexthub {
namespace testing {

constexpr int kMaxFragmentSize = 1024;
constexpr int kFragmentedMessageType = 1025;

// FragmentHeader represents the fragment header in a human readable format.
struct FragmentHeader {
  unsigned int is_last_fragment : 2;
  unsigned int version : 2;
  unsigned int message_id : 4;
  unsigned int index : 8;
  unsigned int message_length_msb : 8;
  unsigned int message_length_lsb : 8;
} __attribute__((packed));

const int kGeneralHeaderSize = sizeof(FragmentHeader);

// if this is the first fragment of the message, we encode extra information.
struct FirstFragmentHeader {
  unsigned int version : 8;
  unsigned int message_type_msb : 8;
  unsigned int message_type_lsb : 8;
  unsigned int message_version : 8;
} __attribute__((packed));

const int kFirstHeaderSize = sizeof(FirstFragmentHeader);

// FragmentHostMessage breaks down the host messages into multiple fragments
// according to the above defined fragment formats.
std::vector<SafeChreMessageFromHostData> FragmentHostMessage(
    uint8_t message_id, const SafeChreMessageFromHostData &original);

// CombineHostMessageFragments takes a vector of host messages and combines them
// in an inverse fashion to how the above FragmentHostMessage fragments them.
SafeChreMessageFromHostData CombineHostMessageFragments(
    absl::Span<const SafeChreMessageFromHostData> fragments);

// Helper function to get fragment header. Can be used to check the message_id,
// index, and whether this is the final fragment.
void FillFragmentHeader(const SafeChreMessageFromHostData &fragment,
                        FragmentHeader *header);

}  // namespace testing
}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_DATA_FEED_FRAGMENT_H_