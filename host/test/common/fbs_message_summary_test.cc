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

#include "chre_host/fbs_message_summary.h"
#include "chre_host/host_protocol_host.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace android::chre {

using testing::HasSubstr;

TEST(FbsMessageSummaryTest, DefaultConstructor) {
  const FbsMessageSummary summary;
  ASSERT_EQ(summary.toString(), "[EMPTY]");
}

TEST(FbsMessageSummaryTest, SetProcessingTime) {
  flatbuffers::FlatBufferBuilder builder(48);
  HostProtocolHost::encodePulseRequest(builder);
  const std::string summaryString =
      FbsMessageSummary::fromRawMessage(builder.GetBufferPointer(),
                                        builder.GetSize())
          .setProcessingTime(100)
          .toString();
  ASSERT_THAT(summaryString, HasSubstr("processingTime=100ms"));
}

TEST(FbsMessageSummaryTest, SetError) {
  flatbuffers::FlatBufferBuilder builder(48);
  HostProtocolHost::encodePulseRequest(builder);
  const std::string summaryString =
      FbsMessageSummary::fromRawMessage(builder.GetBufferPointer(),
                                        builder.GetSize())
          .setError("Test Error")
          .toString();

  ASSERT_THAT(summaryString, HasSubstr("error=Test Error"));
}

TEST(FbsMessageSummaryTest, FromRawMessageValidMessage) {
  flatbuffers::FlatBufferBuilder builder(48);
  HostProtocolHost::encodePulseRequest(builder);
  const std::string summaryString =
      FbsMessageSummary::fromRawMessage(builder.GetBufferPointer(),
                                        builder.GetSize())
          .toString();
  ASSERT_NE(summaryString, "[EMPTY]");
  ASSERT_THAT(summaryString, HasSubstr("type=29"));
}

TEST(FbsMessageSummaryTest, FromRawMessageInvalidMessage) {
  const std::vector<uint8_t> invalidMessage{1, 2, 3};

  const std::string summaryString =
      FbsMessageSummary::fromRawMessage(invalidMessage).toString();
  ASSERT_THAT(summaryString, HasSubstr("error=Invalid"));
  ASSERT_THAT(summaryString, HasSubstr("size=3"));
}

TEST(FbsMessageSummaryTest, FromEmptyRawMessage) {
  const std::vector<uint8_t> emptyMessage;
  const FbsMessageSummary summary =
      FbsMessageSummary::fromRawMessage(emptyMessage);
  ASSERT_EQ(summary.toString(), "[EMPTY]");
}

TEST(FbsMessageSummaryTest, VerifyMessageWithNullData) {
  const FbsMessageSummary summary =
      FbsMessageSummary::fromRawMessage(/* data= */ nullptr, /* size= */ 10);
  ASSERT_EQ(summary.toString(), "[EMPTY]");
}

TEST(FbsMessageSummaryTest, VerifyMessageWithZeroSize) {
  const std::vector<uint8_t> message{1, 2, 3};
  const FbsMessageSummary summary =
      FbsMessageSummary::fromRawMessage(message.data(), /* size= */ 0);
  ASSERT_EQ(summary.toString(), "[EMPTY]");
}
}  // namespace android::chre