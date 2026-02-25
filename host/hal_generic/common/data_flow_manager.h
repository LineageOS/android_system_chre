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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <aidl/android/hardware/contexthub/DataFlowAlertFds.h>
#include <aidl/android/hardware/contexthub/DataFlowId.h>
#include <aidl/android/hardware/contexthub/DataFlowInfo.h>
#include <aidl/android/hardware/contexthub/DataFlowSinkContext.h>
#include <aidl/android/hardware/contexthub/DataFlowSinkRegistrationParams.h>
#include <aidl/android/hardware/contexthub/EndpointId.h>
#include <aidl/android/hardware/contexthub/SharedDataRegion.h>
#include <aidl/android/hardware/contexthub/SharedDataRegionRequirements.h>
#include <android-base/thread_annotations.h>

#include "data_flow_epoll_waiter.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "region_allocator.h"
#include "wakelock_manager.h"

namespace android::hardware::contexthub::common::implementation {

using ::aidl::android::hardware::contexthub::DataFlowAlertFds;
using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::DataFlowInfo;
using ::aidl::android::hardware::contexthub::DataFlowSinkContext;
using ::aidl::android::hardware::contexthub::DataFlowSinkRegistrationParams;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::aidl::android::hardware::contexthub::SharedDataRegion;
using ::aidl::android::hardware::contexthub::SharedDataRegionRequirements;

/**
 * Manages all data flows that involve a host endpoint. Owns the
 * DataFlowEpollWaiter instance. This class is thread-safe. It is expected to be
 * used by MessageHubManager, which is responsible for doing any endpoint checks
 * before calling into this class.
 */
class DataFlowManager : protected DataFlowEpollWaiter::Callback {
 public:
  /**
   * Function to forward a data flow alert to an offload endpoint.
   *
   * @param dataFlow The ID of the data flow.
   * @param receiver The endpoint that should receive the alert.
   * @param waking True if the alert is waking.
   * @return pw::OkStatus() on success, otherwise:
   *   - pw::Status::NotFound() if the data flow or endpoints are not found.
   *   - pw::Status::Internal() if the alert could not be sent.
   */
  using SendAlertFn = std::function<pw::Status(DataFlowId, EndpointId, bool)>;

  /** Entry in the list of data flows associated with a pruned endpoint. */
  struct PrunedEndpointDataFlowEntry {
    DataFlowId dataFlowId;
    std::vector<EndpointId> endpoints;
    bool isSource;

    PrunedEndpointDataFlowEntry(DataFlowId dataFlowId, bool isSource)
        : dataFlowId(dataFlowId), isSource(isSource) {}
  };

  DataFlowManager(const std::shared_ptr<RegionAllocator> &regionAllocator,
                  const std::shared_ptr<WakelockManager> &wakelockManager,
                  SendAlertFn sendAlertFn);
  virtual ~DataFlowManager() = default;

  /**
   * Allocates a shared data region for a data flow with a host source.
   *
   * @param hubId The ID of the hub that requested the allocation.
   * @param requirements The requirements for the shared data region.
   * @return On success, the allocated shared data region, otherwise:
   *   - pw::Status::Unimplemented() if a region with the desired properties
   *     cannot be created (e.g. there is no shared memory region that can be
   *     accessed by all of the necessary hubs).
   *   - pw::Status::ResourceExhausted() if there wasn't enough memory to
   *     allocate the region.
   */
  pw::Result<SharedDataRegion> allocateRegion(
      int64_t hubId, const SharedDataRegionRequirements &requirements)
      EXCLUDES(mLock);

  /**
   * Releases a shared data region allocated via allocateRegion().
   *
   * @param hubId The ID of the hub that owns the region.
   * @param regionId The ID of the region to release.
   * @return pw::OkStatus() on success, otherwise:
   *   - pw::Status::NotFound() if the region is not found.
   *   - pw::Status::FailedPrecondition() if the region is still in use.
   */
  pw::Status releaseRegion(int64_t hubId, int32_t regionId) EXCLUDES(mLock);

