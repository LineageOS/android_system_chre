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

#define LOG_TAG "CHRE.DataFlowManager"

#include "data_flow_manager.h"

#include <unistd.h>

#include <cinttypes>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <utils/Log.h>

#include "data_flow_epoll_waiter.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::hardware::contexthub::common::implementation {

DataFlowManager::DataFlowManager(
    const std::shared_ptr<RegionAllocator> &regionAllocator,
    const std::shared_ptr<WakelockManager> &wakelockManager,
    SendAlertFn sendAlertFn)
    : mRegionAllocator(regionAllocator),
      mWakelockManager(wakelockManager),
      mSendAlertFn(sendAlertFn) {
  auto epollWaiter = DataFlowEpollWaiterReal::create(*this);
  if (!epollWaiter.ok()) {
    LOG_ALWAYS_FATAL("Failed to create DataFlowEpollWaiter: %d",
                     epollWaiter.status().code());
  }
  mEpollWaiter = std::move(*epollWaiter);
}

pw::Result<SharedDataRegion> DataFlowManager::allocateRegion(
    int64_t hubId, const SharedDataRegionRequirements &requirements) {
  std::lock_guard lock(mLock);
  PW_TRY_ASSIGN(auto region, mRegionAllocator->allocateRegion(requirements));
  ALOGI("Allocated region %" PRId32 " for hub %" PRIx64, region.id, hubId);
  // Initialize the use count to 0.
  mIdToHostHubData[hubId].regionToUseCount[region.id] = 0;
  return region;
}

pw::Status DataFlowManager::releaseRegion(int64_t hubId, int32_t regionId) {
  std::lock_guard lock(mLock);
  auto it = mIdToHostHubData.find(hubId);
  if (it == mIdToHostHubData.end()) {
    ALOGE("Hub %" PRIx64 " has no allocated regions", hubId);
    return pw::Status::NotFound();
  }
  auto regionIt = it->second.regionToUseCount.find(regionId);
  if (regionIt == it->second.regionToUseCount.end()) {
    ALOGE("Region %" PRId32 " not found for hub %" PRIx64, regionId, hubId);
    return pw::Status::NotFound();
  }
  if (regionIt->second > 0) {
    ALOGE("Region %" PRId32 " is still in use by hub %" PRIx64, regionId, hubId);
    return pw::Status::FailedPrecondition();
  }
  it->second.regionToUseCount.erase(regionIt);
  if (it->second.regionToUseCount.empty()) {
    mIdToHostHubData.erase(it);
  }
  return mRegionAllocator->releaseRegion(regionId);
}

pw::Result<DataFlowId> DataFlowManager::addHostSourceDataFlow(
    EndpointId source, const DataFlowInfo &info) {
  std::lock_guard lock(mLock);
  auto hubIt = mIdToHostHubData.find(source.hubId);
  if (hubIt == mIdToHostHubData.end()) {
    ALOGE("Hub %" PRIx64 " has no allocated regions", source.hubId);
    return pw::Status::NotFound();
  }
  auto regionIt = hubIt->second.regionToUseCount.find(info.region.id);
  if (regionIt == hubIt->second.regionToUseCount.end()) {
    ALOGE("Region %" PRId32 " not found for hub %" PRIx64, info.region.id,
          source.hubId);
    return pw::Status::NotFound();
  }
  DataFlowId id = {.hubId = source.hubId, .id = hubIt->second.nextDataFlowId};
  PW_TRY(mEpollWaiter->addTriggers(id, source, info.alertFds));
  hubIt->second.nextDataFlowId++;
  regionIt->second++;
  auto &dataFlow = mIdToDataFlow[id] =
      std::make_unique<DataFlow>(id, source, info, /* isHostSource= */ true);
  mEndpointToDataFlows[source].insert(dataFlow.get());
  return id;
}

