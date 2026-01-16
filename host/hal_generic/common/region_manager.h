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
 * Interface for allocating and managing shared data regions for host-driven
 * use-cases (e.g. data flow whose source is a host endpoint). Also used for
 * querying region information for all regions, including for offload-driven
 * use-cases.
 */
class RegionManager {
 public:
  virtual ~RegionManager() = default;

  /** Creates a new region, returning its id. */
  virtual pw::Result<SharedDataRegion> allocateRegion(
      const SharedDataRegionRequirements &requirements) = 0;

  /** Releases a region allocated via allocateRegion(). */
  virtual pw::Status releaseRegion(int32_t id) = 0;

  /**
   * Retrieves the metadata and mappable file descriptor for the region with
   * given id. This region may have been allocated for a host hub via
   * allocateRegion() or allocated internally for an embedded hub.
   */
  virtual pw::Result<SharedDataRegion> getRegionInfo(int32_t id) = 0;

  /**
   * Returns whether consumers on an embedded hub should be given a separate
   * consumer metadata region, i.e. whether the hub supports memory protections.
   */
  virtual bool consumerRequiresSeparateRegion(int64_t hubId) = 0;

 protected:
  RegionManager() = default;
};

}  // namespace android::hardware::contexthub::common::implementation
