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

#include "chre/target_platform/platform_shared_data_region_manager_base.h"
#include "chre/util/non_copyable.h"
#include "chre/util/system/message_common_types.h"
#include "data_flow/queue.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace chre {

/**
 * The common interface to shared data region management functionality that must
 * be implemented by the platform.
 */
class PlatformSharedDataRegionManager
    : public PlatformSharedDataRegionManagerBase,
      public NonCopyable {
 public:
  /**
   * Allocates a shared memory region intended for hosting data flows.
   *
   * @param domains A bitmask of the domains which will need direct access to
   * the region.
   * @param size A lower bound on the size of the region to allocate.
   * @param minAverageWriteIntervalNs The expected minimum average (sustained)
   * interval between successive writes to the data flow, in nanoseconds. See
   * chre_api/chre/data_flow.h for more details.
   * @param maxAverageWriteBandwidthBytesPerSecond The expected maximum average
   * (sustained) write bandwidth for the data flow, in bytes per second. See
   * chre_api/chre/data_flow.h for more details.
   *
   * @return On success, a cookie used to identify the asynchronous result
   * delivered by
   * SharedDataRegionManager::handleAllocateDataFlowRegionAsyncResult(),
   * otherwise:
   * - pw::Status::InvalidArgument() if any of the arguments are invalid
   * - pw::Status::Internal() if the allocation request cannot be dispatched
   */
  pw::Result<uintptr_t> allocateDataFlowRegionAsync(
      uint32_t domains, uint32_t size, uint64_t minAverageWriteIntervalNs,
      uint32_t maxAverageWriteBandwidthBytesPerSecond);

  /**
   * Deallocates a shared memory region allocated through this class.
   *
   * @param regionId The ID of the region to deallocate, obtained from
   * SharedDataRegionManager::handleAllocateDataFlowRegionAsyncResult()
   *
   * @return pw::OkStatus() on success, otherwise:
   * - pw::Status::InvalidArgument() if the region ID is invalid
   * - pw::Status::NotFound() if the region ID is not found
   * - pw::Status::Internal() if the deallocation request cannot be dispatched
   */
  pw::Status deallocateRegion(int32_t regionId);

  /**
   * Increases the reference count on a region with the given ID.
   *
   * This method is only used for regions not allocated by
   * allocateDataFlowRegionAsync(), as regions allocated that way remain valid
   * until deallocateRegion() is called. These region IDs are received via data
   * flow messages sent over message router which include a region ID.
   *
   * Once this method returns successfully once for a given region, the region
   * will not be deallocated as long as decrementRegionRefCount() has been
   * called less times for the same region, with the exception of
   * SharedDataRegionManager::handleRegionInvalidation() being called for the
   * region.
   *
   * @param regionId The ID of the region to increase the reference count for
   *
   * @return On success, the region info and optional memory access object,
   * otherwise:
   * - pw::Status::InvalidArgument() if the region ID is invalid
   * - pw::Status::NotFound() if the region ID is not found
   * - pw::Status::Internal() if the request failed due to a platform error
   */
  pw::Result<std::pair<android::contexthub::data_flow::Region,
                       android::contexthub::data_flow::MemoryAccess *>>
  incrementRegionRefCount(int32_t regionId);

  /**
   * Decreases the reference count on a region with the given ID.
   *
   * If the reference count reaches zero, the region may be deallocated.
   *
   * @param regionId The ID of the region to decrease the reference count for
   *
   * @return pw::OkStatus() on success, otherwise:
   * - pw::Status::InvalidArgument() if the region ID is invalid
   * - pw::Status::NotFound() if the region ID is not found
   * - pw::Status::FailedPrecondition() if the reference count is already zero
   * - pw::Status::Internal() if the request failed due to a platform error
   */
  pw::Status decrementRegionRefCount(int32_t regionId);

  /**
   * Checks if sink endpoints on the given hub need a separate metadata region.
   *
   * For a sink on a hub where memory protection is supported, the data flow
   * source metadata and data storage are exposed to the sink in a read-only
   * region. The sink metadata is stored in a separate read-write region so that
   * it can update its read index and flags.
   *
   * @param hubId The ID of the hub to check.
   *
   * @return true if sink endpoints on the given hub need a separate metadata
   * region, false otherwise.
   */
  bool sinkOnHubRequiresSeparateMetadataRegion(message::MessageHubId hubId);
};

}  // namespace chre

//! The platform can optionally provide an inlined implementation
#if __has_include( \
    "chre/target_platform/platform_shared_data_region_manager_impl.h")
#include "chre/target_platform/platform_shared_data_region_manager_impl.h"
#endif  // __has_include("chre/target_platform/platform_shared_data_region_manager_impl.h")
