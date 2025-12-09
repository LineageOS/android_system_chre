/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <gtest/gtest.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <cstring>

#include "chpp/common/event_log.h"

namespace chre {

namespace {

class EventLogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    chppEventLogInit(&mEventLog, kCapacity);
  }

  void TearDown() override {
    chppEventLogDeinit(&mEventLog);
  }

  static constexpr uint16_t kCapacity = 10;
  struct ChppEventLog mEventLog;
};

TEST_F(EventLogTest, InitAndDeinit) {
  struct ChppEventLog eventLog;
  constexpr uint16_t capacity = 5;

  chppEventLogInit(&eventLog, capacity);

  EXPECT_NE(eventLog.events, nullptr);
  EXPECT_EQ(eventLog.capacity, capacity);
  EXPECT_EQ(eventLog.size, 0);
  EXPECT_EQ(eventLog.tail, 0);

  chppEventLogDeinit(&eventLog);

  EXPECT_EQ(eventLog.events, nullptr);
  EXPECT_EQ(eventLog.capacity, 0);
  EXPECT_EQ(eventLog.size, 0);
  EXPECT_EQ(eventLog.tail, 0);
}

TEST_F(EventLogTest, InitWithZeroCapacity) {
  struct ChppEventLog eventLog;
  chppEventLogInit(&eventLog, 0);

  EXPECT_EQ(eventLog.events, nullptr);
  EXPECT_EQ(eventLog.capacity, 0);
  EXPECT_EQ(eventLog.size, 0);
  EXPECT_EQ(eventLog.tail, 0);

  // Should not crash
  chppEventLogDeinit(&eventLog);
}

TEST_F(EventLogTest, LogEvent) {
  uint16_t eventType = 1;
  chppLogEvent(&mEventLog, eventType);

  EXPECT_EQ(mEventLog.size, 1);
  const struct ChppEvent *event = chppGetEventAtIndex(&mEventLog, 0);
  ASSERT_NE(event, nullptr);
  EXPECT_EQ(event->eventType, eventType);
  EXPECT_NE(event->timestampNs, 0);
}

TEST_F(EventLogTest, LogEventInt64) {
  uint16_t eventType = 2;
  int64_t value = -1234567890;
  chppLogEventInt64(&mEventLog, eventType, value);

  EXPECT_EQ(mEventLog.size, 1);
  const struct ChppEvent *event = chppGetEventAtIndex(&mEventLog, 0);
  ASSERT_NE(event, nullptr);
  EXPECT_EQ(event->eventType, eventType);
  EXPECT_EQ(event->int64_event.signed_int64, value);
  EXPECT_NE(event->timestampNs, 0);
}

TEST_F(EventLogTest, LogUntilFull) {
  for (uint16_t i = 0; i < kCapacity; i++) {
    chppLogEvent(&mEventLog, i);
    EXPECT_EQ(mEventLog.size, i + 1);
  }

  EXPECT_EQ(mEventLog.size, kCapacity);

  for (uint16_t i = 0; i < kCapacity; i++) {
    const struct ChppEvent *event = chppGetEventAtIndex(&mEventLog, i);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->eventType, i);
  }
}

TEST_F(EventLogTest, LogOverflow) {
  for (uint16_t i = 0; i < kCapacity; i++) {
    chppLogEvent(&mEventLog, i);
  }

  uint16_t overflowEventType = 99;
  EXPECT_EQ(mEventLog.size, kCapacity);
  chppLogEvent(&mEventLog, overflowEventType);
  EXPECT_EQ(mEventLog.size, kCapacity);

  // Oldest event is now 1
  const struct ChppEvent *oldestEvent = chppGetEventAtIndex(&mEventLog, 0);
  ASSERT_NE(oldestEvent, nullptr);
  EXPECT_EQ(oldestEvent->eventType, 1);

  // Newest event is 99
  const struct ChppEvent *newestEvent =
      chppGetEventAtIndex(&mEventLog, kCapacity - 1);
  ASSERT_NE(newestEvent, nullptr);
  EXPECT_EQ(newestEvent->eventType, overflowEventType);
}

TEST_F(EventLogTest, GetHead) {
  EXPECT_EQ(chppGetEventLogHead(&mEventLog), 0);

  chppLogEvent(&mEventLog, 1);
  chppLogEvent(&mEventLog, 2);
  EXPECT_EQ(chppGetEventLogHead(&mEventLog), 0);

  for (uint16_t i = 2; i < kCapacity; i++) {
    chppLogEvent(&mEventLog, i + 1);
  }
  EXPECT_EQ(mEventLog.size, kCapacity);
  EXPECT_EQ(chppGetEventLogHead(&mEventLog), 0);

  chppLogEvent(&mEventLog, 99);
  EXPECT_EQ(chppGetEventLogHead(&mEventLog), 1);

  chppLogEvent(&mEventLog, 100);
  EXPECT_EQ(chppGetEventLogHead(&mEventLog), 2);
}

TEST_F(EventLogTest, LogToNullLog) {
  struct ChppEventLog eventLog;
  chppEventLogInit(&eventLog, 0);

  // These should not crash
  chppLogEvent(&eventLog, 1);
  chppLogEventInt64(&eventLog, 2, 123);

  chppEventLogDeinit(&eventLog);
}

}  // namespace

}  // namespace chre
