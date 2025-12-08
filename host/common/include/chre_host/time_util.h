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

#pragma once

#include <chrono>
#include <string>

#include <utils/SystemClock.h>

namespace android::chre {

/**
 * Converts a CHRE timestamp to one comparable with elapsedRealtimeNano().
 *
 * @param chreTime CHRE timestamp in ns
 * @param estimatedHostOffset Estimated offset from host time in ns
 * @return Estimated host timestamp
 */
constexpr uint64_t estimatedHostRealtimeNs(uint64_t chreTime,
                                           uint64_t estimatedHostOffset) {
  return chreTime + estimatedHostOffset;
}

/**
 * Generates a nice representation of the given system time.
 *
 * @param time The time to stringify, default is now
 * @return time formatted as mm-dd HH:MM:SS.xxx
 */
std::string getWallclockTime(std::chrono::time_point<std::chrono::system_clock>
                                 time = std::chrono::system_clock::now());

/**
 * Converts elapsedRealtimeNano() to wallclock time and formats it.
 *
 * @param realtime Output of elapsedRealtimeNano() or comparable timestamp
 * @param now Reference point for converting realtime. Optionally allows passing
 * in the current time.
 * @param nowRealtime Used to compute the duration since realtime. Optionally
 * allows passing in the current time.
 * @param realtime formatted as mm-dd HH:MM:SS.xxx
 */
std::string realtimeNsToWallclockTime(
    uint64_t realtime,
    std::chrono::time_point<std::chrono::system_clock> now =
        std::chrono::system_clock::now(),
    uint64_t nowRealtime = elapsedRealtimeNano());

/**
 * Format nanoseconds with spaces for readability.
 *
 * @param nanos CHRE Timestamp in ns.
 */
std::string formatNanos(uint64_t nanos);

}  // namespace android::chre
