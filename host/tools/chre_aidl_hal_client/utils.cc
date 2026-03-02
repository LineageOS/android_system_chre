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
#include <cstdlib>
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
    char *end;
    unsigned long convertedNumber =
        std::strtoul(number.c_str(), &end, /* base= */ 16);
    if (*end == '\0' &&
        convertedNumber <= std::numeric_limits<uint16_t>::max()) {
      return static_cast<char16_t>(convertedNumber);
    }
  }
  throwError("host endpoint id must be a 16-bits long hex number.");
  return 0;  // code never reached.
}

std::array<uint8_t, 16> parseUuid(const std::string &hex) {
  std::string cleanHex = hex;
  if (hex.substr(0, 2) == "0x" || hex.substr(0, 2) == "0X") {
    cleanHex = hex.substr(2);
  }
  if (cleanHex.length() != 32) {
    throwError("UUID must be 32 hex characters long.");
  }
  std::array<uint8_t, 16> uuid;
  for (size_t i = 0; i < 16; ++i) {
    std::string byteStr = cleanHex.substr(i * 2, 2);
    char *end;
    unsigned long byteVal = std::strtoul(byteStr.c_str(), &end, /* base= */ 16);
    if (*end != '\0' || byteVal > 255) {
      throwError("Invalid UUID hex string.");
    }
    uuid[i] = static_cast<uint8_t>(byteVal);
  }
  return uuid;
}
}  // namespace android::chre::chre_aidl_hal_client
