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

#ifndef ANDROID_CHRE_AIDL_HAL_CLIENT_UTILS_H
#define ANDROID_CHRE_AIDL_HAL_CLIENT_UTILS_H

#include <string>
#include <system_error>

namespace android::chre::chre_aidl_hal_client {

/**
 * Throws a std::system_error with the provided message.
 *
 * @param message The error message to include in the exception.
 */
inline void throwError(const std::string &message) {
  throw std::system_error{std::error_code(), message};
}

/**
 * Checks if a string represents a valid hexadecimal number.
 *
 * A valid hex number must start with "0x" or "0X" and be followed by one or
 * more hexadecimal digits (0-9, a-f, A-F).
 * Throws an error if an invalid character is found after the prefix.
 *
 * @param number The string to validate.
 * @return true if the string is a valid non-empty hex number, false otherwise.
 */
bool isValidHexNumber(const std::string &number);

/**
 * Verifies if a string represents a valid 16-bit hexadecimal number and
 * converts it.
 *
 * Throws an error if the input string is not a valid hex number or if the
 * converted value is outside the range of a 16-bit unsigned integer.
 *
 * @param number The string containing the hex number (e.g., "0x1234").
 * @return The converted char16_t value.
 * @throws std::system_error if the input is invalid.
 */
char16_t verifyAndConvertEndpointHexId(const std::string &number);
}  // namespace android::chre::chre_aidl_hal_client

#endif  // ANDROID_CHRE_AIDL_HAL_CLIENT_UTILS_H