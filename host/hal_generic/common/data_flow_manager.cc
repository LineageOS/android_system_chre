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

#include <errno.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <utils/Log.h>

#include "android/binder_auto_utils.h"
#include "data_flow_epoll_waiter.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "region_allocator.h"
#include "wakelock_manager.h"

namespace android::hardware::contexthub::common::implementation {
namespace {

pw::Result<DataFlowAlertFds> createAlertFds(bool isHostEndpoint) {
  DataFlowAlertFds fds{
      .waking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK)),
      .nonWaking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK)),
      .halAck = ndk::ScopedFileDescriptor(
          isHostEndpoint ? eventfd(0, EFD_NONBLOCK) : -1)};
  if (fds.waking.get() < 0 || fds.nonWaking.get() < 0 ||
      (isHostEndpoint && fds.halAck.get() < 0)) {
    ALOGE("Failed to create alert fds with code %d", errno);
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

pw::Status sendAlert(const DataFlowAlertFds &alertFds, bool isWaking) {
  const uint64_t kOne = 1;
  ssize_t result = TEMP_FAILURE_RETRY(
      write(isWaking ? alertFds.waking.get() : alertFds.nonWaking.get(), &kOne,
            sizeof(kOne)));
  if (result != sizeof(kOne)) {
    ALOGE("Failed to send alert with code %d", errno);
    return pw::Status::Internal();
  }
  return pw::OkStatus();
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
  } else if (regionIt->second > 0) {
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
  DataFlowId dataFlowId = {.hubId = source.hubId,
                           .id = hubIt->second.nextDataFlowId};
  PW_TRY(mEpollWaiter->addTriggers(dataFlowId, source, info.alertFds));
  hubIt->second.nextDataFlowId++;
  regionIt->second++;
  auto &dataFlow = mIdToDataFlow[dataFlowId] = std::make_unique<DataFlow>(
      dataFlowId, source, info, /* isHostSource= */ true);
  mIdToEndpoint.emplace(
      std::piecewise_construct, std::forward_as_tuple(source),
      std::forward_as_tuple(
          dataFlow.get(),
          dupAlertFds(info.alertFds, /* isHostEndpoint= */ true)));
  return dataFlowId;
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
  } else if (dataFlow->sinks.contains(params.sinkId)) {
    ALOGE("Sink (0x%" PRIx64 ", 0x%" PRIx64
          ") already registered on data flow (0x%" PRIx64 ", %" PRId32 ")",
          params.sinkId.hubId, params.sinkId.id, dataFlowId.hubId,
          dataFlowId.id);
    return pw::Status::AlreadyExists();
  }
  PW_TRY(mEpollWaiter->addTriggers(dataFlowId, params.sinkId,
                                   params.context.alertFds));
  Endpoint sink(dataFlow.get());
  PW_TRY_ASSIGN(
      auto region,
      getOffloadSinkMetadataRegionLocked(params.sinkId, dataFlow.get(), sink)
          .or_else([this, &dataFlowId, &params](pw::Status status) {
            mEpollWaiter->removeTriggers(dataFlowId, params.sinkId)
                .IgnoreError();
            return status;
          }));
  dataFlow->sinks.insert(params.sinkId);
  mIdToEndpoint.emplace(params.sinkId, std::move(sink));
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
    } else if (dataFlowIt->second->sinks.contains(sink)) {
      ALOGE("Sink (0x%" PRIx64 ", 0x%" PRIx64
            ") already registered on data flow (0x%" PRIx64 ", %" PRId32 ")",
            sink.hubId, sink.id, dataFlowId.hubId, dataFlowId.id);
      return pw::Status::AlreadyExists();
    } else if (dataFlowIt->second->info.metadataOffsetBytes != metadataOffset) {
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
  mIdToEndpoint.emplace(
      std::piecewise_construct, std::forward_as_tuple(sink),
      std::forward_as_tuple(&dataFlow,
                            dupAlertFds(context.alertFds,
                                        /* isHostEndpoint= */ true)));
  return context;
}

