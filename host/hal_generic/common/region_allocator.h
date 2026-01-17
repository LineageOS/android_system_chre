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

#include <cstdint>

#include <aidl/android/hardware/contexthub/IContextHub.h>

#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::hardware::contexthub::common::implementation {

using ::aidl::android::hardware::contexthub::SharedDataRegion;
using ::aidl::android::hardware::contexthub::SharedDataRegionRequirements;

/**
 * Interface for allocating and managing shared data regions.
 */
class RegionAllocator {
 public:
  virtual ~RegionAllocator() = default;

  /**
   * Creates a new region, returning its id.
   *
   * The region will remain allocated at least until releaseRegion() is called
   * with the returned id. Note that regions may be allocated and shared with
   * the common HAL implementation in other ways than this method in which case
   * releaseRegion() should still be called to release the region.
   */
  virtual pw::Result<SharedDataRegion> allocateRegion(
      const SharedDataRegionRequirements &requirements) = 0;

  /**
   * Retrieves the metadata and mappable file descriptor for the region with
   * given id. This region may have been allocated via allocateRegion() or
   * allocated internally and shared with the common HAL implementation (e.g.
   * when a data flow is shared by an offload endpoint with a host endpoint).
   */
  virtual pw::Result<SharedDataRegion> getRegionInfo(int32_t id) = 0;

  /**
   * Releases resources associated with the region with the given id. This
   * region may have been allocated via allocateRegion() or allocated internally
   * and shared with the common HAL implementation (e.g. when a data flow is
   * shared by an offload endpoint with a host endpoint).
   */
  virtual pw::Status releaseRegion(int32_t id) = 0;

  /**
   * Returns whether consumers on an embedded hub should be given a separate
   * consumer metadata region, i.e. whether the hub supports memory protections.
   */
  virtual bool consumerRequiresSeparateRegion(int64_t hubId) = 0;

 protected:
  RegionAllocator() = default;
};

}  // namespace android::hardware::contexthub::common::implementation
