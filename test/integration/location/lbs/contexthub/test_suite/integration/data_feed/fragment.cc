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

#include "location/lbs/contexthub/test_suite/integration/data_feed/fragment.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "absl/types/span.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"

namespace lbs {
namespace contexthub {
namespace testing {

using std::byte;

std::vector<SafeChreMessageFromHostData> FragmentHostMessage(
    const uint8_t message_id, const SafeChreMessageFromHostData &original) {
  bool finished = false;
  int remaining_msg_size = original.messageSize;
  int read_from_index = 0;
  auto byte_message = static_cast<const byte *>(original.message);

  std::vector<SafeChreMessageFromHostData> fragments;
  while (!finished) {
    // the FirstFragmentHeader is added to the first fragment.
    int header_size =
        kGeneralHeaderSize + (fragments.empty() ? kFirstHeaderSize : 0);
    int allocation_size =
        header_size +
        std::min(remaining_msg_size, kMaxFragmentSize - header_size);
    remaining_msg_size -= (allocation_size - header_size);
    finished = (remaining_msg_size == 0);

    auto curr_fragment = static_cast<byte *>(malloc(allocation_size));

    // fragment header
    FragmentHeader fh{
        .is_last_fragment = finished,
        .version = 0,
        .message_id = message_id,
        .index = static_cast<unsigned int>(fragments.size()),
        .message_length_msb = static_cast<unsigned int>(
            (allocation_size - kGeneralHeaderSize) >> 8),
        .message_length_lsb = static_cast<unsigned int>(
            (allocation_size - kGeneralHeaderSize) & (0xFF)),
    };
    memcpy(curr_fragment, &fh, kGeneralHeaderSize);

    if (fragments.empty()) {
      // first fragment, encode extra information

      FirstFragmentHeader ffh{
          .version = static_cast<unsigned int>(original.message_version),
          .message_type_msb =
              static_cast<unsigned int>(original.messageType) >> 8,
          .message_type_lsb =
              static_cast<unsigned int>(original.messageType) & (0xFF),
          .message_version =
              static_cast<unsigned int>(original.message_version),
      };
      memcpy(curr_fragment + kGeneralHeaderSize, &ffh, kFirstHeaderSize);
    }

    memcpy(curr_fragment + header_size, byte_message + read_from_index,
           allocation_size - header_size);
    read_from_index += allocation_size - header_size;

    SafeChreMessageFromHostData wrapped;
    wrapped.appId = original.appId;
    wrapped.hostEndpoint = original.hostEndpoint;
    wrapped.message = curr_fragment;
    wrapped.messageSize = allocation_size;
    wrapped.messageType = kFragmentedMessageType;

    fragments.push_back(wrapped);
  }

  return fragments;
}

SafeChreMessageFromHostData CombineHostMessageFragments(
    absl::Span<const SafeChreMessageFromHostData> fragments) {
  SafeChreMessageFromHostData combined;
  FirstFragmentHeader ffh;
  auto message = static_cast<const byte *>(fragments[0].message);
  memcpy(&ffh, message + kGeneralHeaderSize, kFirstHeaderSize);

  combined.messageType = (ffh.message_type_msb << 8) + ffh.message_type_lsb;
  combined.appId = fragments[0].appId;
  combined.hostEndpoint = fragments[0].hostEndpoint;
  combined.messageSize = 0;

  // each message except contains an extra kGeneralHeaderSize bytes of for the
  // header. The first fragment has an extra kFristHeaderSize as well. These
  // have to be removed to get the actual message size.
  for (const auto &fragment : fragments)
    combined.messageSize += fragment.messageSize - kGeneralHeaderSize;
  combined.messageSize -= kFirstHeaderSize;

  auto final_message = static_cast<byte *>(malloc(combined.messageSize));
  int final_message_index = 0;

  for (const auto &fragment : fragments) {
    auto start_at_byte =
        kGeneralHeaderSize + (final_message_index == 0 ? kFirstHeaderSize : 0);
    auto bytes_to_copy = fragment.messageSize - start_at_byte;
    memcpy(final_message + final_message_index,
           static_cast<const byte *>(fragment.message) + start_at_byte,
           bytes_to_copy);

    final_message_index += bytes_to_copy;
  }

  combined.message = final_message;
  return combined;
}

void FillFragmentHeader(const SafeChreMessageFromHostData &fragment,
                        FragmentHeader *header) {
  memcpy(header, fragment.message, kGeneralHeaderSize);
}

}  // namespace testing
}  // namespace contexthub
}  // namespace lbs