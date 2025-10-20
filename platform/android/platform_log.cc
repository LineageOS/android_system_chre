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
#define LOG_TAG "CHRE_AP"

#include "chre/platform/android/platform_log.h"

#include <cstdarg>
#include <cstdio>
#include <iostream>

#include "chre/platform/fatal_error.h"
#include "chre/platform/shared/bt_snoop_log.h"

#include <android/log_macros.h>

void chrePlatformBtSnoopLog(BtSnoopDirection /*direction*/,
                            const uint8_t * /*buffer*/, size_t /*size*/) {
  // Unimplemented in this platform
}

namespace chre {

void PlatformLog::logVa(chreLogLevel logLevel, const char *formatStr,
                        va_list args) {
  char buffer[512];
  int result = vsnprintf(buffer, sizeof(buffer), formatStr, args);
  if (result < 0) {
    return;
  }

#ifdef CHRE_AP_ALSO_LOG_TO_STDERR
  std::cerr << buffer << std::endl;
#endif

  switch (logLevel) {
    case CHRE_LOG_DEBUG:
      ALOGD("%s", buffer);
      break;
    case CHRE_LOG_INFO:
      ALOGI("%s", buffer);
      break;
    case CHRE_LOG_WARN:
      ALOGW("%s", buffer);
      break;
    case CHRE_LOG_ERROR:
      ALOGE("%s", buffer);
      break;
    default:
      FATAL_ERROR("Unexpected log level");
  }
}

}  // namespace chre
