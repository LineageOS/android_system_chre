/*
 * Copyright 2025 The Android Open Source Project
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

#include "chre_host/time_util.h"

#include <android-base/parseint.h>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

namespace android::chre {

std::string getWallclockTime(
    std::chrono::time_point<std::chrono::system_clock> time) {
  auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      time.time_since_epoch());

  constexpr int kBufferSize = 20;  // mm-dd HH:MM:SS.xxx
  char buffer[kBufferSize]{};
  time_t cTime = std::chrono::system_clock::to_time_t(time);
  std::strftime(buffer, kBufferSize, "%m-%d %H:%M:%S.", std::localtime(&cTime));
  // The offset 15 is right after the `.` printed by strftime(). The size 4 is
  // the 3 digits of the durationMs followed by a null terminator.
  std::snprintf(buffer + 15, /* size= */ 4, "%03" PRIu16,
                static_cast<uint16_t>(durationMs.count() % 1000));
  return {buffer};
}

std::string realtimeNsToWallclockTime(
    uint64_t realtime, std::chrono::time_point<std::chrono::system_clock> now,
    uint64_t nowRealtime) {
  if (nowRealtime < realtime) {
    return "<Error - Could not compute wallclock time>";
  }
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::nanoseconds{nowRealtime - realtime});
  auto hostTime = now - diff;
  return getWallclockTime(hostTime);
}

// Constants for time conversion
const uint64_t NANOS_IN_SECOND = 1'000'000'000;
const uint64_t NANOS_IN_MILLI = 1'000'000;
const uint64_t NANOS_IN_MICRO = 1'000;

std::string formatNanos(uint64_t nanos) {
  std::stringstream ss;

  // Calculate each component
  uint64_t seconds = nanos / NANOS_IN_SECOND;
  uint64_t remaining_nanos = nanos % NANOS_IN_SECOND;
  uint64_t milliseconds = remaining_nanos / NANOS_IN_MILLI;
  uint64_t microseconds = (remaining_nanos % NANOS_IN_MILLI) / NANOS_IN_MICRO;
  uint64_t nanoseconds_part = remaining_nanos % NANOS_IN_MICRO;

  // Stream the parts with proper formatting
  ss << seconds << '.' << std::setw(3) << std::setfill('0') << milliseconds
     << ' ' << std::setw(3) << std::setfill('0') << microseconds << ' '
     << std::setw(3) << std::setfill('0') << nanoseconds_part;

  return ss.str();
}

std::string appendWalltimeToTimestamp(const std::string &str,
                                      std::optional<int64_t> hostOffset) {
  // Regex to find timestamp keys ('ts=', 'time=', 'time(ns)=') followed by
  // digits. The group (ms)? optionally captures the millisecond unit.
  // The capture groups:
  //   - Group 1: The timestamp key (e.g., "ts=", "time=", "time(ns)=")
  //   - Group 2: The timestamp value (digits)
  //   - Group 3: Optional "ms" unit
  std::regex ts_regex(R"(\b(ts=|time=|time\(ns\)=)(\d+)(ms)?)");
  auto it = std::sregex_iterator(str.begin(), str.end(), ts_regex);
  auto end = std::sregex_iterator();

  if (it == end) {
    return str;  // No matches, return original string
  }

  std::ostringstream ss;
  size_t last_pos = 0;
  for (; it != end; ++it) {
    std::smatch match = *it;
    if (match.size() < 3) {
      continue;
    }
    ss << str.substr(last_pos, match.position() - last_pos);

    uint64_t ts_val = 0;
    bool success = android::base::ParseUint(match[2].str(), &ts_val);
    if (!success) {
      continue;
    }

    uint64_t ts_val_ns = 0;

    // Check if the 3rd capture group ("ms") matched
    if (match.size() >= 4 && match[3].matched) {
      // Case: Milliseconds (e.g., ts=123ms)
      // Output exactly as is, do not use formatNanos
      ss << match[1].str() << ts_val << "ms";
      ts_val_ns = ts_val * 1000000;  // Convert to ns for walltime calc
    } else {
      // Case: Nanoseconds (e.g., ts=123456)
      // Format for readability using existing logic
      ss << "ts=" << formatNanos(ts_val);
      ts_val_ns = ts_val;
    }

    if (hostOffset.has_value()) {
      ts_val_ns += hostOffset.value();
      ss << " [" << realtimeNsToWallclockTime(ts_val_ns) << "]";
    }
    last_pos = match.position() + match.length();

    if (last_pos + 1 < str.size() && str.substr(last_pos, 2) == "ns") {
      last_pos += 2;
    }
  }
  // Append the remainder of the string after the last match
  if (last_pos < str.size()) {
    ss << str.substr(last_pos);
  }

  return ss.str();
}

}  // namespace android::chre
