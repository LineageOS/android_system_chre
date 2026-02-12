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
#include <functional>
#include <memory>
#include <vector>

#include <aidl/android/hardware/contexthub/DataFlowId.h>
#include <aidl/android/hardware/contexthub/DataFlowInfo.h>
#include <aidl/android/hardware/contexthub/DataFlowSinkContext.h>
#include <aidl/android/hardware/contexthub/DataFlowSinkRegistrationParams.h>
#include <aidl/android/hardware/contexthub/EndpointId.h>
#include <aidl/android/hardware/contexthub/SharedDataRegion.h>
#include <aidl/android/hardware/contexthub/SharedDataRegionRequirements.h>

#include "data_flow_epoll_waiter.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "region_allocator.h"
#include "wakelock_manager.h"

namespace android::hardware::contexthub::common::implementation {

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
   * @param sender The endpoint that sent the alert.
   * @param receiver The endpoint that should receive the alert.
   * @param waking True if the alert is waking.
   * @return pw::OkStatus() on success, otherwise:
   *   - pw::Status::NotFound() if the data flow or endpoints are not found.
   *   - pw::Status::Internal() if the alert could not be sent.
   */
  using SendAlertFn =
      std::function<pw::Status(DataFlowId, EndpointId, EndpointId, bool)>;

  /** Entry in the list of data flows associated with a pruned endpoint. */
  struct PrunedEndpointDataFlowEntry {
    DataFlowId dataFlowId;
    std::vector<EndpointId> endpoints;
    bool isSource;
  };

  DataFlowManager(const std::shared_ptr<RegionAllocator> &regionAllocator,
                  const std::shared_ptr<WakelockManager> &wakelockManager,
                  SendAlertFn sendAlertFn);
  virtual ~DataFlowManager() = default;

  /**
   * Allocates a shared data region for a data flow with a host source.
   *
   * @param requirements The requirements for the shared data region.
   * @return The allocated shared data region.
   */
  pw::Result<SharedDataRegion> allocateRegion(
      const SharedDataRegionRequirements &requirements);

  /**
   * Releases a shared data region allocated via allocateRegion().
   *
   * @param region The ID of the region to release.
   * @return pw::OkStatus() on success, otherwise:
   *   - pw::Status::NotFound() if the region is not found.
   *   - pw::Status::FailedPrecondition() if the region is still in use.
   *   - pw::Status::PermissionDenied() if the region was not allocated via
   *     allocateRegion().
   */
  pw::Status releaseRegion(int32_t region);

  /**
   * Initializes the state for a new data flow with a host source.
   *
   * @param source The source endpoint of the data flow.
   * @param id The ID to assign to the data flow.
   * @param info The data flow information.
   * @return pw::OkStatus() on success, otherwise:
   *   - pw::Status::AlreadyExists() if the id is already in use.
   *   - pw::Status::NotFound() if the region indicated in the info doesn't
   *     exist.
   *   - pw::Status::PermissionDenied() if the region indicated in the info
   *     isn't attributed to the source endpoint.
   *   - pw::Status::Internal() if the data flow could not be set up (e.g.
   *     failed to set up alert handling).
   */
  pw::Status addHostSourceDataFlow(EndpointId source, DataFlowId id,
                                   const DataFlowInfo &info);

  /**
   * Initializes the state for a offload sink a host source data flow.
   *
   * @param params The parameters for the offload sink registration.
   * @return on success, the previously registered DataFlowInfo and, if
   * necessary, a separate region for the allocation of the sink metadata,
   * otherwise:
   *   - pw::Status::NotFound() if the data flow is not found.
   *   - pw::Status::AlreadyExists() if the sink is already registered.
   *   - pw::Status::Internal() if the data flow could not be set up (e.g.
   *     failed to set up alert handling).
   */
  pw::Result<std::pair<DataFlowInfo, std::optional<SharedDataRegion>>>
  addOffloadSink(DataFlowSinkRegistrationParams params);

  /**
   * Initializes the state for a host sink on a data flow.
   *
   * If the data flow hasn't previously been shared with a host endpoint,
   * initializes epoll events for its source.
   *
   * @param dataFlow The ID of the data flow.
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
      DataFlowId dataFlow, EndpointId source, EndpointId sink,
      int32_t primaryRegionId, int32_t sinkMetadataRegionId,
      uint32_t metadataOffset, uint32_t sinkMetadataOffset);

  /**
   * Removes a data flow, releasing any associated resources.
   *
   * @param id The ID of the data flow to clear state for.
   * @return the list of endpoints to notify, otherwise:
   *   - pw::Status::NotFound() if the data flow is not found.
   */
  pw::Result<std::vector<EndpointId>> removeDataFlow(DataFlowId id);

  /**
   * Removes a sink from a data flow, releasing any associated resources.
   *
   * @param dataFlow The data flow to remove the sink from.
   * @param sink The sink endpoint to remove.
   * @return the source endpoint of the data flow, otherwise:
   *   - pw::Status::NotFound() if the data flow or sink is not found.
   *   - pw::Status::InvalidArgument() if the endpoint is not a sink on the
   *     data flow.
   */
  pw::Result<EndpointId> removeSink(DataFlowId dataFlow, EndpointId sink);

  /**
   * Removes all state associated with the given endpoint.
   *
   * @param endpoint The endpoint to remove.
   * @return the list of associated data flows for which endpoints should be
   * notified, otherwise:
   *   - pw::Status::NotFound() if the endpoint is not found.
   */
  pw::Result<std::vector<PrunedEndpointDataFlowEntry>> pruneEndpoint(
      EndpointId endpoint);

 protected:
  // DataFlowEpollWaiter::Callback interface
  void onAlert(DataFlowId dataFlowId, EndpointId endpointId,
               bool waking) override;
  void onWakingAck(DataFlowId dataFlowId, EndpointId endpointId,
                   uint64_t wakeCount) override;
};

}  // namespace android::hardware::contexthub::common::implementation