  /**
   * Initializes the state for a new data flow with a host source.
   *
   * @param source The source endpoint of the data flow.
   * @param info The data flow information.
   * @return pw::OkStatus() on success, otherwise:
   *   - pw::Status::AlreadyExists() if the id is already in use.
   *   - pw::Status::NotFound() if the region indicated in the info doesn't
   *     exist.
   *   - pw::Status::Internal() if the data flow could not be set up (e.g.
   *     failed to set up alert handling).
   */
  pw::Result<DataFlowId> addHostSourceDataFlow(EndpointId source,
                                               const DataFlowInfo &info)
      EXCLUDES(mLock);

  /**
   * Initializes the state for a offload sink a host source data flow.
   *
   * @param params The parameters for the offload sink registration.
   * @return on success, a shallow copy of the previously registered
   * DataFlowInfo (i.e. without the region fd) and a SharedDataRegion for the
   * allocation of the sink metadata, otherwise:
   *   - pw::Status::NotFound() if the data flow is not found.
   *   - pw::Status::AlreadyExists() if the sink is already registered.
   *   - pw::Status::Internal() if the data flow could not be set up (e.g.
   *     failed to set up alert handling).
   */
  pw::Result<std::pair<DataFlowInfo, SharedDataRegion>> addOffloadSink(
      const DataFlowSinkRegistrationParams &params) EXCLUDES(mLock);

  /**
   * Initializes the state for a host sink on a data flow.
   *
   * If the data flow hasn't previously been shared with a host endpoint,
   * initializes epoll events for its source.
   *
   * @param dataFlowId The ID of the data flow.
   * @param source The source endpoint of the data flow.
   * @param sink The sink endpoint of the data flow.
   * @param primaryRegionId The ID of the primary shared data region.
   * @param sinkMetadataRegionId The ID of the shared data region for sink
   *     metadata.
   * @param metadataOffset The offset of the metadata in the primary shared
   *     data region.
   * @param sinkMetadataOffset The offset of the metadata in the sink metadata
   *     shared data region.
   * @return pw::OkStatus() on success, otherwise:
   *   - pw::Status::NotFound() if the data flow or regions are not found.
   *   - pw::Status::InvalidArgument() if the data flow id and source id do not
   *     match, or if the data flow id and primary region id do not match. This
   *     will only happen if the data flow already exists.
   *   - pw::Status::AlreadyExists() if the sink is already registered.
   *   - pw::Status::Internal() if the data flow could not be set up (e.g.
   *     failed to set up alert handling).
   */
  pw::Result<DataFlowSinkContext> addHostSink(
      DataFlowId dataFlowId, EndpointId source, EndpointId sink,
      int32_t primaryRegionId, int32_t sinkMetadataRegionId,
      uint32_t metadataOffset, uint32_t sinkMetadataOffset) EXCLUDES(mLock);

  /**
   * Verifies that the given endpoint is present on the given data flow.
   *
   * @param dataFlowId The ID of the data flow.
   * @param endpointId The ID of the endpoint.
   * @param isHost True if the endpoint is a host endpoint.
   * @return pw::OkStatus() on success, otherwise:
   *   - pw::Status::NotFound() if the data flow or endpoint is not found.
   */
  pw::Status verifyEndpointOnDataFlow(DataFlowId dataFlowId,
                                      EndpointId endpointId, bool isHost)
      EXCLUDES(mLock);

  /**
   * Sends an alert to one or more host endpoints on a data flow.
   *
   * @param dataFlowId The ID of the data flow.
   * @param endpointIds The IDs of the endpoints to receive the alert.
   * @param isWaking True if the alert is waking.
   * @return pw::OkStatus() on success, otherwise:
   *   - pw::Status::NotFound() if the data flow or endpoint is not found.
   *   - pw::Status::InvalidArgument() if the endpoint is not a host endpoint.
   */
  pw::Status alertHostEndpoints(DataFlowId dataFlowId,
                                const std::vector<EndpointId> &endpointIds,
                                bool isWaking) EXCLUDES(mLock);

