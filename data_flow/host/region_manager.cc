/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include "data_flow/host/region_manager.h"

#include <sys/mman.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <log/log.h>

#include "data_flow/queue_defs.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::aidl::android::hardware::contexthub::SharedDataRegion;

/** Map a shared data region into this process's virtual memory space. */
pw::Result<uintptr_t> mapSharedDataRegion(SharedDataRegion &&region,
                                          bool readOnly) {
  // Move the region into this scope so that any fds are closed.
  auto tmp = std::move(region);
  if (tmp.size < 0 || tmp.sharedMemory.get() < 0) {
    return pw::Status::InvalidArgument();
  }
  int prot = readOnly ? PROT_READ : PROT_READ | PROT_WRITE;
  void *addr = mmap(/*addr=*/nullptr, tmp.size, prot, MAP_SHARED,
                    tmp.sharedMemory.get(), /*offset=*/0);
  if (addr != MAP_FAILED) {
    return reinterpret_cast<uintptr_t>(addr);
  }
  switch (errno) {
    case EACCES:
      [[fallthrough]];
    case EPERM:
      return pw::Status::PermissionDenied();
    case EBADF:
      [[fallthrough]];
    case EINVAL:
      return pw::Status::InvalidArgument();
    case ENOMEM:
      return pw::Status::ResourceExhausted();
    default:
      return pw::Status::Internal();
  }
}

}  // namespace

pw::Result<AllocatorRegion> RegionManager::mapHostProducerRegion(
    SharedDataRegion &&region) {
  std::lock_guard lock(mLock);
  if (mIdToHostAllocatorRegion.contains(region.id)) {
    return pw::Status::AlreadyExists();
  }
  PW_TRY_ASSIGN(auto *mappedRegion,
                mapAndLinkHostAllocatorRegion(std::move(region),
                                              /*consumer=*/std::nullopt));
  return *mappedRegion;
}

pw::Result<AllocatorRegion> RegionManager::getHostProducerRegion(int id) {
  std::lock_guard lock(mLock);
  auto it = mIdToHostAllocatorRegion.find(id);
  if (it == mIdToHostAllocatorRegion.end()) {
    return pw::Status::NotFound();
  } else if (it->second->consumers.has_value()) {
    return pw::Status::InvalidArgument();
  }
  return *it->second;
}

pw::Status RegionManager::unmapHostProducerRegion(int id) {
  std::lock_guard lock(mLock);
  auto it = mIdToHostAllocatorRegion.find(id);
  if (it == mIdToHostAllocatorRegion.end()) {
    return pw::Status::NotFound();
  } else if (!it->second->dataFlows.empty()) {
    return pw::Status::FailedPrecondition();
  }
  auto *base = reinterpret_cast<void *>(it->second->base);
  size_t size = it->second->size;
  mIdToHostAllocatorRegion.erase(it);
  munmap(base, size);
  return pw::OkStatus();
}

pw::Status RegionManager::linkHostProducerDataFlowToRegion(int region,
                                                           int dataFlow) {
  std::lock_guard lock(mLock);
  auto it = mIdToHostAllocatorRegion.find(region);
  if (it == mIdToHostAllocatorRegion.end()) {
    return pw::Status::NotFound();
  } else if (mHostProducerDataFlowToRegions.contains(dataFlow) ||
             !it->second->dataFlows.insert(dataFlow).second) {
    return pw::Status::AlreadyExists();
  }
  mHostProducerDataFlowToRegions[dataFlow].insert(it->second.get());
  return pw::OkStatus();
}

