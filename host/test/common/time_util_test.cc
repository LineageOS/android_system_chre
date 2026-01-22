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

#include "chre_host/time_util.h"

#include <string>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace android::chre {

using testing::HasSubstr;
using testing::Not;

TEST(TimeUtilTest, AppendWalltimeToTimestampEmptyString) {
  EXPECT_EQ(appendWalltimeToTimestamp("", std::nullopt), "");
}

TEST(TimeUtilTest, AppendWalltimeToTimestampNoMatch) {
  std::string input = "No timestamp here";
  EXPECT_EQ(appendWalltimeToTimestamp(input, std::nullopt), input);
}

TEST(TimeUtilTest, AppendWalltimeToTimestampTs) {
  // 1000000000 ns = 1s
  std::string input = "Log ts=1000000000";
  // formatNanos(1000000000) -> "1.000 000 000"
  std::string expected = "Log ts=1.000 000 000";
  EXPECT_EQ(appendWalltimeToTimestamp(input, std::nullopt), expected);
}

TEST(TimeUtilTest, AppendWalltimeToTimestampTime) {
  std::string input = "Log time=1000000000";
  std::string expected = "Log ts=1.000 000 000";
  EXPECT_EQ(appendWalltimeToTimestamp(input, std::nullopt), expected);
}

TEST(TimeUtilTest, AppendWalltimeToTimestampTimeNs) {
  std::string input = "Log time(ns)=1000000000";
  std::string expected = "Log ts=1.000 000 000";
  EXPECT_EQ(appendWalltimeToTimestamp(input, std::nullopt), expected);
}

TEST(TimeUtilTest, AppendWalltimeToTimestampMs) {
  std::string input = "Log ts=1000ms";
  std::string expected = "Log ts=1000ms";
  EXPECT_EQ(appendWalltimeToTimestamp(input, std::nullopt), expected);
}

TEST(TimeUtilTest, AppendWalltimeToTimestampWithOffset) {
  uint64_t ts = 1000000000;
  std::string input = "Log ts=" + std::to_string(ts);
  int64_t offset = 0;

  std::string result = appendWalltimeToTimestamp(input, offset);
  EXPECT_NE(result, input);
  // Should contain something in brackets
  EXPECT_TRUE(result.find("[") != std::string::npos);
  EXPECT_TRUE(result.find("]") != std::string::npos);
  // Also should have formatted nanos
  EXPECT_TRUE(result.find("1.000 000 000") != std::string::npos);
  // Calculation should be successful
  EXPECT_THAT(result, Not(HasSubstr("Error")));
}

TEST(TimeUtilTest, AppendWalltimeToTimestampMultiple) {
  std::string input = "ts=1000000000 time=2000000000";
  std::string expected = "ts=1.000 000 000 ts=2.000 000 000";
  EXPECT_EQ(appendWalltimeToTimestamp(input, std::nullopt), expected);
}

TEST(TimeUtilTest, AppendWalltimeToTimestampExistingNs) {
  std::string input = "ts=1000000000ns";
  std::string expected = "ts=1.000 000 000";  // "ns" is stripped/skipped
  EXPECT_EQ(appendWalltimeToTimestamp(input, std::nullopt), expected);
}

TEST(TimeUtilTest, AppendWalltimeToTimestampMsWithOffset) {
  std::string input = "ts=1000ms";
  int64_t offset = 0;
  std::string result = appendWalltimeToTimestamp(input, offset);
  EXPECT_TRUE(result.find("ts=1000ms") != std::string::npos);
  EXPECT_TRUE(result.find("[") != std::string::npos);
  // Calculation should be successful
  EXPECT_THAT(result, Not(HasSubstr("Error")));
}

TEST(TimeUtilTest, AppendWalltimeToTimestampTimeWithOffset) {
  std::string input = "time=1000000000";
  int64_t offset = 0;
  std::string result = appendWalltimeToTimestamp(input, offset);
  // Key is normalized to ts=
  EXPECT_THAT(result, HasSubstr("ts=1.000 000 000"));
  EXPECT_THAT(result, HasSubstr("["));
  EXPECT_THAT(result, Not(HasSubstr("Error")));
}

TEST(TimeUtilTest, AppendWalltimeToTimestampTimeNsWithOffset) {
  std::string input = "time(ns)=1000000000";
  int64_t offset = 0;
  std::string result = appendWalltimeToTimestamp(input, offset);
  // Key is normalized to ts=
  EXPECT_THAT(result, HasSubstr("ts=1.000 000 000"));
  EXPECT_THAT(result, HasSubstr("["));
  EXPECT_THAT(result, Not(HasSubstr("Error")));
}

}  // namespace android::chre
