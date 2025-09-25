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

#ifndef CHRE_PLATFORM_ANDROID_PLATFORM_LOG_H_
#define CHRE_PLATFORM_ANDROID_PLATFORM_LOG_H_

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "chre/util/singleton.h"
#include "chre_api/chre/re.h"

namespace chre {

/**
 * The CHRE AP implementation of the PlatformLog class.
 */
class PlatformLog {
 public:
  PlatformLog() = default;
  ~PlatformLog() = default;

  /**
   * Logs message with printf-style arguments. No trailing newline is required
   * for this method.
   */
  static void log(chreLogLevel logLevel, const char *formatStr, ...) {
    va_list args;
    va_start(args, formatStr);
    logVa(logLevel, formatStr, args);
    va_end(args);
  }

  /**
   * Logs message with printf-style arguments. No trailing newline is required
   * for this method. Uses va_list parameter instead of ...
   */
  static void logVa(chreLogLevel logLevel, const char *formatStr, va_list args);
};
}  // namespace chre

#endif  // CHRE_PLATFORM_ANDROID_PLATFORM_LOG_H_