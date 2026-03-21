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

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"

namespace {

using lbs::contexthub::testing::CombineHostMessageFragments;
using lbs::contexthub::testing::FirstFragmentHeader;
using lbs::contexthub::testing::FragmentHeader;
using lbs::contexthub::testing::FragmentHostMessage;
using lbs::contexthub::testing::kFirstHeaderSize;
using lbs::contexthub::testing::kGeneralHeaderSize;
using std::byte;

TEST(FragmentsTest, SingleMessageFragmentWorks) {
  byte* message = static_cast<byte*>(malloc(8));
  for (int i = 0; i < 8; i++) message[i] = static_cast<byte>(i);
  SafeChreMessageFromHostData host_msg;
  host_msg.message = message;
  host_msg.messageType = 3;
  host_msg.appId = 5;
  host_msg.messageSize = 8;
  host_msg.hostEndpoint = 0;

  auto out = FragmentHostMessage(2, host_msg);
  ASSERT_EQ(out.size(), 1);

  auto fragment = out[0];
  EXPECT_EQ(fragment.hostEndpoint, host_msg.hostEndpoint);
  EXPECT_EQ(fragment.appId, host_msg.appId);
  EXPECT_EQ(fragment.messageType, 1025);
  EXPECT_EQ(fragment.messageSize,
            host_msg.messageSize + kGeneralHeaderSize + kFirstHeaderSize);

  auto frag_msg = static_cast<const byte*>(fragment.message);
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(message[i], frag_msg[i + 8]);
  }

  FragmentHeader expected_fh;
  FirstFragmentHeader expected_ffh;
  memcpy(&expected_fh, frag_msg, kGeneralHeaderSize);
  memcpy(&expected_ffh, frag_msg + kGeneralHeaderSize, kFirstHeaderSize);

  EXPECT_EQ(expected_fh.message_id, 2);
  EXPECT_EQ(expected_fh.version, 0);
  EXPECT_EQ(expected_fh.is_last_fragment, true);
  EXPECT_EQ(expected_fh.index, 0);
  EXPECT_EQ(expected_fh.message_length_msb, 0);
  EXPECT_EQ(expected_fh.message_length_lsb, 12);

  EXPECT_EQ(expected_ffh.message_type_msb, 0);
  EXPECT_EQ(expected_ffh.message_type_lsb, 3);
  EXPECT_EQ(expected_ffh.message_version, 1);
  EXPECT_EQ(expected_ffh.message_version, 1);
}

TEST(FragmentsTest, MultiMessageFragmentWorks) {
  // by accounting for header size, a message of size 1020 will be split into
  // 2 messages: the first containing bytes [0,1016), with total size 1024,
  // the second containing bytes [1016,1020) with total size 8.
  byte* message = static_cast<byte*>(malloc(1020));
  for (int i = 0; i < 1020; i++) message[i] = static_cast<byte>(i);
  SafeChreMessageFromHostData host_msg;
  host_msg.message = message;
  host_msg.messageType = 5;
  host_msg.appId = 5;
  host_msg.messageSize = 1020;
  host_msg.hostEndpoint = 0;

  auto out = FragmentHostMessage(2, host_msg);
  ASSERT_EQ(out.size(), 2);

  auto fragment = out[0];
  EXPECT_EQ(fragment.hostEndpoint, host_msg.hostEndpoint);
  EXPECT_EQ(fragment.appId, host_msg.appId);
  EXPECT_EQ(fragment.messageType, 1025);
  EXPECT_EQ(fragment.messageSize, 1024);

  auto frag_msg = static_cast<const byte*>(fragment.message);
  FragmentHeader expected_fh;
  FirstFragmentHeader expected_ffh;
  memcpy(&expected_fh, frag_msg, kGeneralHeaderSize);
  memcpy(&expected_ffh, frag_msg + kGeneralHeaderSize, kFirstHeaderSize);

  EXPECT_EQ(expected_fh.message_id, 2);
  EXPECT_EQ(expected_fh.version, 0);
  EXPECT_EQ(expected_fh.is_last_fragment, false);
  EXPECT_EQ(expected_fh.index, 0);
  EXPECT_EQ(
      (expected_fh.message_length_msb << 8) + expected_fh.message_length_lsb,
      1020);

  EXPECT_EQ(expected_ffh.message_type_msb, 0);
  EXPECT_EQ(expected_ffh.message_type_lsb, 5);
  EXPECT_EQ(expected_ffh.message_version, 1);
  EXPECT_EQ(expected_ffh.message_version, 1);

  // the last fragment byte should be the 1016th byte of message above.
  EXPECT_EQ(message[1015], frag_msg[1023]);

  auto fragment2 = out[1];
  EXPECT_EQ(fragment2.hostEndpoint, host_msg.hostEndpoint);
  EXPECT_EQ(fragment2.appId, host_msg.appId);
  EXPECT_EQ(fragment2.messageType, 1025);
  EXPECT_EQ(fragment2.messageSize, 8);

  auto frag_msg2 = static_cast<const byte*>(fragment2.message);

  memcpy(&expected_fh, frag_msg2, kGeneralHeaderSize);

  EXPECT_EQ(expected_fh.message_id, 2);
  EXPECT_EQ(expected_fh.version, 0);
  EXPECT_EQ(expected_fh.is_last_fragment, true);
  EXPECT_EQ(expected_fh.index, 1);
  EXPECT_EQ(expected_fh.message_length_msb, 0);
  EXPECT_EQ(expected_fh.message_length_lsb, 4);

  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(message[1016 + i], frag_msg2[i + kGeneralHeaderSize]);
  }
}

