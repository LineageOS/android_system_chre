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

#include "utils.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "chre_api/chre/version.h"
namespace android::chre::chre_aidl_hal_client {

bool isValidHexNumber(const std::string &number) {
  if (number.empty() ||
      (number.substr(0, 2) != "0x" && number.substr(0, 2) != "0X")) {
    return false;
  }
  for (int i = 2; i < number.size(); i++) {
    if (!isxdigit(number[i])) {
      throwError("Hex app id " + number + " contains invalid character.");
    }
  }
  return number.size() > 2;
}

char16_t verifyAndConvertEndpointHexId(const std::string &number) {
  // host endpoint id must be a 16-bits long hex number.
  if (isValidHexNumber(number)) {
    const char16_t convertedNumber =
        std::stoi(number, /* idx= */ nullptr, /* base= */ 16);
    if (convertedNumber < std::numeric_limits<uint16_t>::max()) {
      return convertedNumber;
    }
  }
  throwError("host endpoint id must be a 16-bits long hex number.");
  return 0;  // code never reached.
}
}  // namespace android::chre::chre_aidl_hal_client