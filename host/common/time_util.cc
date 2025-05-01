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

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <ctime>
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

}  // namespace android::chre
