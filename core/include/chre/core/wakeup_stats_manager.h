/*
 * Copyright (C) 2026 The Android Open Source Project
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

#ifndef CHRE_CORE_WAKEUP_STATS_MANAGER_H_
#define CHRE_CORE_WAKEUP_STATS_MANAGER_H_

#include "chre/core/nanoapp.h"
#include "chre/platform/atomic.h"
#include "chre/util/non_copyable.h"

namespace chre {

/**
 * Categorization of host wakeup reasons.
 *
 * These values correspond to the messages in the ChreMessage union defined in
 * system/chre/platform/shared/idl/host_messages.fbs.
 */
enum class WakeupReason : uint8_t {
  NANOAPP_MESSAGE,
  METRIC_LOG,
};

/**
 * Central manager for host wakeup attribution and framework-level statistics.
 */
class WakeupStatsManager : public NonCopyable {
 public:
  /**
   * Records a host wakeup and attributes it to a nanoapp or the framework.
   *
   * @param nanoapp Pointer to the nanoapp that triggered the wakeup, or nullptr
   *                if triggered by the framework.
   * @param reason The reason for the host wakeup.
   */
  void blameWakeup(Nanoapp *nanoapp, WakeupReason reason);

  /**
   * Resets the host wakeup blame latch. This should be called when the host
   * is known to be suspended.
   */
  void resetBlameForHostWakeup();

  /**
   * @return true if the host wakeup has already been blamed in the current
   *         cycle.
   */
  bool isHostWakeupBlamed() const {
    return mHostWakeupBlamed;
  }

 private:
  //! Ensures that we do not blame more than once per host wakeup.
  AtomicBool mHostWakeupBlamed{false};
};

}  // namespace chre

#endif  // CHRE_CORE_WAKEUP_STATS_MANAGER_H_
