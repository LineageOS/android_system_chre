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

#pragma once

#include <cstddef>

#include "pw_status/status.h"

namespace android::hardware::contexthub::common::implementation {

/** Interface for acquiring and managing a wakelock across use-cases. */
class WakelockManager {
 public:
  virtual ~WakelockManager() = default;

  /** Use-case for acquiring (increment the reference count) on the wakelock. */
  enum class Usage {
    kDataFlow,
  };

  /**
   * Increases the wakelock reference count, acquiring it if not already held.
   *
   * @param usage The use-case for which to acquire the wakelock.
   * @param count The amount to increase the reference count by.
   * @return pw::OkStatus() on success.
   */
  virtual pw::Status increaseWakeCount(Usage usage, size_t count) = 0;

  /**
   * Decrements the wakelock reference count, releasing it if it reaches 0.
   *
   * @param usage The use-case for which to release the wakelock.
   * @param count The amount to decrease the reference count by.
   * @return pw::OkStatus() on success.
   */
  virtual pw::Status decreaseWakeCount(Usage usage, size_t count) = 0;

 protected:
  WakelockManager() = default;
};

}  // namespace android::hardware::contexthub::common::implementation
