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

#define LOG_TAG "DATA_FLOW.RegionManager"

#include "data_flow/host/region_manager.h"

#include <errno.h>
#include <inttypes.h>
#include <sys/mman.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <utils/Log.h>

#include "data_flow/queue_defs.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::aidl::android::hardware::contexthub::SharedDataRegion;

pw::Result<RegionManager::RegionToMap> convertSharedDataRegion(
    const SharedDataRegion &region) {
  auto fd = region.sharedMemory.dup();
  if (fd.get() < 0) {
    ALOGE("Failed to dup SharedDataRegion fd with %d", errno);
    return pw::Status::Internal();
  }
  return RegionManager::RegionToMap{
      .id = region.id,
      .fd = std::move(fd),
      .size = static_cast<size_t>(region.sizeBytes),
  };
}

/** Map a shared data region into this process's virtual memory space. */
pw::Result<uintptr_t> mapSharedDataRegion(RegionManager::RegionToMap &&region,
                                          bool readOnly) {
  // Move the region into this scope so that any fds are closed.
  auto tmp = std::move(region);
  if (tmp.size < 0 || tmp.fd.get() < 0) {
    ALOGE("Invalid shared data region to map: size %zu, fd %d", tmp.size,
          tmp.fd.get());
    return pw::Status::InvalidArgument();
  }
  int prot = readOnly ? PROT_READ : PROT_READ | PROT_WRITE;
  void *addr = mmap(/*addr=*/nullptr, tmp.size, prot, MAP_SHARED, tmp.fd.get(),
                    /*offset=*/0);
  if (addr != MAP_FAILED) {
    return reinterpret_cast<uintptr_t>(addr);
  }
  ALOGE("Failed to map shared data region with %d", errno);
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
    const SharedDataRegion &region) {
  PW_TRY_ASSIGN(auto regionToMap, convertSharedDataRegion(region));
  return mapHostProducerRegion(std::move(regionToMap));
}

pw::Result<AllocatorRegion> RegionManager::mapHostProducerRegion(
    RegionToMap &&region) {
  std::lock_guard lock(mLock);
  if (mIdToHostAllocatorRegion.contains(region.id)) {
    ALOGE("Attempted to map duplicate host producer region (%" PRId32 ")",
          region.id);
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
    ALOGE("Attempted to get unknown host producer region (%" PRId32 ")", id);
    return pw::Status::NotFound();
  } else if (it->second->consumers.has_value()) {
    ALOGE("Attempted to get consumer descriptor region (%" PRId32
          ") as though it was a host producer region",
          id);
    return pw::Status::InvalidArgument();
  }
  return *it->second;
}

pw::Status RegionManager::unmapHostProducerRegion(int id) {
  std::lock_guard lock(mLock);
  auto it = mIdToHostAllocatorRegion.find(id);
  if (it == mIdToHostAllocatorRegion.end()) {
    ALOGE("Attempted to unmap unknown host producer region (%" PRId32 ")", id);
    return pw::Status::NotFound();
  } else if (!it->second->dataFlows.empty()) {
    ALOGE("Attempted to unmap active host producer region (%" PRId32 ")", id);
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
    ALOGE("Attempted to link data flow (%" PRId32
          ") to unknown region (%" PRId32 ")",
          dataFlow, region);
    return pw::Status::NotFound();
  } else if (mHostProducerDataFlowToRegions.contains(dataFlow) ||
             !it->second->dataFlows.insert(dataFlow).second) {
    ALOGE("Attempted to link duplicate data flow (%" PRId32
          ") to region (%" PRId32 ")",
          dataFlow, region);
    return pw::Status::AlreadyExists();
  }
  mHostProducerDataFlowToRegions[dataFlow].insert(it->second.get());
  return pw::OkStatus();
}

pw::Result<size_t> RegionManager::unlinkHostProducerDataFlow(int id) {
  std::lock_guard lock(mLock);
  auto regionsIt = mHostProducerDataFlowToRegions.find(id);
  if (regionsIt == mHostProducerDataFlowToRegions.end()) {
    ALOGE("Attempted to unlink unknown data flow (%" PRId32 ")", id);
    return pw::Status::NotFound();
  }
  size_t primaryRegionReferences = 0;
  // Remove references to this data flow from all associated regions.
  for (auto *region : regionsIt->second) {
    auto it = mIdToHostAllocatorRegion.find(region->id);
    if (it == mIdToHostAllocatorRegion.end()) {
      LOG_ALWAYS_FATAL("Could not look up expected region (%" PRId32
                       ") associated with data flow (%" PRId32 ")",
                       region->id, id);
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
    const SharedDataRegion &region, const EndpointId &consumer, int dataFlow) {
  PW_TRY_ASSIGN(auto regionToMap, convertSharedDataRegion(region));
  return mapOffloadConsumerRegion(std::move(regionToMap), consumer, dataFlow);
}

pw::Result<AllocatorRegion> RegionManager::mapOffloadConsumerRegion(
    RegionToMap &&region, const EndpointId &consumer, int dataFlow) {
  std::lock_guard lock(mLock);
  auto regionsIt = mHostProducerDataFlowToRegions.find(dataFlow);
  if (regionsIt == mHostProducerDataFlowToRegions.end()) {
    ALOGE("Attempted to map region (%" PRId32 ") for consumer (%" PRIx64
          ", %" PRIx64 ") for unknown data flow (%" PRId32 ")",
          region.id, consumer.hubId, consumer.id, dataFlow);
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
RegionManager::mapHostConsumerRegions(const SharedDataRegion &region,
                                      const SharedDataRegion *metadataRegion,
                                      const DataFlowId &dataFlow) {
  PW_TRY_ASSIGN(auto regionToMap, convertSharedDataRegion(region));
  std::optional<RegionToMap> metadataRegionToMap;
  if (metadataRegion) {
    PW_TRY_ASSIGN(metadataRegionToMap,
                  convertSharedDataRegion(*metadataRegion));
  }
  return mapHostConsumerRegions(std::move(regionToMap),
                                std::move(metadataRegionToMap), dataFlow);
}

pw::Result<std::pair<Region, std::optional<Region>>>
RegionManager::mapHostConsumerRegions(
    RegionToMap &&region, std::optional<RegionToMap> &&metadataRegion,
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
    ALOGE("Attempted to unlink unknown data flow (%" PRIx64 ", %" PRIx32 ")",
          dataFlow.hubId, dataFlow.id);
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
    RegionToMap &&region, std::optional<EndpointId> consumer) {
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
RegionManager::mapAndLinkHostConsumerRegion(RegionToMap &&region,
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