pw::Status DataFlowManager::verifyEndpointOnDataFlow(DataFlowId dataFlowId,
                                                     EndpointId endpointId,
                                                     bool isHost) {
  std::lock_guard lock(mLock);
  PW_TRY_ASSIGN(auto dataFlowAndEndpoint,
                lookupDataFlowAndEndpointLocked(dataFlowId, endpointId));
  if (dataFlowAndEndpoint.second->isHost != isHost) {
    ALOGE("Endpoint (0x%" PRIx64 ", 0x%" PRIx64 ") is not a %s endpoint",
          endpointId.hubId, endpointId.id, isHost ? "host" : "offload");
    return pw::Status::NotFound();
  }
  return pw::OkStatus();
}

pw::Status DataFlowManager::alertHostEndpoints(
    DataFlowId dataFlowId, const std::vector<EndpointId> &endpointIds,
    bool isWaking) {
  std::lock_guard lock(mLock);
  auto dataFlowIt = mIdToDataFlow.find(dataFlowId);
  if (dataFlowIt == mIdToDataFlow.end()) {
    ALOGE("Data flow (0x%" PRIx64 ", %" PRId32 ") not found", dataFlowId.hubId,
          dataFlowId.id);
    return pw::Status::NotFound();
  } else if (dataFlowIt->second->isHostSource) {
    ALOGE(
        "alertHostEndpoints() called on data flow with a host source "
        "(0x%" PRIx64 ", %" PRId32 ")",
        dataFlowId.hubId, dataFlowId.id);
    return pw::Status::InvalidArgument();
  }
  for (const auto &endpointId : endpointIds) {
    if (!dataFlowIt->second->sinks.contains(endpointId)) {
      ALOGW("Sink (0x%" PRIx64 ", 0x%" PRIx64
            ") not found on data flow (0x%" PRIx64 ", %" PRId32 ")",
            endpointId.hubId, endpointId.id, dataFlowId.hubId, dataFlowId.id);
      continue;
    }
    auto &endpoint = getEndpointLocked(endpointId)->second;
    auto &[alertFds, outstandingWakeCount] =
        getAlertFdsAndWakeCountMapLocked(endpointId, endpoint, dataFlowId);
    bool decrementWakeCountOnFailure =
        isWaking &&
        !incrementWakeCountLocked(endpointId, dataFlowId, outstandingWakeCount);
    if (auto status = sendAlert(alertFds, isWaking); !status.ok()) {
      ALOGE("Failed to send alert for data flow (0x%" PRIx64 ", %" PRId32
            ") to endpoint (0x%" PRIx64 ", 0x%" PRIx64 ") with %d",
            dataFlowId.hubId, dataFlowId.id, endpointId.hubId, endpointId.id,
            status.code());
      if (decrementWakeCountOnFailure) {
        decreaseWakeCountLocked(endpointId, dataFlowId, outstandingWakeCount,
                                /*decrease=*/1);
      }
    }
  }
  return pw::OkStatus();
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
  } else if (!dataFlowIt->second->sinks.contains(sink)) {
    ALOGE("Sink (0x%" PRIx64 ", 0x%" PRIx64
          ") not found on data flow (0x%" PRIx64 ", %" PRId32 ")",
          sink.hubId, sink.id, dataFlowId.hubId, dataFlowId.id);
    return pw::Status::NotFound();
  }
  return removeSinkLocked(dataFlowIt, getEndpointLocked(sink));
}

