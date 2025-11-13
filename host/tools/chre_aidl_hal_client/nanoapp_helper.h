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

#ifndef ANDROID_CHRE_AIDL_HAL_CLIENT_NANOAPP_HELPER_H
#define ANDROID_CHRE_AIDL_HAL_CLIENT_NANOAPP_HELPER_H

#include <fstream>
#include <map>
#include <string>

#include "chre_host/napp_header.h"

namespace android::chre::chre_aidl_hal_client {

/**
 * Provides static utility functions for handling nanoapps.
 *
 * This class offers functionalities like parsing versions, validating IDs,
 * finding and reading nanoapp headers, and resolving nanoapp IDs from names or
 * hex strings.
 */
class NanoappHelper {
 public:
  /**
   * Parses a raw nanoapp version number into a human-readable string.
   *
   * Formats the version as "0x<hex_version> (v<major>.<minor>.<patch>)".
   *
   * @param version The raw 32-bit version number.
   * @return A string representation of the version.
   */
  static std::string parseAppVersion(uint32_t version);

  /**
   * Checks if a string represents a valid 64-bit hexadecimal nanoapp ID.
   *
   * A valid hex ID must start with "0x" or "0X", be followed by 1 to 16
   * hexadecimal digits (0-9, a-f, A-F), resulting in a total length between 3
   * and 18 characters. Throws an error if the format is invalid after the
   * prefix or if the length constraint is violated.
   *
   * @param number The string to validate.
   * @return true if the string is a valid hex nanoapp ID format, false
   * otherwise.
   * @throws std::system_error if the format is invalid after the prefix or if
   * the length constraint is violated.
   */
  static bool isValidNanoappHexId(const std::string &number);

  /**
   * Prints the details of a NanoAppBinaryHeader to standard output.
   *
   * @param header The nanoapp header structure to print.
   */
  static void printNanoappHeader(const NanoAppBinaryHeader &header);

  /**
   * Finds and reads a nanoapp header file by name within a specific directory.
   *
   * Searches for a file named "<appName>.napp_header" in the given
   * `binaryPath`.
   *
   * @param appName The base name of the nanoapp (without path or extension).
   * @param binaryPath The directory path to search within.
   * @return A unique pointer to the read NanoAppBinaryHeader if found,
   * otherwise nullptr.
   */
  static std::unique_ptr<NanoAppBinaryHeader> findHeaderByName(
      const std::string &appName, const std::string &binaryPath);

  /**
   * Reads all nanoapp header files from a specified directory.
   *
   * Scans the directory for files matching "*.napp_header", reads them, and
   * populates the provided map. The map keys will be the nanoapp names
   * extracted from the filenames.
   *
   * @param nanoapps A map to populate with nanoapp names and their
   * corresponding headers.
   * @param binaryPath The directory path to scan for header files.
   */
  static void readNanoappHeaders(
      std::map<std::string, NanoAppBinaryHeader> &nanoapps,
      const std::string &binaryPath);

  /**
   * Finds the .napp_header file associated with a nanoapp and normalizes its
   * path.
   *
   * Parses the input `pathAndName` to extract the path and name. If an
   * absolute path is given, it searches there. Otherwise, it searches
   * predefined system paths. If found, it updates `pathAndName` to the full,
   * normalized path (e.g., "/path/to/app.so") and returns the header.
   *
   * This function guarantees to return a non-null {@link NanoAppBinaryHeader}
   * pointer if successful.
   *
   * @param pathAndName Input string potentially containing path and name (e.g.,
   * "my_app", "/vendor/etc/chre/my_app.so"). This string reference will be
   * modified to the normalized absolute path ending in ".so" if the header is
   * found.
   * @return A unique pointer to the {@link NanoAppBinaryHeader} found.
   * @throws std::system_error if the input format is invalid or the header
   * file cannot be found.
   */
  static std::unique_ptr<NanoAppBinaryHeader> findHeaderAndNormalizePath(
      std::string &pathAndName);

  /**
   * Gets the 64-bit nanoapp ID from a string, which can be a hex ID or a
   * name/path.
   *
   * If the input string is identified as a valid hex nanoapp ID (using
   * `isValidNanoappHexId`), it's converted directly. Otherwise, the string is
   * treated as a nanoapp name (potentially with a path), and its header is
   * located using `findHeaderAndNormalizePath` to retrieve the ID. The input
   * string `appIdOrName` might be modified by
   * `findHeaderAndNormalizePath` if it's treated as a name.
   *
   * @param appIdOrName A string containing either the hex ID ("0x...") or the
   * nanoapp name/path. This string reference might be modified.
   * @return The 64-bit nanoapp ID.
   * @throws std::system_error if the input is neither a valid hex ID nor a
   * resolvable nanoapp name.
   */
  static int64_t getNanoappIdFrom(std::string &appIdOrName);

  /**
   * Reads all nanoapp headers from the specified path and prints their details.
   *
   * Scans the given directory for files ending in ".napp_header", reads each
   * header, and prints the extracted nanoapp name along with its header
   * information (using `printNanoappHeader`) to standard output. If no headers
   * are found, it prints a message indicating that.
   *
   * @param path The directory path to scan for nanoapp header files.
   */
  static void listNanoappsInPath(const std::string &path);
};
}  // namespace android::chre::chre_aidl_hal_client

#endif  // ANDROID_CHRE_AIDL_HAL_CLIENT_NANOAPP_HELPER_H