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

#include "error_util.h"

#include "chre_api/chre.h"

using ::aidl::android::hardware::contexthub::ErrorCode;

namespace android::hardware::contexthub::common::implementation {

uint8_t toChreErrorCode(ErrorCode errorCode) {
  switch (errorCode) {
    case ErrorCode::OK:
      return CHRE_ERROR_NONE;
    case ErrorCode::TRANSIENT_ERROR:
      return CHRE_ERROR_TRANSIENT;
    case ErrorCode::PERMANENT_ERROR:
      return CHRE_ERROR;
    case ErrorCode::PERMISSION_DENIED:
      return CHRE_ERROR_PERMISSION_DENIED;
    case ErrorCode::DESTINATION_NOT_FOUND:
      return CHRE_ERROR_DESTINATION_NOT_FOUND;
  }

  return CHRE_ERROR;
}

ErrorCode toErrorCode(uint32_t chreErrorCode) {
  switch (chreErrorCode) {
    case CHRE_ERROR_NONE:
      return ErrorCode::OK;
    case CHRE_ERROR_BUSY:  // fallthrough
    case CHRE_ERROR_TRANSIENT:
      return ErrorCode::TRANSIENT_ERROR;
    case CHRE_ERROR:
      return ErrorCode::PERMANENT_ERROR;
    case CHRE_ERROR_PERMISSION_DENIED:
      return ErrorCode::PERMISSION_DENIED;
    case CHRE_ERROR_DESTINATION_NOT_FOUND:
      return ErrorCode::DESTINATION_NOT_FOUND;
  }

  return aidl::android::hardware::contexthub::ErrorCode::PERMANENT_ERROR;
}

}  // namespace android::hardware::contexthub::common::implementation