pw::Result<size_t> RegionManager::unlinkHostProducerDataFlow(int id) {
  std::lock_guard lock(mLock);
  auto regionsIt = mHostProducerDataFlowToRegions.find(id);
  if (regionsIt == mHostProducerDataFlowToRegions.end()) {
    return pw::Status::NotFound();
  }
  size_t primaryRegionReferences = 0;
  // Remove references to this data flow from all associated regions.
  for (auto *region : regionsIt->second) {
    auto it = mIdToHostAllocatorRegion.find(region->id);
    if (it == mIdToHostAllocatorRegion.end()) {
      LOG_FATAL("Could not look up expected HostAllocatorRegion");
      continue;
    }
    region->dataFlows.erase(id);
    if (!region->consumers) {  // Check if this is the host producer region.
      // Get the number of references to the primary region remaining.
      primaryRegionReferences = region->dataFlows.size();
      continue;
    } else if (region->dataFlows.empty()) {
      // This region has no more data flows referencing it. Remove references
      // with offload consumers and unmap it.
      for (auto consumerIt = region->consumers->begin();
           consumerIt != region->consumers->end();) {
        auto &consumerRegions = mOffloadConsumerToRegions[*consumerIt];
        consumerRegions.erase(region);
        // Remove the consumer if no more regions are associated with it.
        if (consumerRegions.empty()) {
          mOffloadConsumerToRegions.erase(*consumerIt);
          consumerIt = region->consumers->erase(consumerIt);
        } else {
          ++consumerIt;
        }
      }
      unmapHostAllocatorRegion(region);
    }
  }
  // Remove the data flow from the map.
  mHostProducerDataFlowToRegions.erase(regionsIt);
  return primaryRegionReferences;
}

pw::Result<AllocatorRegion> RegionManager::mapOffloadConsumerRegion(
    SharedDataRegion &&region, const EndpointId &consumer, int dataFlow) {
  std::lock_guard lock(mLock);
  auto regionsIt = mHostProducerDataFlowToRegions.find(dataFlow);
  if (regionsIt == mHostProducerDataFlowToRegions.end()) {
    return pw::Status::FailedPrecondition();
  }
  HostAllocatorRegion *mappedRegion;
  auto it = mIdToHostAllocatorRegion.find(region.id);
  if (it != mIdToHostAllocatorRegion.end()) {
    mappedRegion = it->second.get();
  } else {
    PW_TRY_ASSIGN(mappedRegion,
                  mapAndLinkHostAllocatorRegion(std::move(region), consumer));
  }
  // Link the region to the data flow.
  mappedRegion->dataFlows.insert(dataFlow);
  regionsIt->second.insert(mappedRegion);
  // Link the region to the consumer.
  mappedRegion->consumers->insert(consumer);
  mOffloadConsumerToRegions[consumer].insert(mappedRegion);
  return *mappedRegion;
}

pw::Result<std::pair<Region, std::optional<Region>>>
RegionManager::mapHostConsumerRegions(
    SharedDataRegion &&region, std::optional<SharedDataRegion> &&metadataRegion,
    const DataFlowId &dataFlow) {
  std::lock_guard lock(mLock);
  // Create and map the region(s) if necessary.
  HostConsumerRegion *regionPtr, *metadataRegionPtr = nullptr;
  if (auto it = mIdToHostConsumerRegion.find(region.id);
      it != mIdToHostConsumerRegion.end()) {
    regionPtr = it->second.get();
  } else {
    PW_TRY_ASSIGN(regionPtr, mapAndLinkHostConsumerRegion(
                                 std::move(region),
                                 /*readOnly=*/metadataRegion.has_value()));
  }
  if (metadataRegion) {
    if (auto it = mIdToHostConsumerRegion.find(metadataRegion->id);
        it != mIdToHostConsumerRegion.end()) {
      metadataRegionPtr = it->second.get();
    } else {
      PW_TRY_ASSIGN(metadataRegionPtr,
                    mapAndLinkHostConsumerRegion(std::move(*metadataRegion),
                                                 /*readOnly=*/false));
    }
  }
  // Link the region to the data flow.
  regionPtr->dataFlows.insert(dataFlow);
  auto &dataFlowRegions =
      mOffloadProducerDataFlowToRegions[dataFlow] = {regionPtr, nullptr};
  if (metadataRegionPtr) {
    metadataRegionPtr->dataFlows.insert(dataFlow);
    dataFlowRegions.second = metadataRegionPtr;
  }
  return std::make_pair(
      *regionPtr, metadataRegionPtr ? std::optional<Region>(*metadataRegionPtr)
                                    : std::nullopt);
}