pw::Result<std::vector<DataFlowManager::PrunedEndpointDataFlowEntry>>
DataFlowManager::pruneEndpoint(EndpointId endpointId) {
  std::lock_guard lock(mLock);
  auto endpointIt = mIdToEndpoint.find(endpointId);
  if (endpointIt == mIdToEndpoint.end()) {
    ALOGE("Endpoint (0x%" PRIx64 ", 0x%" PRIx64 ") not found", endpointId.hubId,
          endpointId.id);
    return pw::Status::NotFound();
  }
  std::vector<PrunedEndpointDataFlowEntry> prunedDataFlows;
  for (auto *dataFlow : endpointIt->second.dataFlows) {
    prunedDataFlows.emplace_back(dataFlow->id, dataFlow->source == endpointId);
  }
  for (auto &entry : prunedDataFlows) {
    auto dataFlowIt = mIdToDataFlow.find(entry.dataFlowId);
    if (dataFlowIt == mIdToDataFlow.end()) {
      LOG_ALWAYS_FATAL("Data flow (0x%" PRIx64 ", %" PRId32
                       ") associated with endpoint (0x%" PRIx64 ", 0x%" PRIx64
                       ") not found",
                       entry.dataFlowId.hubId, entry.dataFlowId.id,
                       endpointId.hubId, endpointId.id);
    }
    if (entry.isSource) {
      PW_TRY_ASSIGN(entry.endpoints, removeDataFlowLocked(dataFlowIt));
    } else {
      PW_TRY_ASSIGN(entry.endpoints.emplace_back(),
                    removeSinkLocked(dataFlowIt, endpointIt));
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

void DataFlowManager::onAlert(DataFlowId dataFlowId, EndpointId endpointId,
                              bool waking) {
  {
    std::lock_guard lock(mLock);
    if (!lookupDataFlowAndEndpointLocked(dataFlowId, endpointId).ok()) {
      ALOGW("Could not find data flow and/or endpoint for alert");
      return;
    }
  }
  if (auto status = mSendAlertFn(dataFlowId, endpointId, waking);
      !status.ok()) {
    ALOGW("Failed to send alert for data flow (0x%" PRIx64 ", %" PRId32
          ") to endpoint (0x%" PRIx64 ", 0x%" PRIx64 ") with %d",
          dataFlowId.hubId, dataFlowId.id, endpointId.hubId, endpointId.id,
          status.code());
  }
}

void DataFlowManager::onWakingAck(DataFlowId dataFlowId, EndpointId endpointId,
                                  uint64_t wakeCount) {
  std::lock_guard lock(mLock);
  auto result = lookupDataFlowAndEndpointLocked(dataFlowId, endpointId);
  if (!result.ok()) {
    ALOGW("Could not find data flow and/or endpoint for waking ack");
    return;
  }
  auto &endpoint = *result.value().second;
  if (!endpoint.isHost) {
    ALOGW("Waking ack for non-host endpoint (0x%" PRIx64 ", 0x%" PRIx64 ")",
          endpointId.hubId, endpointId.id);
    return;
  }
  auto &outstandingWakeCount =
      getAlertFdsAndWakeCountMapLocked(endpointId, endpoint, dataFlowId).second;
  decreaseWakeCountLocked(
      endpointId, dataFlowId, outstandingWakeCount,
      /*decrease=*/std::min(outstandingWakeCount, wakeCount));
}

pw::Result<DataFlowManager::DataFlowMap::iterator>
DataFlowManager::addOffloadSourceDataFlowLocked(DataFlowId dataFlowId,
                                                EndpointId source,
                                                DataFlowInfo &info) {
  PW_TRY_ASSIGN(info.alertFds, createAlertFds(/* isHostEndpoint= */ false));
  PW_TRY(mEpollWaiter->addTriggers(dataFlowId, source, info.alertFds));
  auto [dataFlowIt, _] = mIdToDataFlow.insert(
      {dataFlowId, std::make_unique<DataFlow>(dataFlowId, source, info,
                                              /* isHostSource= */ false)});
  mIdToEndpoint.emplace(source, dataFlowIt->second.get());
  return dataFlowIt;
}

pw::Result<std::vector<EndpointId>> DataFlowManager::removeDataFlowLocked(
    DataFlowMap::iterator it) {
  auto &[dataFlowId, dataFlow] = *it;
  if (dataFlow->isHostSource) {
    decrementHostRegionUseCountLocked(dataFlowId, it->second->info.region.id);
  }
  removeEndpointDataFlowAssociationLocked(getEndpointLocked(dataFlow->source),
                                          dataFlow.get());
  std::vector<EndpointId> endpointsToNotify;
  for (const auto &sinkId : dataFlow->sinks) {
    auto sinkIt = getEndpointLocked(sinkId);
    releaseEndpointResourcesLocked(sinkIt, dataFlow.get());
    endpointsToNotify.push_back(sinkId);
    removeEndpointDataFlowAssociationLocked(sinkIt, dataFlow.get());
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
    DataFlowMap::iterator dataFlowIt, EndpointMap::iterator sinkIt) {
  auto &[dataFlowId, dataFlow] = *dataFlowIt;
  // If this is the only sink remaining for an offload data flow, remove it.
  if (!dataFlow->isHostSource && dataFlow->sinks.size() == 1) {
    auto source = dataFlow->source;
    PW_TRY(removeDataFlowLocked(dataFlowIt).status());
    return source;
  }
  releaseEndpointResourcesLocked(sinkIt, dataFlow.get());
  auto &sinkId = sinkIt->first;
  auto status = mEpollWaiter->removeTriggers(dataFlowId, sinkId);
  if (!status.ok()) {
    ALOGE("Failed to remove triggers for sink (0x%" PRIx64 ", 0x%" PRIx64
          ") on data flow (0x%" PRIx64 ", %" PRId32 ") with %d",
          sinkId.hubId, sinkId.id, dataFlowId.hubId, dataFlowId.id,
          status.code());
  }
  dataFlow->sinks.erase(sinkId);
  removeEndpointDataFlowAssociationLocked(sinkIt, dataFlow.get());
  return dataFlow->source;
}

void DataFlowManager::removeEndpointDataFlowAssociationLocked(
    EndpointMap::iterator endpointIt, DataFlow *dataFlow) {
  auto &endpoint = endpointIt->second;
  endpoint.dataFlows.erase(dataFlow);
  if (endpoint.dataFlows.empty()) {
    mIdToEndpoint.erase(endpointIt);
  }
}

pw::Result<SharedDataRegion>
DataFlowManager::getOffloadSinkMetadataRegionLocked(EndpointId sinkId,
                                                    DataFlow *dataFlow,
                                                    Endpoint &sink) {
  SharedDataRegion region;
  if (mRegionAllocator->consumerRequiresSeparateRegion(sinkId.hubId)) {
    auto &metadataRegionMap = std::get<Endpoint::MetadataRegionMap>(sink.map);
    auto it = metadataRegionMap.find(dataFlow->id);
    if (it != metadataRegionMap.end()) {
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
      metadataRegionMap[dataFlow->id] = {region.id, 1};
    }
  } else {
    PW_TRY_ASSIGN(region,
                  mRegionAllocator->getRegionInfo(dataFlow->info.region.id));
  }
  return region;
}

void DataFlowManager::releaseEndpointResourcesLocked(
    EndpointMap::iterator endpointIt, DataFlow *dataFlow) {
  auto &[endpointId, endpoint] = *endpointIt;
  if (endpoint.isHost) {
    auto &outstandingWakeCount =
        getAlertFdsAndWakeCountMapLocked(endpointId, endpoint, dataFlow->id)
            .second;
    decreaseWakeCountLocked(endpointId, dataFlow->id, outstandingWakeCount,
                            outstandingWakeCount);
  } else {
    unlinkOffloadSinkMetadataRegionLocked(endpoint, dataFlow);
  }
}

void DataFlowManager::unlinkOffloadSinkMetadataRegionLocked(
    Endpoint &sink, DataFlow *dataFlow) {
  auto &metadataRegionMap = std::get<Endpoint::MetadataRegionMap>(sink.map);
  auto regionIt = metadataRegionMap.find(dataFlow->id);
  if (regionIt != metadataRegionMap.end()) {
    regionIt->second.second--;
    if (regionIt->second.second == 0) {
      auto status = mRegionAllocator->releaseRegion(regionIt->second.first);
      if (!status.ok()) {
        ALOGE("Failed to release sink metadata region %" PRId32
              " for data flow (0x%" PRIx64 ", %" PRId32 "): %d",
              regionIt->second.first, dataFlow->id.hubId, dataFlow->id.id,
              status.code());
      }
      metadataRegionMap.erase(regionIt);
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

pw::Result<std::pair<DataFlowManager::DataFlow *, DataFlowManager::Endpoint *>>
DataFlowManager::lookupDataFlowAndEndpointLocked(DataFlowId dataFlowId,
                                                 EndpointId endpointId) {
  auto dataFlowIt = mIdToDataFlow.find(dataFlowId);
  if (dataFlowIt == mIdToDataFlow.end()) {
    ALOGW("Could not find data flow (0x%" PRIx64 ", %" PRId32 ") on callback",
          dataFlowId.hubId, dataFlowId.id);
    return pw::Status::NotFound();
  }
  auto *dataFlow = dataFlowIt->second.get();
  auto endpointIt = mIdToEndpoint.find(endpointId);
  if (endpointIt == mIdToEndpoint.end()) {
    ALOGW("Could not find endpoint (0x%" PRIx64 ", 0x%" PRIx64 ") on callback",
          endpointId.hubId, endpointId.id);
    return pw::Status::NotFound();
  } else if (!endpointIt->second.dataFlows.contains(dataFlow)) {
    ALOGW("Cannot find association between endpoint (0x%" PRIx64 ", 0x%" PRIx64
          ") and data flow (0x%" PRIx64 ", %" PRId32 ") on callback",
          endpointId.hubId, endpointId.id, dataFlowId.hubId, dataFlowId.id);
    return pw::Status::NotFound();
  }
  return std::make_pair(dataFlow, &endpointIt->second);
}

DataFlowManager::EndpointMap::iterator DataFlowManager::getEndpointLocked(
    EndpointId endpointId) {
  auto endpointIt = mIdToEndpoint.find(endpointId);
  if (endpointIt == mIdToEndpoint.end()) {
    LOG_ALWAYS_FATAL("Endpoint (0x%" PRIx64 ", 0x%" PRIx64 ") not found",
                     endpointId.hubId, endpointId.id);
  }
  return endpointIt;
}

std::pair<DataFlowAlertFds, uint64_t> &
DataFlowManager::getAlertFdsAndWakeCountMapLocked(EndpointId endpointId,
                                                  Endpoint &endpoint,
                                                  DataFlowId dataFlowId) {
  auto &alertFdAndWakeCountMap =
      std::get<Endpoint::AlertFdAndWakeCountMap>(endpoint.map);
  auto it = alertFdAndWakeCountMap.find(dataFlowId);
  if (it == alertFdAndWakeCountMap.end()) {
    LOG_ALWAYS_FATAL(
        "Could not find alert fds for data flow (0x%" PRIx64 ", %" PRId32
        ") on endpoint (0x%" PRIx64 ", 0x%" PRIx64 ")",
        dataFlowId.hubId, dataFlowId.id, endpointId.hubId, endpointId.id);
  }
  return it->second;
}

bool DataFlowManager::incrementWakeCountLocked(EndpointId endpointId,
                                               DataFlowId dataFlowId,
                                               uint64_t &wakeCount) {
  auto status =
      mWakelockManager->increaseWakeCount(WakelockManager::Usage::kDataFlow, 1);
  if (!status.ok()) {
    ALOGW(
        "Failed to increase wake count for waking alert to endpoint "
        "(0x%" PRIx64 ", 0x%" PRIx64 ") on data flow (0x%" PRIx64 ", %" PRId32
        ") with %d",
        endpointId.hubId, endpointId.id, dataFlowId.hubId, dataFlowId.id,
        status.code());
  } else {
    wakeCount++;
  }
  return status.ok();
}

void DataFlowManager::decreaseWakeCountLocked(EndpointId endpointId,
                                              DataFlowId dataFlowId,
                                              uint64_t &wakeCount,
                                              size_t decrease) {
  if (decrease == 0) {
    return;
  }
  auto status = mWakelockManager->decreaseWakeCount(
      WakelockManager::Usage::kDataFlow, decrease);
  if (!status.ok()) {
    ALOGE(
        "Failed to decrease wake count for waking alert to endpoint "
        "(0x%" PRIx64 ", 0x%" PRIx64 ") on data flow (0x%" PRIx64 ", %" PRId32
        ") with %d",
        endpointId.hubId, endpointId.id, dataFlowId.hubId, dataFlowId.id,
        status.code());
  } else {
    wakeCount -= decrease;
  }
}

}  // namespace android::hardware::contexthub::common::implementation