pw::Result<std::pair<DataFlowInfo, SharedDataRegion>>
DataFlowManager::addOffloadSink(const DataFlowSinkRegistrationParams &params) {
  std::lock_guard lock(mLock);
  const auto &dataFlowId = params.context.id;
  auto dataFlowIt = mIdToDataFlow.find(dataFlowId);
  if (dataFlowIt == mIdToDataFlow.end()) {
    ALOGE("Data flow (%" PRIx64 ", %" PRId32 ") not found", dataFlowId.hubId,
          dataFlowId.id);
    return pw::Status::NotFound();
  }
  auto &dataFlow = dataFlowIt->second;
  if (dataFlow->source != params.sourceId) {
    ALOGE("Source id mismatch for data flow (%" PRIx64 ", %" PRId32 ")",
          dataFlowId.hubId, dataFlowId.id);
    return pw::Status::InvalidArgument();
  }
  if (dataFlow->sinks.contains(params.sinkId)) {
    ALOGE("Sink (%" PRIx64 ", %" PRIx64
          ") already registered on data flow (%" PRIx64 ", %" PRId32 ")",
          params.sinkId.hubId, params.sinkId.id, dataFlowId.hubId,
          dataFlowId.id);
    return pw::Status::AlreadyExists();
  }
  PW_TRY(mEpollWaiter->addTriggers(dataFlowId, params.sinkId,
                                   params.context.alertFds));
  PW_TRY_ASSIGN(
      auto region,
      getOffloadSinkMetadataRegionLocked(params.sinkId, dataFlow.get())
          .or_else([this, &dataFlowId, &params](pw::Status status) {
            mEpollWaiter->removeTriggers(dataFlowId, params.sinkId)
                .IgnoreError();
            return status;
          }));
  dataFlow->sinks.insert(params.sinkId);
  mEndpointToDataFlows[params.sinkId].insert(dataFlow.get());
  return std::make_pair(shallowCopyDataFlowInfo(dataFlow->info),
                        std::move(region));
}

