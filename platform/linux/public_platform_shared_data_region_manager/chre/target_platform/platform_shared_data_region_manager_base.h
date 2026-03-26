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
#include <cstdint>

#include "data_flow/queue_defs.h"

namespace chre {

/** Fake implementation of PlatformSharedDataRegionManagerBase for testing. */
class PlatformSharedDataRegionManagerBase {
 public:
  PlatformSharedDataRegionManagerBase() = default;
  ~PlatformSharedDataRegionManagerBase() = default;

  /** Visible for testing. */
  size_t getNumCallsToDeallocateRegion() const {
    return mNumCallsToDeallocateRegion;
  }

  /** Visible for testing. */
  void resetNumCallsToDeallocateRegion() {
    mNumCallsToDeallocateRegion = 0;
  }

  /**
   * Sets the allocator region for a region with the given ID. Visible for
   * testing.
   *
   * @param allocatorRegion The allocator region to set for the region.
   */
  void setAllocatorRegion(
      const ::android::contexthub::data_flow::AllocatorRegion &allocatorRegion);

  /**
   * Clears the allocator region for a region with the given ID. Visible for
   * testing.
   *
   * @param regionId The ID of the region to clear the allocator region for.
   */
  void clearAllocatorRegion();

 protected:
  //! The number of calls to deallocateRegion.
  size_t mNumCallsToDeallocateRegion = 0;

  //! The cookie to return for all async allocation requests.
  uintptr_t mCookie = 0xDEADBEEF;
};

}  // namespace chre
