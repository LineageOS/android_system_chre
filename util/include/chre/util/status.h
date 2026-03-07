/*
 * Copyright (C) 2026 The Android Open Source Project
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

#ifndef CHRE_UTIL_STATUS_H_
#define CHRE_UTIL_STATUS_H_

#include <cstdint>

#include "chre_api/chre.h"
#include "pw_status/status.h"

namespace chre {

/**
 * Converts a pw::Status to a chreStatus.
 *
 * @param status The pw::Status to convert.
 * @return The corresponding chreStatus.
 */
inline uint32_t toChreStatus(pw::Status status) {
  switch (status.code()) {
    case PW_STATUS_OK:
      return CHRE_STATUS_OK;
    case PW_STATUS_CANCELLED:
      return CHRE_STATUS_CANCELLED;
    case PW_STATUS_UNKNOWN:
      return CHRE_STATUS_UNKNOWN;
    case PW_STATUS_INVALID_ARGUMENT:
      return CHRE_STATUS_INVALID_ARGUMENT;
    case PW_STATUS_DEADLINE_EXCEEDED:
      return CHRE_STATUS_DEADLINE_EXCEEDED;
    case PW_STATUS_NOT_FOUND:
      return CHRE_STATUS_NOT_FOUND;
    case PW_STATUS_ALREADY_EXISTS:
      return CHRE_STATUS_ALREADY_EXISTS;
    case PW_STATUS_PERMISSION_DENIED:
      return CHRE_STATUS_PERMISSION_DENIED;
    case PW_STATUS_RESOURCE_EXHAUSTED:
      return CHRE_STATUS_RESOURCE_EXHAUSTED;
    case PW_STATUS_FAILED_PRECONDITION:
      return CHRE_STATUS_FAILED_PRECONDITION;
    case PW_STATUS_ABORTED:
      return CHRE_STATUS_ABORTED;
    case PW_STATUS_OUT_OF_RANGE:
      return CHRE_STATUS_OUT_OF_RANGE;
    case PW_STATUS_UNIMPLEMENTED:
      return CHRE_STATUS_UNIMPLEMENTED;
    case PW_STATUS_INTERNAL:
      return CHRE_STATUS_INTERNAL;
    case PW_STATUS_UNAVAILABLE:
      return CHRE_STATUS_UNAVAILABLE;
    case PW_STATUS_DATA_LOSS:
      return CHRE_STATUS_DATA_LOSS;
    case PW_STATUS_UNAUTHENTICATED:
      return CHRE_STATUS_UNAUTHENTICATED;
    default:
      return CHRE_STATUS_UNKNOWN;
  }
}

}  // namespace chre

#endif  // CHRE_UTIL_STATUS_H_
