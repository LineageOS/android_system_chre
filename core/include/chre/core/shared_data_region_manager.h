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

#if defined(CHRE_DATA_FLOW_SUPPORT_ENABLED)

#include <cstdint>
#include "chre/platform/mutex.h"
#include "chre/platform/platform_shared_data_region_manager.h"
#include "chre/util/non_copyable.h"
#include "data_flow/queue.h"
#include "pw_containers/vector.h"
#include "pw_status/status.h"

namespace chre {

/**
 * Manages the shared memory regions used for nanoapp data flow support.
 *
 * See the documentation on chre::Nanoapp to understand the relationship between
 * *Manager, Platform*Manager, and Platform*ManagerBase.
 */
class SharedDataRegionManager : public PlatformSharedDataRegionManager {
 public:
  /**
   * RAII wrapper for shared data regions not allocated by CHRE.
   *
   * Increments the reference count of the region on construction and decrements
   * it on destruction.
   */
  class RegionGuard : public NonCopyable {
   public:
    explicit RegionGuard(int32_t regionId);
    ~RegionGuard();

    RegionGuard(RegionGuard &&other);
    RegionGuard &operator=(RegionGuard &&other);

    bool isValid() const {
      return mIsValid;
    }
    int32_t getRegionId() const {
      return mRegionId;
    }
    android::contexthub::data_flow::Region getRegion() const {
      return mRegion;
    }
    android::contexthub::data_flow::MemoryAccess *getMemoryAccess() const {
      return mMemoryAccess;
    }

   private:
    friend class SharedDataRegionManager;

    int32_t mRegionId;
    android::contexthub::data_flow::Region mRegion;
    android::contexthub::data_flow::MemoryAccess *mMemoryAccess;
    bool mIsValid;
  };

  /**
   * Handles when a data flow stops on a region created using this manager.
   * This will deallocate the region if needed.
   */
  void handleDataFlowStopped(int32_t regionId);

  /**
   * Delivers the result of the allocation of a shared data region on behalf of
   * a nanoapp that will be the source of a data flow in that region.
   *
   * @param cookie The cookie returned by
   * PlatformSharedDataRegionManager::allocateDataFlowRegionAsync()
   * @param status The status of the request, pw::OkStatus() on success
   * @param regionId The ID of the region that was allocated
   * @param region Details of the allocated region including the allocator used
   * to manage it
   * @param [opt] memoryAccess If present, an object used to access the region
   */
  void handleAllocateDataFlowRegionAsyncResult(
      uintptr_t cookie, pw::Status status, int32_t regionId,
      const android::contexthub::data_flow::AllocatorRegion &region,
      android::contexthub::data_flow::MemoryAccess *memoryAccess);

  /**
   * Delivers an event indicating that a region has been invalidated.
   *
   * This method is only invoked in exceptional circumstances, e.g. the
   * connection to the host has been reset.
   *
   * @param regionId The ID of the region that is no longer valid
   */
  void handleRegionInvalidated(int32_t regionId);

 private:
  //! Maximum number of regions tracked in the reference counter
  constexpr static size_t kMaxNumRegions = 10;

  //! A struct to hold the result of an async data flow region allocation.
  struct DataFlowAllocationResult {
    uintptr_t cookie;
    pw::Status status;
    int32_t regionId;
    android::contexthub::data_flow::AllocatorRegion region;
    android::contexthub::data_flow::MemoryAccess *memoryAccess;
  };

  //! Region reference counter
  struct RegionRefCounter {
    int32_t regionId;
    size_t refCount;
  };

  //! Lock for the regions vector
  Mutex mMutex;

  //! Regions created through this manager
  pw::Vector<RegionRefCounter, kMaxNumRegions> mRegions;
};

}  // namespace chre

#endif  // defined(CHRE_DATA_FLOW_SUPPORT_ENABLED)
