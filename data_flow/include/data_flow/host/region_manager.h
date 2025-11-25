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

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <android-base/thread_annotations.h>

#include "data_flow/queue_defs.h"
#include "pw_allocator/dl_allocator.h"
#include "pw_allocator/synchronized_allocator.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::contexthub::data_flow {

/**
 * Manages the shared data regions between this endpoint and offload endpoints.
 * Creates and manages memory mappings, tracking references to data flows and
 * offload endpoints.
 *
 * This class is thread-safe.
 */
class RegionManager {
 public:
  /**
   * Maps a shared data region into which this endpoint will produce.
   *
   * The pw::Allocator* in the returned AllocatorRegion will be valid until
   * unmapHostProducerRegion() is called.
   *
   * @param region The region to map into this process's virtual memory space.
   * @return On success, the details of the mapping and an allocator over it.
   * pw::Status::Internal() if it is not possible to map the region.
   */
  pw::Result<AllocatorRegion> mapHostProducerRegion(
      ::aidl::android::hardware::contexthub::SharedDataRegion &&region)
      EXCLUDES(mLock);

  /** @return A host producer region's details or pw::Status::NotFound(). */
  pw::Result<AllocatorRegion> getHostProducerRegion(int id) EXCLUDES(mLock);

  /**
   * Unmaps a shared data region previously mapped with mapHostProducerRegion().
   *
   * All allocations in the region must have been freed before calling this
   * function.
   *
   * @param id The id of the region to unmap.
   * @return pw::Status::NotFound() if the region is not found.
   * pw::Status::FailedPrecondition() if the region is still in use.
   */
  pw::Status unmapHostProducerRegion(int id) EXCLUDES(mLock);

  /**
   * Links a data flow produced by this endpoint to a shared data region.
   *
   * @param region The id of the region the data flow will use.
   * @param dataFlow The id of the data flow (NOTE: this doesn't include hub id
   * since it must be the hub this endpoint is on).
   * @return pw::Status::NotFound() if the region is not found.
   * pw::Status::AlreadyExists() if the data flow is already added to the
   * region.
   */
  pw::Status linkHostProducerDataFlowToRegion(int region, int dataFlow)
      EXCLUDES(mLock);

  /**
   * Links a data flow produced by this endpoint.
   *
   * Triggers the removal of any offload consumer regions mapped with
   * mapOffloadConsumerRegion() if this is the last data flow using them.
   *
   * @param id The id of the data flow to remove.
   * @return On success, the reference count of the region hosting the data flow
   * after removal. pw::Status::NotFound() if the data flow is not found.
   * pw::Status::Internal() on unexpected internal error.
   */
  pw::Result<size_t> unlinkHostProducerDataFlow(int id) EXCLUDES(mLock);

  /**
   * Maps a shared data region for offload consumer descriptors.
   *
   * Doesn't map the region if it already exists. Links the region to the
   * consumer and data flow.
   *
   * The region is automatically unmapped and cleaned up if there are no data
   * flows using it or if the consumer is removed. The pw::Allocator* in the
   * returned AllocatorRegion will be invalidated in such a case.
   *
   * @param region The region to map into this process's virtual memory space.
   * @param consumer The id of the offload consumer.
   * @param dataFlow The id of the data flow (NOTE: this doesn't include hub id
   * since it must be the hub this endpoint is on).
   * @return On success, the details of the mapping and the allocator over it.
   * pw::Status::FailedPrecondition() if the data flow is not known.
   * pw::Status::Internal() if it is not possible to map the region.
   */
  pw::Result<AllocatorRegion> mapOffloadConsumerRegion(
      ::aidl::android::hardware::contexthub::SharedDataRegion &&region,
      const ::aidl::android::hardware::contexthub::EndpointId &consumer,
      int dataFlow) EXCLUDES(mLock);

  /**
   * Maps shared data region(s) this endpoint will consume from.
   *
   * Does nothing if the given region is already mapped.
   *
   * This region is automatically unmapped and cleaned up if there are no data
   * flows using it or if the producer is removed.
   *
   * @param region The region to map into this process's virtual memory space.
   * @param metadataRegion [optional] If provided, a separate writable region
   * containing the consumer metadata. If not provided, region will be writable.
   * @param dataFlow The id of the data flow.
   * @return On success, the details of the mapping. pw::Status::Internal() if
   * it is not possible to map the region.
   */
  pw::Result<std::pair<Region, std::optional<Region>>> mapHostConsumerRegions(
      ::aidl::android::hardware::contexthub::SharedDataRegion &&region,
      std::optional<::aidl::android::hardware::contexthub::SharedDataRegion>
          &&metadataRegion,
      const ::aidl::android::hardware::contexthub::DataFlowId &dataFlow)
      EXCLUDES(mLock);

