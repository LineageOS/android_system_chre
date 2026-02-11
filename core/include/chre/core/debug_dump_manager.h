/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef CHRE_CORE_DEBUG_DUMP_MANAGER_H_
#define CHRE_CORE_DEBUG_DUMP_MANAGER_H_

#include <cstdarg>
#include <cstdint>

#include "chre/core/multi_threading_api_mutex.h"
#include "chre/core/nanoapp.h"
#include "chre/platform/atomic.h"
#include "chre/platform/platform_debug_dump_manager.h"
#include "chre/util/optional.h"
#include "chre/util/system/debug_dump.h"
#include "chre/util/thread_annotations.h"

namespace chre {

// An externally accessible debug dump string used for testing.
inline constexpr char kEventLoopDebugDumpFormatString[] =
    "\n--- Event Loop %" PRIu32 " Debug Dump ---\n";

/**
 * A helper class that manages the CHRE framework and nanoapp debug dump
 * process.
 */
class DebugDumpManager : public PlatformDebugDumpManager {
 public:
  /**
   * Triggers the CHRE framework and nanoapp debug dump process.
   */
  void trigger();

  /**
   * Helper function to log nanoapp debug dump.
   */
  void appendNanoappLog(const Nanoapp &nanoapp, const char *formatStr,
                        va_list args);

  /**
   * Starts the full debug dump collection process.
   *
   * This function initiates the collection of debug dumps from the CHRE
   * framework and then proceeds to collect debug dumps from each event loop
   * and the nanoapps running on them.
   *
   * This function must be called from the main event loop.
   */
  void startFullDebugDumpCollection()
      CHRE_REQUIRES(getMultiThreadingApiMutex());

  /**
   * Performs a debug dump for the event loop/nanoapps on this thread.
   */
  void handleEventLoopAndNanoappDebugDump()
      CHRE_REQUIRES(getMultiThreadingApiMutex());

 private:
  //! Utility to hold the framework and nanoapp debug dumps.
  DebugDumpWrapper mDebugDump{kDebugDumpStrMaxSize};

  uint32_t mCurrentDebugDumpEventLoopIndex = 0;
  bool mCollectingDebugDumps = false;

  //! Instance ID of the nanoapp that was last logging debug dumps in this
  //! session.
  Optional<uint32_t> mLastNanoappId;

  /**
   * Append CHRE capabilities to the debug dump.
   */
  void appendCapabilities();

  /**
   * Collect CHRE framework debug dumps.
   *
   * Should only be called from the main CHRE event loop.
   */
  void collectFrameworkDebugDumps();

  /**
   * Send collected debug dumps to the host.
   *
   * Can be called from any event loop.
   */
  void sendDebugDumps();
};

}  // namespace chre

#endif  // CHRE_CORE_DEBUG_DUMP_MANAGER_H_
