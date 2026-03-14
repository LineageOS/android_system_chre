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

#if defined(CHRE_DATA_FLOW_SUPPORT_ENABLED)

#include "chre/core/shared_data_region_manager.h"

#include "chre/core/event_loop_manager.h"
#include "chre/util/lock_guard.h"
#include "chre/util/unique_ptr.h"

namespace chre {

void SharedDataRegionManager::handleDataFlowStopped(int32_t regionId) {
  LockGuard lock(mMutex);
  for (RegionRefCounter &regionRef: mRegions) {
    if (regionRef.regionId == regionId) {
      if (regionRef.refCount <= 1) {
        pw::Status status = deallocateRegion(regionId);
        if (!status.ok()) {
          LOGE("Cannot dealloate region with ID: %" PRId32 " and status: %s",
               regionId, status.str());
        }
        mRegions.erase(&regionRef);
      } else {
        --regionRef.refCount;
      }
      return;
    }
  }
  LOGW("handleDataFlowStopped called on an invalid region with ID: %" PRId32, regionId);
}

void SharedDataRegionManager::handleAllocateDataFlowRegionAsyncResult(
    uintptr_t cookie, pw::Status status, int32_t regionId,
    const android::contexthub::data_flow::AllocatorRegion &region,
    android::contexthub::data_flow::MemoryAccess *memoryAccess) {
  if (status.ok()) {
    LockGuard lock(mMutex);
    bool found = false;
    for (RegionRefCounter &regionRef : mRegions) {
      if (regionRef.regionId == regionId) {
        ++regionRef.refCount;
        found = true;
        break;
      }
    }

    if (!found) {
      if (mRegions.full()) {
        FATAL_ERROR_OOM();
      }
      mRegions.push_back({.regionId = regionId, .refCount = 1});
    }
  }

  auto result = MakeUnique<DataFlowAllocationResult>();
  result->cookie = cookie;
  result->status = status;
  result->regionId = regionId;
  result->region = region;
  result->memoryAccess = memoryAccess;

  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::SharedDataRegionAllocation, std::move(result),
      [](SystemCallbackType /*type*/,
         UniquePtr<DataFlowAllocationResult> &&data) {
        EventLoopManagerSingleton::get()
            ->getDataFlowManager()
            .handleAllocateDataFlowRegionAsyncResult(
                data->cookie, data->status, data->regionId, data->region,
                data->memoryAccess);
      });
}

void SharedDataRegionManager::handleRegionInvalidated(int32_t /*regionId*/) {
  // TODO(b/457453613): Implement this.
}

}  // namespace chre

#endif  // defined(CHRE_DATA_FLOW_SUPPORT_ENABLED)