  /**
   * Release resources associated with an offload producer data flow.
   *
   * Triggers the removal of any host consumer regions mapped with
   * mapHostConsumerRegion() if this is the last data flow using them.
   *
   * @param dataFlow The id of the data flow to remove resources for.
   * @return pw::Status::NotFound() if the data flow is not found.
   */
  pw::Status unlinkHostConsumerDataFlow(
      const ::aidl::android::hardware::contexthub::DataFlowId &dataFlow)
      EXCLUDES(mLock);

  /**
   * Release resources associated with an offload endpoint.
   *
   * Removes references to this consumer on any regions. If those regions have
   * no more consumer references, they are unmapped. All allocations for that
   * consumer must have been freed before calling this function.
   *
   * @param consumer The id of the offload consumer to remove references to.
   */
  void pruneOffloadConsumer(
      const ::aidl::android::hardware::contexthub::EndpointId &consumer)
      EXCLUDES(mLock);

 private:
  /** Represents a host producer region. */
  struct HostAllocatorRegion : public Region {
    pw::allocator::DlAllocator<> allocator;
    pw::allocator::SynchronizedAllocator<std::mutex> syncAllocator;
    std::unordered_set<int> dataFlows;
    // Set only if used to allocate descriptors for an offload consumer.
    std::optional<std::set<::aidl::android::hardware::contexthub::EndpointId>>
        consumers;
    int id;

    HostAllocatorRegion(int id, uintptr_t base, uint32_t size, bool isConsumer)
        : Region(base, size),
          allocator(pw::ByteSpan(reinterpret_cast<std::byte *>(base), size)),
          syncAllocator(allocator),
          id(id) {
      if (isConsumer) {
        consumers.emplace();
      }
    }

    operator AllocatorRegion() {
      return AllocatorRegion{
          {.base = base, .size = size},
          .allocator = &syncAllocator,
      };
    }
  };

  /** Represents a host consumer region. */
  struct HostConsumerRegion : public Region {
    std::set<::aidl::android::hardware::contexthub::DataFlowId> dataFlows;
    int id;

    HostConsumerRegion(int id, uintptr_t base, uint32_t size)
        : Region(base, size), id(id) {}
  };

  /** Maps a HostAllocatorRegion and links it into the master map. */
  pw::Result<HostAllocatorRegion *> mapAndLinkHostAllocatorRegion(
      ::aidl::android::hardware::contexthub::SharedDataRegion &&region,
      std::optional<::aidl::android::hardware::contexthub::EndpointId> consumer)
      EXCLUSIVE_LOCKS_REQUIRED(mLock);

  /** Maps a HostConsumerRegion and links it into the master map. */
  pw::Result<HostConsumerRegion *> mapAndLinkHostConsumerRegion(
      ::aidl::android::hardware::contexthub::SharedDataRegion &&region,
      bool readOnly) EXCLUSIVE_LOCKS_REQUIRED(mLock);

  /** Removes a data flow from a HostConsumerRegion, possibly unmapping it. */
  void unlinkDataFlowFromHostConsumerRegion(
      HostConsumerRegion *region,
      ::aidl::android::hardware::contexthub::DataFlowId dataFlow)
      EXCLUSIVE_LOCKS_REQUIRED(mLock);

  /** Unmaps a HostAllocatorRegion and removes it from the master map. */
  void unmapHostAllocatorRegion(HostAllocatorRegion *region)
      EXCLUSIVE_LOCKS_REQUIRED(mLock);

  /** Guards internal state. */
  std::mutex mLock;
  /** Master map of all mapped HostAllocatorRegions. */
  std::unordered_map<int, std::unique_ptr<HostAllocatorRegion>>
      mIdToHostAllocatorRegion GUARDED_BY(mLock);
  /** Master map of all mapped HostConsumerRegions. */
  std::unordered_map<int, std::unique_ptr<HostConsumerRegion>>
      mIdToHostConsumerRegion GUARDED_BY(mLock);
  /** Map of host producer data flow to HostAllocatorRegions. */
  std::unordered_map<int, std::unordered_set<HostAllocatorRegion *>>
      mHostProducerDataFlowToRegions GUARDED_BY(mLock);
  /** Map of offload consumer to associated HostAllocatorRegions. */
  std::map<::aidl::android::hardware::contexthub::EndpointId,
           std::unordered_set<HostAllocatorRegion *>>
      mOffloadConsumerToRegions GUARDED_BY(mLock);
  /** Map of offload producer data flow to region. */
  std::map<::aidl::android::hardware::contexthub::DataFlowId,
           std::pair<HostConsumerRegion *, HostConsumerRegion *>>
      mOffloadProducerDataFlowToRegions GUARDED_BY(mLock);
};

}  // namespace android::contexthub::data_flow