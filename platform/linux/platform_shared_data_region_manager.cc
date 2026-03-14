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

#include "chre/platform/platform_shared_data_region_manager.h"

#include "chre/core/event_loop_manager.h"
#include "chre/core/shared_data_region_manager.h"
#include "data_flow/queue.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

#include <thread>

namespace chre {

namespace {

//! The region ID to use for all regions.
constexpr int32_t kRegionId = 0;

}  // anonymous namespace

pw::Result<uintptr_t>
PlatformSharedDataRegionManager::allocateDataFlowRegionAsync(
    uint32_t /*domains*/, uint32_t /*size*/,
    uint64_t /*minAverageWriteIntervalNs*/,
    uint32_t /*maxAverageWriteBandwidthBytesPerSecond*/) {
  // For the linux implementation,  we only have one region, which is just the
  // whole heap. Our allocator just calls (eventually) malloc() and free(). This
  // should be large enough to allocate any data flow in this region.

  uintptr_t cookie = mCookie++;
  auto callback = [](uint16_t /*type*/, void *data, void *extraData) {
    auto *manager = static_cast<PlatformSharedDataRegionManager *>(extraData);
    auto cookieValue = reinterpret_cast<uintptr_t>(data);
    EventLoopManagerSingleton::get()
        ->getSharedDataRegionManager()
        .handleAllocateDataFlowRegionAsyncResult(
            cookieValue, pw::OkStatus(),
            /* regionId= */ kRegionId,
            /* region= */ {.allocator = &manager->mAllocator},
            /* memoryAccess= */ nullptr);
  };
  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::AllocateDataFlowRegionAsyncResult,
      reinterpret_cast<void *>(cookie), callback, this);
  return cookie;
}

pw::Status PlatformSharedDataRegionManager::deallocateRegion(
    int32_t /*regionId*/) {
  ++mNumCallsToDeallocateRegion;
  return pw::OkStatus();
}

pw::Result<std::pair<android::contexthub::data_flow::Region,
                     android::contexthub::data_flow::MemoryAccess *>>
PlatformSharedDataRegionManager::incrementRegionRefCount(int32_t /*regionId*/) {
  // TODO(b/475656750): Implement this.
  return pw::Status::Unimplemented();
}

pw::Status PlatformSharedDataRegionManager::decrementRegionRefCount(
    int32_t /*regionId*/) {
  // TODO(b/475656750): Implement this.
  return pw::Status::Unimplemented();
}

}  // namespace chre