pw::Result<DataFlowSinkContext> DataFlowManager::addHostSink(
    DataFlowId /* dataFlow */, EndpointId /* source */, EndpointId /* sink */,
    int32_t /* primaryRegionId */, int32_t /* sinkMetadataRegionId */,
    uint32_t /* metadataOffset */, uint32_t /* sinkMetadataOffset */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

pw::Result<std::vector<EndpointId>> DataFlowManager::removeDataFlow(
    DataFlowId id) {
  std::lock_guard lock(mLock);
  auto it = mIdToDataFlow.find(id);
  if (it == mIdToDataFlow.end()) {
    ALOGE("Data flow (%" PRIx64 ", %" PRId32 ") not found", id.hubId, id.id);
    return pw::Status::NotFound();
  }
  if (it->second->isHostSource) {
    decrementHostRegionUseCountLocked(id, it->second->info.region.id);
  }
  auto &dataFlow = it->second;
  removeEndpointDataFlowAssociationLocked(dataFlow->source, dataFlow.get());
  std::vector<EndpointId> endpointsToNotify;
  for (const auto &sink : dataFlow->sinks) {
    if (dataFlow->isHostSource) {
      unlinkOffloadSinkMetadataRegionLocked(sink, dataFlow.get());
    }
    endpointsToNotify.push_back(sink);
    removeEndpointDataFlowAssociationLocked(sink, dataFlow.get());
  }
  if (auto status = mEpollWaiter->removeTriggers(id, /* endpointId= */ {});
      !status.ok()) {
    ALOGE("Failed to remove triggers for data flow (%" PRIx64 ", %" PRId32
          ") with %d",
          id.hubId, id.id, status.code());
  }
  mIdToDataFlow.erase(it);
  return endpointsToNotify;
}

pw::Result<EndpointId> DataFlowManager::removeSink(DataFlowId /* dataFlow */,
                                                   EndpointId /* sink */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

pw::Result<std::vector<DataFlowManager::PrunedEndpointDataFlowEntry>>
DataFlowManager::pruneEndpoint(EndpointId /* endpoint */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

void DataFlowManager::onAlert(DataFlowId /* dataFlowId */,
                              EndpointId /* endpointId */, bool /* waking */) {
  // TODO(b/463163051): Implement this.
}

void DataFlowManager::onWakingAck(DataFlowId /* dataFlowId */,
                                  EndpointId /* endpointId */,
                                  uint64_t /* wakeCount */) {
  // TODO(b/463163051): Implement this.
}

void DataFlowManager::removeEndpointDataFlowAssociationLocked(
    EndpointId endpoint, DataFlow *dataFlow) {
  auto endpointFlowsIt = mEndpointToDataFlows.find(endpoint);
  if (endpointFlowsIt != mEndpointToDataFlows.end()) {
    endpointFlowsIt->second.erase(dataFlow);
    if (endpointFlowsIt->second.empty()) {
      mEndpointToDataFlows.erase(endpointFlowsIt);
    }
  }
}

pw::Result<SharedDataRegion>
DataFlowManager::getOffloadSinkMetadataRegionLocked(EndpointId sinkId,
                                                    DataFlow *dataFlow) {
  SharedDataRegion region;
  if (mRegionAllocator->consumerRequiresSeparateRegion(sinkId.hubId)) {
    auto &dataFlowToMetadataRegionIdAndRefCount =
        mOffloadSinkToDataFlowToMetadataRegionIdAndRefCount[sinkId];
    auto it = dataFlowToMetadataRegionIdAndRefCount.find(dataFlow->id);
    if (it != dataFlowToMetadataRegionIdAndRefCount.end()) {
      PW_TRY_ASSIGN(region, mRegionAllocator->getRegionInfo(it->second.first));
      it->second.second++;
    } else {
      // Allocate a region of page size for sink metadata. This should be
      // sufficient given that this region is only used for sink metadata with
      // data flows that have the same source and this specific sink.
      PW_TRY_ASSIGN(
          region,
          mRegionAllocator->allocateRegion(SharedDataRegionRequirements{
              .sizeBytes = getpagesize(), .targetHubIds = {sinkId.hubId}}));
      dataFlowToMetadataRegionIdAndRefCount[dataFlow->id] = {region.id, 1};
    }
  } else {
    PW_TRY_ASSIGN(region,
                  mRegionAllocator->getRegionInfo(dataFlow->info.region.id));
  }
  return region;
}

void DataFlowManager::unlinkOffloadSinkMetadataRegionLocked(
    EndpointId sinkId, DataFlow *dataFlow) {
  if (mRegionAllocator->consumerRequiresSeparateRegion(sinkId.hubId)) {
    // Reduce the ref count on the sink's metadata region, releasing it if
    // there are no more references.
    auto dataFlowsIt =
        mOffloadSinkToDataFlowToMetadataRegionIdAndRefCount.find(sinkId);
    if (dataFlowsIt !=
        mOffloadSinkToDataFlowToMetadataRegionIdAndRefCount.end()) {
      auto regionIt = dataFlowsIt->second.find(dataFlow->id);
      if (regionIt != dataFlowsIt->second.end()) {
        regionIt->second.second--;
        if (regionIt->second.second == 0) {
          auto status = mRegionAllocator->releaseRegion(regionIt->second.first);
          if (!status.ok()) {
            ALOGE("Failed to release sink metadata region %" PRId32
                  " for data flow (%" PRIx64 ", %" PRId32 "): %d",
                  regionIt->second.first, dataFlow->id.hubId, dataFlow->id.id,
                  status.code());
          }
          dataFlowsIt->second.erase(regionIt);
        }
      }
      if (dataFlowsIt->second.empty()) {
        mOffloadSinkToDataFlowToMetadataRegionIdAndRefCount.erase(dataFlowsIt);
      }
    }
  }
}

void DataFlowManager::decrementHostRegionUseCountLocked(DataFlowId dataFlowId,
                                                        int32_t regionId) {
  auto hubIt = mIdToHostHubData.find(dataFlowId.hubId);
  if (hubIt == mIdToHostHubData.end()) {
    LOG_ALWAYS_FATAL("Hub %" PRIx64 " has no allocated regions",
                     dataFlowId.hubId);
  }
  auto regionIt = hubIt->second.regionToUseCount.find(regionId);
  if (regionIt == hubIt->second.regionToUseCount.end()) {
    LOG_ALWAYS_FATAL("Region %" PRId32 " not found for hub %" PRIx64, regionId,
                     dataFlowId.hubId);
  }
  regionIt->second--;
}

}  // namespace android::hardware::contexthub::common::implementation