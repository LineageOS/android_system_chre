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

#include "chre/platform/shared/pal_system_api.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "chre/pal/system.h"
#include "chre_api/chre.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"
#include "platform/linux/include/chre/target_platform/log.h"

#define PAL_LOG_FORMAT_STR "PAL: %s"

namespace chre {

uint64_t palSystemApiGetCurrentTime() {
  return lbs::contexthub::testing::Simulator::GetInstance()->GetCurrentTime();
}

void palSystemApiLog(enum chreLogLevel level, const char *formatStr, ...) {
  char logBuf[512];
  va_list args;

  va_start(args, formatStr);
  vsnprintf(logBuf, sizeof(logBuf), formatStr, args);
  va_end(args);

  switch (level) {
    case CHRE_LOG_ERROR:
      LOGE(PAL_LOG_FORMAT_STR, logBuf);
      break;
    case CHRE_LOG_WARN:
      LOGW(PAL_LOG_FORMAT_STR, logBuf);
      break;
    case CHRE_LOG_INFO:
      LOGI(PAL_LOG_FORMAT_STR, logBuf);
      break;
    case CHRE_LOG_DEBUG:
    default:
      LOGD(PAL_LOG_FORMAT_STR, logBuf);
  }
}

// Initialize the CHRE System API with function implementations provided above.
const chrePalSystemApi gChrePalSystemApi = {
    CHRE_PAL_SYSTEM_API_V1_0,   /* version */
    palSystemApiGetCurrentTime, /* getCurrentTime */
    palSystemApiLog,            /* log */
    malloc,                     /* memoryAlloc */
    free,                       /* memoryFree */
};

}  // namespace chre