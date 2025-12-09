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

#ifndef CHPP_EVENT_LOG_H_
#define CHPP_EVENT_LOG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ChppEvent {
  uint64_t timestampNs;
  uint16_t eventType;
  // Defines the payloads required by various events
  union {
    struct {
      int64_t signed_int64;
    } int64_event;
  };
};

struct ChppEventLog {
  uint16_t capacity;
  uint16_t size;
  uint16_t tail;
  struct ChppEvent *events;
};

void chppEventLogInit(struct ChppEventLog *eventLog, uint16_t capacity);

void chppEventLogDeinit(struct ChppEventLog *eventLog);

void chppLogEventInt64(struct ChppEventLog *eventLog, uint16_t eventType,
                       int64_t signed_int64);

void chppLogEvent(struct ChppEventLog *eventLog, uint16_t eventType);

uint16_t chppGetEventLogHead(const struct ChppEventLog *eventLog);

// Returns a pointer to the event at the given virtual index of the underlying
// circular buffer. This should be the preferred way to read the event log to
// ensure that the offset math is correct.
//
// Index must be between 0 and eventLog->size - 1, inclusive. Otherwise, this
// function will log an error and return NULL.
const struct ChppEvent *chppGetEventAtIndex(const struct ChppEventLog *eventLog,
                                            uint16_t index);

#ifdef __cplusplus
}
#endif

#endif  // CHPP_EVENT_LOG_H_