TEST(FragmentsTest, SingleMessageCombineWorks) {
  FragmentHeader fh{
      .is_last_fragment = true,
      .version = 0,
      .message_id = 2,
      .index = 0,
      .message_length_msb = 0,
      .message_length_lsb = 8,
  };

  FirstFragmentHeader ffh{
      .version = 1,
      .message_type_msb = 0,
      .message_type_lsb = 12,
      .message_version = 1,
  };

  byte* message = static_cast<byte*>(malloc(12));
  memcpy(message, &fh, kGeneralHeaderSize);
  memcpy((message + kGeneralHeaderSize), &ffh, kFirstHeaderSize);
  for (int i = 0; i < 4; i++)
    message[kFirstHeaderSize + kGeneralHeaderSize + i] = static_cast<byte>(i);

  auto fragmented = std::vector<SafeChreMessageFromHostData>(1);
  fragmented[0].messageType = 1025;
  fragmented[0].message = message;
  fragmented[0].messageSize = 12;
  fragmented[0].hostEndpoint = 7;
  fragmented[0].appId = 3;

  auto combined = CombineHostMessageFragments(fragmented);
  EXPECT_EQ(combined.messageType, 12);
  EXPECT_EQ(combined.appId, 3);
  EXPECT_EQ(combined.messageSize, 4);
  EXPECT_EQ(combined.hostEndpoint, 7);
  auto int_msg = static_cast<const byte*>(combined.message);
  for (int i = 0; i < 4; i++) EXPECT_EQ((int)int_msg[i], i);
}

TEST(FragmentsTest, MultiMessageCombi1orks) {
  byte* message = static_cast<byte*>(malloc(1020));
  for (int i = 0; i < 1020; i++) message[i] = static_cast<byte>(i);
  SafeChreMessageFromHostData host_msg;
  host_msg.message = message;
  host_msg.messageType = 5;
  host_msg.appId = 5;
  host_msg.messageSize = 1020;
  host_msg.hostEndpoint = 0;

  auto fragmented = FragmentHostMessage(2, host_msg);
  auto combined = CombineHostMessageFragments(fragmented);

  EXPECT_EQ(host_msg.messageType, combined.messageType);
  EXPECT_EQ(host_msg.appId, combined.appId);
  EXPECT_EQ(host_msg.messageSize, combined.messageSize);
  EXPECT_EQ(host_msg.hostEndpoint, combined.hostEndpoint);
  auto comb_msg = static_cast<const byte*>(combined.message);

  for (int i = 0; i < 11; i++) {
    EXPECT_EQ(message[i * 101], comb_msg[i * 101]);
  }
}

}  // namespace