pw::Status RegionManager::unlinkHostConsumerDataFlow(
    const DataFlowId &dataFlow) {
  std::lock_guard lock(mLock);
  auto regionsIt = mOffloadProducerDataFlowToRegions.find(dataFlow);
  if (regionsIt == mOffloadProducerDataFlowToRegions.end()) {
    return pw::Status::NotFound();
  }
  // Remove all references to this data flow.
  auto &regionPair = regionsIt->second;
  unlinkDataFlowFromHostConsumerRegion(regionPair.first, dataFlow);
  if (regionPair.second) {
    unlinkDataFlowFromHostConsumerRegion(regionPair.second, dataFlow);
  }
  // Remove the data flow from the map.
  mOffloadProducerDataFlowToRegions.erase(regionsIt);
  return pw::OkStatus();
}

void RegionManager::pruneOffloadConsumer(const EndpointId &consumer) {
  std::lock_guard lock(mLock);
  auto it = mOffloadConsumerToRegions.find(consumer);
  if (it == mOffloadConsumerToRegions.end()) {
    return;
  }
  for (auto regionIt = it->second.begin(); regionIt != it->second.end();) {
    auto &region = *regionIt;
    region->consumers->erase(consumer);
    if (!region->consumers->empty()) {
      ++regionIt;
      continue;
    }
    // The region has no more consumers. Remove data flow references to it and
    // unmap it.
    for (const auto &dataFlow : region->dataFlows) {
      mHostProducerDataFlowToRegions[dataFlow].erase(region);
    }
    unmapHostAllocatorRegion(region);
    regionIt = it->second.erase(regionIt);
  }
  // Remove the consumer from the map.
  mOffloadConsumerToRegions.erase(it);
}

pw::Result<RegionManager::HostAllocatorRegion *>
RegionManager::mapAndLinkHostAllocatorRegion(
    SharedDataRegion &&region, std::optional<EndpointId> consumer) {
  int id = region.id;
  uint32_t size = region.size;
  PW_TRY_ASSIGN(auto base,
                mapSharedDataRegion(std::move(region), /*readOnly=*/false));
  auto &mappedRegion = mIdToHostAllocatorRegion[id] =
      std::make_unique<HostAllocatorRegion>(id, base, size,
                                            consumer.has_value());
  return mappedRegion.get();
}

pw::Result<RegionManager::HostConsumerRegion *>
RegionManager::mapAndLinkHostConsumerRegion(SharedDataRegion &&region,
                                            bool readOnly) {
  int id = region.id;
  size_t size = region.size;
  PW_TRY_ASSIGN(auto base, mapSharedDataRegion(std::move(region), readOnly));
  auto &mappedRegion = mIdToHostConsumerRegion[id] =
      std::make_unique<HostConsumerRegion>(id, base, size);
  return mappedRegion.get();
}

void RegionManager::unlinkDataFlowFromHostConsumerRegion(
    HostConsumerRegion *region, DataFlowId dataFlow) {
  region->dataFlows.erase(dataFlow);
  if (region->dataFlows.empty()) {
    // This is the last data flow referencing the region. Unmap the region.
    munmap(reinterpret_cast<void *>(region->base), region->size);
    mIdToHostConsumerRegion.erase(region->id);
  }
}

void RegionManager::unmapHostAllocatorRegion(HostAllocatorRegion *region) {
  // Unmap the region after destroying the allocator.
  auto *base = reinterpret_cast<void *>(region->base);
  size_t size = region->size;
  mIdToHostAllocatorRegion.erase(region->id);  // Invalidates region.
  munmap(base, size);
}

}  // namespace android::contexthub::data_flow