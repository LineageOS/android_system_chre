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

#ifndef ANDROID_HARDWARE_CONTEXTHUB_COMMON_ERROR_UTIL_H
#define ANDROID_HARDWARE_CONTEXTHUB_COMMON_ERROR_UTIL_H

#include "aidl/android/hardware/contexthub/ErrorCode.h"

namespace android::hardware::contexthub::common::implementation {

//! Converts the AIDL ErrorCode to the CHRE error code.
uint8_t toChreErrorCode(
    aidl::android::hardware::contexthub::ErrorCode errorCode);

//! Converts the CHRE error code to the AIDL ErrorCode.
aidl::android::hardware::contexthub::ErrorCode toErrorCode(
    uint32_t chreErrorCode);

}  // namespace android::hardware::contexthub::common::implementation

#endif  // ANDROID_HARDWARE_CONTEXTHUB_COMMON_ERROR_UTIL_H