  /**
   * Removes a data flow, releasing any associated resources.
   *
   * @param id The ID of the data flow to clear state for.
   * @return the list of endpoints to notify, otherwise:
   *   - pw::Status::NotFound() if the data flow is not found.
   */
  pw::Result<std::vector<EndpointId>> removeDataFlow(DataFlowId id)
      EXCLUDES(mLock);

  /**
   * Removes a sink from a data flow, releasing any associated resources.
   *
   * This may result in the removal of state associated with the data flow if it
   * has an offload source and no more host sinks.
   *
   * @param dataFlowId The data flow to remove the sink from.
   * @param sink The sink endpoint to remove.
   * @return the source endpoint of the data flow, otherwise:
   *   - pw::Status::NotFound() if the data flow or sink is not found on the
   *     data flow.
   */
  pw::Result<EndpointId> removeSink(DataFlowId dataFlowId, EndpointId sink)
      EXCLUDES(mLock);

  /**
   * Removes all state associated with the given endpoint.
   *
   * @param endpointId The endpoint to remove.
   * @return the list of associated data flows for which endpoints should be
   * notified, otherwise:
   *   - pw::Status::NotFound() if the endpoint is not found.
   */
  pw::Result<std::vector<PrunedEndpointDataFlowEntry>> pruneEndpoint(
      EndpointId endpointId) EXCLUDES(mLock);

 protected:
  // Data associated with a host hub.
  struct HostHubData {
    std::unordered_map<int32_t, size_t> regionToUseCount;
    int32_t nextDataFlowId = 1;
  };

  // Data associated with a data flow.
  struct DataFlow {
    DataFlowInfo info;
    EndpointId source;
    std::set<EndpointId> sinks;
    DataFlowId id;
    bool isHostSource;

    DataFlow(DataFlowId _id, EndpointId _source, const DataFlowInfo &_info,
             bool _isHostSource);
  };

  // Data associated with an endpoint.
  struct Endpoint {
    // Map of data flow id to the pair of sink metadata region id and ref count.
    // Only relevant for endpoints that are offload sinks.
    using MetadataRegionMap = std::map<DataFlowId, std::pair<int32_t, size_t>>;
    // Map of data flow id to alert fds and outstanding wake count. Only
    // relevant for endpoints on the host.
    using AlertFdAndWakeCountMap =
        std::map<DataFlowId, std::pair<DataFlowAlertFds, uint64_t>>;

    std::unordered_set<DataFlow *> dataFlows;
    std::variant<MetadataRegionMap, AlertFdAndWakeCountMap> map;
    bool isHost;

    // Constructor for host endpoints.
    Endpoint(DataFlow *dataFlow, DataFlowAlertFds alertFds)
        : dataFlows{dataFlow}, isHost(true) {
      map.emplace<AlertFdAndWakeCountMap>().emplace(
          std::piecewise_construct, std::forward_as_tuple(dataFlow->id),
          std::forward_as_tuple(std::move(alertFds), 0));
    }

    // Constructor for offload endpoints.
    Endpoint(DataFlow *dataFlow) : dataFlows{dataFlow}, isHost(false) {
      map.emplace<MetadataRegionMap>();
    }
  };

  // Convenience type for the map of data flows.
  using DataFlowMap = std::map<DataFlowId, std::unique_ptr<DataFlow>>;
  // Convenience type for the map of endpoints.
  using EndpointMap = std::map<EndpointId, Endpoint>;

  // DataFlowEpollWaiter::Callback interface
  void onAlert(DataFlowId dataFlowId, EndpointId endpointId,
               bool waking) override;
  void onWakingAck(DataFlowId dataFlowId, EndpointId endpointId,
                   uint64_t wakeCount) override;

  // Adds an offload source data flow to the map, returning an iterator to it.
  pw::Result<DataFlowMap::iterator> addOffloadSourceDataFlowLocked(
      DataFlowId dataFlowId, EndpointId source, DataFlowInfo &info)
      REQUIRES(mLock);

