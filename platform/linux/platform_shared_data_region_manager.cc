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
#include "pw_allocator/best_fit.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

using ::android::contexthub::data_flow::AllocatorRegion;

namespace chre {

namespace {

//! The region ID to use for all regions.
constexpr int32_t kRegionId = 0;

//! The region size.
constexpr size_t kRegionSize = 1024 * 1024;

//! The region buffer.
std::byte gRegionBuffer[kRegionSize];

//! The span for the region buffer.
pw::span<std::byte> gRegionSpan(gRegionBuffer, gRegionBuffer + kRegionSize);

//! Returns the allocator for the region.
pw::Allocator *getAllocator() {
  static pw::allocator::BestFitAllocator<pw::allocator::BestFitBlock<uintptr_t>>
      sAllocator(gRegionSpan);
  return &sAllocator;
}

}  // anonymous namespace

pw::Result<uintptr_t>
PlatformSharedDataRegionManager::allocateDataFlowRegionAsync(
    uint32_t /*domains*/, uint32_t size, uint64_t /*minAverageWriteIntervalNs*/,
    uint32_t /*maxAverageWriteBandwidthBytesPerSecond*/) {
  if (size > kRegionSize) {
    return pw::Status::ResourceExhausted();
  }

  uintptr_t cookie = mCookie++;
  auto callback = [](uint16_t /*type*/, void *data, void * /*extraData*/) {
    auto cookieValue = reinterpret_cast<uintptr_t>(data);
    AllocatorRegion region;
    region.base = reinterpret_cast<uintptr_t>(gRegionBuffer);
    region.size = kRegionSize;
    region.allocator = getAllocator();
    EventLoopManagerSingleton::get()
        ->getSharedDataRegionManager()
        .handleAllocateDataFlowRegionAsyncResult(cookieValue, pw::OkStatus(),
                                                 /* regionId= */ kRegionId,
                                                 region,
                                                 /* memoryAccess= */ nullptr);
  };

  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::AllocateDataFlowRegionAsyncResult,
      reinterpret_cast<void *>(cookie), callback);
  return cookie;
}

pw::Status PlatformSharedDataRegionManager::deallocateRegion(int32_t regionId) {
  if (regionId != kRegionId) {
    return pw::Status::InvalidArgument();
  }

  ++mNumCallsToDeallocateRegion;
  return pw::OkStatus();
}

pw::Result<std::pair<android::contexthub::data_flow::Region,
                     android::contexthub::data_flow::MemoryAccess *>>
PlatformSharedDataRegionManager::incrementRegionRefCount(int32_t regionId) {
  if (regionId != kRegionId) {
    return pw::Status::InvalidArgument();
  }
  std::pair<android::contexthub::data_flow::Region,
            android::contexthub::data_flow::MemoryAccess *>
      retVal = {{.base = reinterpret_cast<uintptr_t>(gRegionBuffer),
                 .size = kRegionSize},
                nullptr};
  return retVal;
}

pw::Status PlatformSharedDataRegionManager::decrementRegionRefCount(
    int32_t regionId) {
  if (regionId != kRegionId) {
    return pw::Status::InvalidArgument();
  }
  return pw::OkStatus();
}

bool PlatformSharedDataRegionManager::sinkOnHubRequiresSeparateMetadataRegion(
    message::MessageHubId hubId) {
  return EventLoopManagerSingleton::get()
      ->getHostMessageHubManager()
      .isHostHub(hubId);
}

}  // namespace chre