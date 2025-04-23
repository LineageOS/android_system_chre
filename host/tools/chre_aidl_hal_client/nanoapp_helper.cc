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

#include "nanoapp_helper.h"
#include "utils.h"

#include <dirent.h>
#include <fstream>
#include <future>
#include <iostream>
#include <regex>
#include <string>

#include "chre_api/chre/version.h"

namespace android::chre::chre_aidl_hal_client {

namespace {
// Locations should be searched in the sequence defined below:
const char *kPredefinedNanoappPaths[] = {
    "/vendor/etc/chre/",
    "/vendor/dsp/adsp/",
    "/vendor/dsp/sdsp/",
    "/vendor/lib/rfsa/adsp/",
};
}  // namespace

std::string NanoappHelper::parseAppVersion(uint32_t version) {
  std::ostringstream stringStream;
  stringStream << std::hex << "0x" << version << std::dec << " (v"
               << CHRE_EXTRACT_MAJOR_VERSION(version) << "."
               << CHRE_EXTRACT_MINOR_VERSION(version) << "."
               << CHRE_EXTRACT_PATCH_VERSION(version) << ")";
  return stringStream.str();
}

bool NanoappHelper::isValidNanoappHexId(const std::string &number) {
  if (!isValidHexNumber(number)) {
    return false;
  }
  // Once the input has the hex prefix, an exception will be thrown if it is
  // malformed because it shouldn't be treated as an app name anymore.
  if (number.size() > 18) {
    throwError(
        "Hex app id must have a length of [3, 18] including the prefix.");
  }
  return true;
}

void NanoappHelper::printNanoappHeader(const NanoAppBinaryHeader &header) {
  std::cout << " {"
            << "\n\tappId: 0x" << std::hex << header.appId << std::dec
            << "\n\tappVersion: "
            << NanoappHelper::parseAppVersion(header.appVersion)
            << "\n\tflags: " << header.flags << "\n\ttarget CHRE API version: "
            << static_cast<int>(header.targetChreApiMajorVersion) << "."
            << static_cast<int>(header.targetChreApiMinorVersion) << "\n}"
            << std::endl;
}

std::unique_ptr<NanoAppBinaryHeader> NanoappHelper::findHeaderByName(
    const std::string &appName, const std::string &binaryPath) {
  DIR *dir = opendir(binaryPath.c_str());
  if (dir == nullptr) {
    return nullptr;
  }
  std::regex regex(appName + ".napp_header");
  std::cmatch match;

  std::unique_ptr<NanoAppBinaryHeader> result = nullptr;
  for (dirent *entry; (entry = readdir(dir)) != nullptr;) {
    if (!std::regex_match(entry->d_name, match, regex)) {
      continue;
    }
    std::ifstream input(std::string(binaryPath) + "/" + entry->d_name,
                        std::ios::binary);
    result = std::make_unique<NanoAppBinaryHeader>();
    input.read(reinterpret_cast<char *>(result.get()),
               sizeof(NanoAppBinaryHeader));
    break;
  }
  closedir(dir);
  return result;
}

void NanoappHelper::readNanoappHeaders(
    std::map<std::string, NanoAppBinaryHeader> &nanoapps,
    const std::string &binaryPath) {
  DIR *dir = opendir(binaryPath.c_str());
  if (dir == nullptr) {
    return;
  }
  std::regex regex("(\\w+)\\.napp_header");
  std::cmatch match;
  for (struct dirent *entry; (entry = readdir(dir)) != nullptr;) {
    if (!std::regex_match(entry->d_name, match, regex)) {
      continue;
    }
    std::ifstream input(std::string(binaryPath) + "/" + entry->d_name,
                        std::ios::binary);
    input.read(reinterpret_cast<char *>(&nanoapps[match[1]]),
               sizeof(NanoAppBinaryHeader));
  }
  closedir(dir);
}

/**
 * Finds the .napp_header file associated to the nanoapp.
 *
 * This function guarantees to return a non-null {@link NanoAppBinaryHeader}
 * pointer. In case a .napp_header file cannot be found an exception will be
 * raised.
 *
 * @param pathAndName name of the nanoapp that might be prefixed with it path.
 * It will be normalized to the format of <absolute-path><name>.so at the end.
 * For example, "abc" will be changed to "/path/to/abc.so".
 *
 * @return a unique pointer to the {@link NanoAppBinaryHeader} found
 */
std::unique_ptr<NanoAppBinaryHeader> NanoappHelper::findHeaderAndNormalizePath(
    std::string &pathAndName) {
  // To match the file pattern of [path]<name>[.so]
  std::regex pathNameRegex("(.*?)(\\w+)(\\.so)?");
  std::smatch smatch;
  if (!std::regex_match(pathAndName, smatch, pathNameRegex)) {
    throwError("Invalid nanoapp: " + pathAndName);
  }
  std::string fullPath = smatch[1];
  std::string appName = smatch[2];
  // absolute path is provided:
  if (!fullPath.empty() && fullPath[0] == '/') {
    auto result = findHeaderByName(appName, fullPath);
    if (result == nullptr) {
      throwError("Unable to find the nanoapp header for " + pathAndName);
    }
    pathAndName = fullPath + appName + ".so";
    return result;
  }
  // relative path is searched form predefined locations:
  for (const std::string &predefinedPath : kPredefinedNanoappPaths) {
    auto result = findHeaderByName(appName, predefinedPath);
    if (result == nullptr) {
      continue;
    }
    pathAndName = predefinedPath + appName + ".so";
    return result;
  }
  throwError("Unable to find the nanoapp header for " + pathAndName);
  return nullptr;
}

int64_t NanoappHelper::getNanoappIdFrom(std::string &appIdOrName) {
  int64_t appId;
  if (NanoappHelper::isValidNanoappHexId(appIdOrName)) {
    appId = std::stoll(appIdOrName, nullptr, 16);
  } else {
    // Treat the appIdOrName as the app name and try again
    appId =
        static_cast<int64_t>(findHeaderAndNormalizePath(appIdOrName)->appId);
  }
  return appId;
}

void NanoappHelper::listNanoappsInPath(const std::string &path) {
  std::map<std::string, NanoAppBinaryHeader> nanoapps{};
  NanoappHelper::readNanoappHeaders(nanoapps, path);
  if (nanoapps.empty()) {
    std::cout << "No nanoapp headers found in " << path << std::endl;
    return;
  }
  std::cout << "Nanoapps found in " << path << ":" << std::endl;
  for (const auto &[appName, appHeader] : nanoapps) {
    std::cout << appName;
    NanoappHelper::printNanoappHeader(appHeader);
  }
}

}  // namespace android::chre::chre_aidl_hal_client