  // Implementation of removeDataFlow() that assumes the lock is held.
  pw::Result<std::vector<EndpointId>> removeDataFlowLocked(
      DataFlowMap::iterator it) REQUIRES(mLock);

  // Removes a sink from a data flow, releasing any associated resources. This
  // may result in the removal of an offload source data flow that has no more
  // host sinks.
  pw::Result<EndpointId> removeSinkLocked(DataFlowMap::iterator dataFlowIt,
                                          EndpointMap::iterator sinkIt)
      REQUIRES(mLock);

  // Removes the association between the given endpoint and data flow. If the
  // endpoint has no more data flows, removes it from the map.
  void removeEndpointDataFlowAssociationLocked(EndpointMap::iterator endpointIt,
                                               DataFlow *dataFlow)
      REQUIRES(mLock);

  // Allocates or retrieves the metadata region for an offload sink.
  pw::Result<SharedDataRegion> getOffloadSinkMetadataRegionLocked(
      EndpointId sinkId, DataFlow *dataFlow, Endpoint &sink) REQUIRES(mLock);

  // Releases any resources associated with an endpoint on a data flow.
  void releaseEndpointResourcesLocked(EndpointMap::iterator endpointIt,
                                      DataFlow *dataFlow) REQUIRES(mLock);

  // Removes the reference to the metadata region for the given data flow,
  // releasing it if there are no more references.
  void unlinkOffloadSinkMetadataRegionLocked(Endpoint &sink, DataFlow *dataFlow)
      REQUIRES(mLock);

  // Looks up and decrements the use count for a host allocated region.
  void decrementHostRegionUseCountLocked(DataFlowId dataFlowId,
                                         int32_t regionId) REQUIRES(mLock);

  // Looks up a data flow associated with an endpoint for callback handling.
  pw::Result<std::pair<DataFlow *, Endpoint *>> lookupDataFlowAndEndpointLocked(
      DataFlowId dataFlowId, EndpointId endpointId) REQUIRES(mLock);

  // Looks up an endpoint in the map. Fatal if not found.
  EndpointMap::iterator getEndpointLocked(EndpointId endpointId)
      REQUIRES(mLock);

  // Retrieves the alert fd and wake count for an endpoint on a data flow.
  // Fatal if not found.
  std::pair<DataFlowAlertFds, uint64_t> &getAlertFdsAndWakeCountMapLocked(
      EndpointId endpointId, Endpoint &endpoint, DataFlowId dataFlowId)
      REQUIRES(mLock);

  // Increases the wake count for a host endpoint on a data flow. Only returns
  // true and updates the wake count if the wakelock was successfully taken.
  bool incrementWakeCountLocked(EndpointId endpointId, DataFlowId dataFlowId,
                                uint64_t &wakeCount) REQUIRES(mLock);

  // Decreases the wake count for a host endpoint on a data flow. Only updates
  // the wake count if the wakelock was successfully released.
  void decreaseWakeCountLocked(EndpointId endpointId, DataFlowId dataFlowId,
                               uint64_t &wakeCount, size_t decrease)
      REQUIRES(mLock);

  std::mutex mLock;

  // Members set at construction.
  std::shared_ptr<RegionAllocator> mRegionAllocator GUARDED_BY(mLock);
  std::shared_ptr<WakelockManager> mWakelockManager;
  std::unique_ptr<DataFlowEpollWaiter> mEpollWaiter GUARDED_BY(mLock);
  SendAlertFn mSendAlertFn;

  // Map of host hub allocated regions and their use counts.
  std::unordered_map<int64_t, HostHubData> mIdToHostHubData GUARDED_BY(mLock);
  // Map of all host endpoint associated data flows.
  DataFlowMap mIdToDataFlow GUARDED_BY(mLock);
  // Map of endpoint id to associated data.
  EndpointMap mIdToEndpoint GUARDED_BY(mLock);
};

}  // namespace android::hardware::contexthub::common::implementation