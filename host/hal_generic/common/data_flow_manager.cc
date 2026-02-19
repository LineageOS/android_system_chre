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

#include <sys/eventfd.h>
#include <unistd.h>

#include <cinttypes>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <aidl/android/hardware/contexthub/DataFlowAlertFds.h>
#include <utils/Log.h>

#include "android/binder_auto_utils.h"
#include "data_flow_epoll_waiter.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::hardware::contexthub::common::implementation {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowAlertFds;

pw::Result<DataFlowAlertFds> createAlertFds(bool isHostEndpoint) {
  DataFlowAlertFds fds{
      .waking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK)),
      .nonWaking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK)),
      .halAck = ndk::ScopedFileDescriptor(
          isHostEndpoint ? eventfd(0, EFD_NONBLOCK) : -1)};
  if (fds.waking.get() < 0 || fds.nonWaking.get() < 0 ||
      (isHostEndpoint && fds.halAck.get() < 0)) {
    ALOGE("Failed to create alert fds");
    return pw::Status::Internal();
  }
  return fds;
}

DataFlowAlertFds dupAlertFds(const DataFlowAlertFds &fds, bool isHostEndpoint) {
  DataFlowAlertFds dupFds{
      .waking =
          ndk::ScopedFileDescriptor(TEMP_FAILURE_RETRY(dup(fds.waking.get()))),
      .nonWaking = ndk::ScopedFileDescriptor(
          TEMP_FAILURE_RETRY(dup(fds.nonWaking.get()))),
      .halAck = isHostEndpoint ? ndk::ScopedFileDescriptor(
                                     TEMP_FAILURE_RETRY(dup(fds.halAck.get())))
                               : ndk::ScopedFileDescriptor(-1)};
  if (dupFds.waking.get() < 0 || dupFds.nonWaking.get() < 0 ||
      (isHostEndpoint && dupFds.halAck.get() < 0)) {
    LOG_ALWAYS_FATAL("Failed to dup alert fds");
  }
  return dupFds;
}

}  // namespace

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
  ALOGI("Allocated region %" PRId32 " for hub 0x%" PRIx64, region.id, hubId);
  // Initialize the use count to 0.
  mIdToHostHubData[hubId].regionToUseCount[region.id] = 0;
  return region;
}

