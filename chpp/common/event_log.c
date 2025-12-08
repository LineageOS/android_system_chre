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

#include "chpp/common/event_log.h"
#include "chpp/log.h"
#include "chpp/macros.h"
#include "chpp/memory.h"
#include "chpp/time.h"

void chppEventLogInit(struct ChppEventLog *eventLog, uint16_t capacity) {
  eventLog->capacity = capacity;
  eventLog->size = 0;
  eventLog->tail = 0;
  if (eventLog->capacity != 0) {
    eventLog->events = chppMalloc(sizeof(struct ChppEvent) * capacity);
    if (eventLog->events == NULL) {
      CHPP_LOGE("Failed to allocate event log, capacity: %d", capacity);
      eventLog->capacity = 0;
    }
  } else {
    eventLog->events = NULL;
  }
}

void chppEventLogDeinit(struct ChppEventLog *eventLog) {
  if (eventLog->capacity != 0) {
    CHPP_DEBUG_NOT_NULL(eventLog->events);
    CHPP_FREE_AND_NULLIFY(eventLog->events);
  }
  eventLog->capacity = 0;
  eventLog->size = 0;
  eventLog->tail = 0;
}

void chppLogEventInt64(struct ChppEventLog *eventLog, uint16_t eventType,
                       int64_t signed_int64) {
  if (eventLog->capacity == 0) {
    return;
  }
  eventLog->events[eventLog->tail].int64_event.signed_int64 = signed_int64;
  chppLogEvent(eventLog, eventType);
}

void chppLogEvent(struct ChppEventLog *eventLog, uint16_t eventType) {
  if (eventLog->capacity == 0) {
    return;
  }
  eventLog->events[eventLog->tail].timestampNs = chppGetCurrentTimeNs();
  eventLog->events[eventLog->tail].eventType = eventType;
  if (eventLog->size != eventLog->capacity) {
    eventLog->size++;
  }
  eventLog->tail = (eventLog->tail + 1) % eventLog->capacity;
}

uint16_t chppGetEventLogHead(const struct ChppEventLog *eventLog) {
  // Keep the offset relative to the capacity so that we do not hit any
  // underflow cases with the unsigned subtraction.
  uint16_t offset = eventLog->capacity - eventLog->size;
  return (eventLog->tail + offset) % eventLog->capacity;
}

const struct ChppEvent *chppGetEventAtIndex(const struct ChppEventLog *eventLog,
                                            uint16_t index) {
  if (index >= eventLog->size) {
    CHPP_LOGE(
        "Attempting to get event at index %d, but only %d events in the log.",
        index, eventLog->size);
    return NULL;
  }

  uint16_t next = (chppGetEventLogHead(eventLog) + index) % eventLog->capacity;
  return &eventLog->events[next];
}