pw::Status DataFlowManager::releaseRegion(int64_t hubId, int32_t regionId) {
  std::lock_guard lock(mLock);
  auto it = mIdToHostHubData.find(hubId);
  if (it == mIdToHostHubData.end()) {
    ALOGE("Hub 0x%" PRIx64 " has no allocated regions", hubId);
    return pw::Status::NotFound();
  }
  auto regionIt = it->second.regionToUseCount.find(regionId);
  if (regionIt == it->second.regionToUseCount.end()) {
    ALOGE("Region %" PRId32 " not found for hub 0x%" PRIx64, regionId, hubId);
    return pw::Status::NotFound();
  }
  if (regionIt->second > 0) {
    ALOGE("Region %" PRId32 " is still in use by hub 0x%" PRIx64, regionId,
          hubId);
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
    ALOGE("Hub 0x%" PRIx64 " has no allocated regions", source.hubId);
    return pw::Status::NotFound();
  }
  auto regionIt = hubIt->second.regionToUseCount.find(info.region.id);
  if (regionIt == hubIt->second.regionToUseCount.end()) {
    ALOGE("Region %" PRId32 " not found for hub 0x%" PRIx64, info.region.id,
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
    ALOGE("Data flow (0x%" PRIx64 ", %" PRId32 ") not found", dataFlowId.hubId,
          dataFlowId.id);
    return pw::Status::NotFound();
  }
  auto &dataFlow = dataFlowIt->second;
  if (dataFlow->source != params.sourceId) {
    ALOGE("Source id mismatch for data flow (0x%" PRIx64 ", %" PRId32 ")",
          dataFlowId.hubId, dataFlowId.id);
    return pw::Status::InvalidArgument();
  }
  if (dataFlow->sinks.contains(params.sinkId)) {
    ALOGE("Sink (0x%" PRIx64 ", 0x%" PRIx64
          ") already registered on data flow (0x%" PRIx64 ", %" PRId32 ")",
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
  return std::make_pair(
      DataFlowInfo{.region = {.id = dataFlow->info.region.id},
                   .metadataOffsetBytes = dataFlow->info.metadataOffsetBytes},
      std::move(region));
}

pw::Result<DataFlowSinkContext> DataFlowManager::addHostSink(
    DataFlowId dataFlowId, EndpointId source, EndpointId sink,
    int32_t primaryRegionId, int32_t sinkMetadataRegionId,
    uint32_t metadataOffset, uint32_t sinkMetadataOffset) {
  std::lock_guard lock(mLock);
  PW_TRY_ASSIGN(auto primaryRegion,
                mRegionAllocator->getRegionInfo(primaryRegionId));
  PW_TRY_ASSIGN(auto sinkMetadataRegion,
                mRegionAllocator->getRegionInfo(sinkMetadataRegionId));
  DataFlowSinkContext context = {
      .id = dataFlowId,
      .sinkMetadataRegion = std::move(sinkMetadataRegion),
      .metadataOffsetBytes = sinkMetadataOffset};
  PW_TRY_ASSIGN(context.alertFds, createAlertFds(/* isHostEndpoint= */ true));
  context.info = DataFlowInfo{.region = std::move(primaryRegion),
                              .metadataOffsetBytes = metadataOffset};
  auto dataFlowIt = mIdToDataFlow.find(dataFlowId);
  if (dataFlowIt == mIdToDataFlow.end()) {
    PW_TRY_ASSIGN(dataFlowIt, addOffloadSourceDataFlowLocked(dataFlowId, source,
                                                             *context.info));
  } else {
    if (source != dataFlowIt->second->source) {
      ALOGE("Source id mismatch for data flow (0x%" PRIx64 ", %" PRId32 ")",
            dataFlowId.hubId, dataFlowId.id);
      return pw::Status::AlreadyExists();
    }
    if (dataFlowIt->second->sinks.contains(sink)) {
      ALOGE("Sink (0x%" PRIx64 ", 0x%" PRIx64
            ") already registered on data flow (0x%" PRIx64 ", %" PRId32 ")",
            sink.hubId, sink.id, dataFlowId.hubId, dataFlowId.id);
      return pw::Status::AlreadyExists();
    }
    if (dataFlowIt->second->info.metadataOffsetBytes != metadataOffset) {
      ALOGE("Metadata offset mismatch for data flow (0x%" PRIx64 ", %" PRId32
            ")",
            dataFlowId.hubId, dataFlowId.id);
      return pw::Status::AlreadyExists();
    }
    context.info->alertFds = dupAlertFds(dataFlowIt->second->info.alertFds,
                                         /* isHostEndpoint= */ false);
  }
  auto &dataFlow = *dataFlowIt->second;
  auto status = mEpollWaiter->addTriggers(dataFlowId, sink, context.alertFds);
  if (!status.ok()) {
    if (dataFlow.sinks.empty()) {
      // Clear the state for the new data flow as there are no host sinks.
      removeDataFlowLocked(dataFlowIt).IgnoreError();
    }
    return status;
  }
  dataFlow.sinks.insert(sink);
  mEndpointToDataFlows[sink].insert(&dataFlow);
  return context;
}

pw::Result<std::vector<EndpointId>> DataFlowManager::removeDataFlow(
    DataFlowId id) {
  std::lock_guard lock(mLock);
  auto it = mIdToDataFlow.find(id);
  if (it == mIdToDataFlow.end()) {
    ALOGE("Data flow (0x%" PRIx64 ", %" PRId32 ") not found", id.hubId, id.id);
    return pw::Status::NotFound();
  }
  return removeDataFlowLocked(it);
}

pw::Result<EndpointId> DataFlowManager::removeSink(DataFlowId dataFlowId,
                                                   EndpointId sink) {
  std::lock_guard lock(mLock);
  auto dataFlowIt = mIdToDataFlow.find(dataFlowId);
  if (dataFlowIt == mIdToDataFlow.end()) {
    ALOGE("Data flow (0x%" PRIx64 ", %" PRId32 ") not found", dataFlowId.hubId,
          dataFlowId.id);
    return pw::Status::NotFound();
  }
  if (!dataFlowIt->second->sinks.contains(sink)) {
    ALOGE("Sink (0x%" PRIx64 ", 0x%" PRIx64
          ") not found on data flow (0x%" PRIx64 ", %" PRId32 ")",
          sink.hubId, sink.id, dataFlowId.hubId, dataFlowId.id);
    return pw::Status::NotFound();
  }
  return removeSinkLocked(dataFlowIt, sink);
}

pw::Result<std::vector<DataFlowManager::PrunedEndpointDataFlowEntry>>
DataFlowManager::pruneEndpoint(EndpointId endpoint) {
  std::lock_guard lock(mLock);
  auto endpointNode = mEndpointToDataFlows.extract(endpoint);
  if (endpointNode.empty()) {
    ALOGE("Endpoint (0x%" PRIx64 ", 0x%" PRIx64 ") not found", endpoint.hubId,
          endpoint.id);
    return pw::Status::NotFound();
  }
  std::vector<PrunedEndpointDataFlowEntry> prunedDataFlows;
  for (auto *dataFlow : endpointNode.mapped()) {
    auto &entry = prunedDataFlows.emplace_back(dataFlow->id,
                                               dataFlow->source == endpoint);
    auto dataFlowIt = mIdToDataFlow.find(dataFlow->id);
    if (dataFlowIt == mIdToDataFlow.end()) {
      LOG_ALWAYS_FATAL(
          "Data flow (0x%" PRIx64 ", %" PRId32
          ") associated with endpoint (0x%" PRIx64 ", 0x%" PRIx64 ") not found",
          dataFlow->id.hubId, dataFlow->id.id, endpoint.hubId, endpoint.id);
    }
    if (entry.isSource) {
      PW_TRY_ASSIGN(entry.endpoints, removeDataFlowLocked(dataFlowIt));
    } else {
      PW_TRY_ASSIGN(entry.endpoints.emplace_back(),
                    removeSinkLocked(dataFlowIt, endpoint));
    }
  }
  return prunedDataFlows;
}

DataFlowManager::DataFlow::DataFlow(DataFlowId _id, EndpointId _source,
                                    const DataFlowInfo &_info,
                                    bool _isHostSource)
    : info{.region = {.id = _info.region.id},
           .metadataOffsetBytes = _info.metadataOffsetBytes},
      source(_source),
      id(_id),
      isHostSource(_isHostSource) {
  // Store the alert fds if this is an offload source data flow so they can be
  // duplicated when host sinks are added.
  if (!_isHostSource) {
    info.alertFds = dupAlertFds(_info.alertFds, /* isHostEndpoint= */ false);
  }
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

pw::Result<DataFlowManager::DataFlowMap::iterator>
DataFlowManager::addOffloadSourceDataFlowLocked(DataFlowId dataFlowId,
                                                EndpointId source,
                                                DataFlowInfo &info) {
  PW_TRY_ASSIGN(info.alertFds, createAlertFds(/* isHostEndpoint= */ false));
  PW_TRY(mEpollWaiter->addTriggers(dataFlowId, source, info.alertFds));
  auto itAndInserted = mIdToDataFlow.insert(
      {dataFlowId, std::make_unique<DataFlow>(dataFlowId, source, info,
                                              /* isHostSource= */ false)});
  mEndpointToDataFlows[source].insert(itAndInserted.first->second.get());
  return itAndInserted.first;
}

pw::Result<std::vector<EndpointId>> DataFlowManager::removeDataFlowLocked(
    DataFlowMap::iterator it) {
  auto &[dataFlowId, dataFlow] = *it;
  if (dataFlow->isHostSource) {
    decrementHostRegionUseCountLocked(dataFlowId, it->second->info.region.id);
  }
  removeEndpointDataFlowAssociationLocked(dataFlow->source, dataFlow.get());
  std::vector<EndpointId> endpointsToNotify;
  for (const auto &sink : dataFlow->sinks) {
    if (dataFlow->isHostSource) {
      unlinkOffloadSinkMetadataRegionLocked(sink, dataFlow.get());
    }
    endpointsToNotify.push_back(sink);
    removeEndpointDataFlowAssociationLocked(sink, dataFlow.get());
  }
  if (auto status =
          mEpollWaiter->removeTriggers(dataFlowId, /* endpointId= */ {});
      !status.ok()) {
    ALOGE("Failed to remove triggers for data flow (0x%" PRIx64 ", %" PRId32
          ") with %d",
          dataFlowId.hubId, dataFlowId.id, status.code());
  }
  mIdToDataFlow.erase(it);
  return endpointsToNotify;
}

pw::Result<EndpointId> DataFlowManager::removeSinkLocked(
    DataFlowMap::iterator it, EndpointId sink) {
  auto &[dataFlowId, dataFlow] = *it;
  // If this is the only sink remaining for an offload data flow, remove it.
  if (!dataFlow->isHostSource && dataFlow->sinks.size() == 1) {
    auto source = dataFlow->source;
    PW_TRY(removeDataFlowLocked(it).status());
    return source;
  }
  if (dataFlow->isHostSource) {
    unlinkOffloadSinkMetadataRegionLocked(sink, dataFlow.get());
  }
  removeEndpointDataFlowAssociationLocked(sink, dataFlow.get());
  auto status = mEpollWaiter->removeTriggers(dataFlowId, sink);
  if (!status.ok()) {
    ALOGE("Failed to remove triggers for sink (0x%" PRIx64 ", 0x%" PRIx64
          ") on data flow (0x%" PRIx64 ", %" PRId32 ") with %d",
          sink.hubId, sink.id, dataFlowId.hubId, dataFlowId.id, status.code());
  }
  dataFlow->sinks.erase(sink);
  return dataFlow->source;
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
                  " for data flow (0x%" PRIx64 ", %" PRId32 "): %d",
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
    LOG_ALWAYS_FATAL("Hub 0x%" PRIx64 " has no allocated regions",
                     dataFlowId.hubId);
  }
  auto regionIt = hubIt->second.regionToUseCount.find(regionId);
  if (regionIt == hubIt->second.regionToUseCount.end()) {
    LOG_ALWAYS_FATAL("Region %" PRId32 " not found for hub 0x%" PRIx64,
                     regionId, dataFlowId.hubId);
  }
  regionIt->second--;
}

}  // namespace android::hardware::contexthub::common::implementation