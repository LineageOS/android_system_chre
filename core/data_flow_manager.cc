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

#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED

#include "chre/core/data_flow_manager.h"
#include "chre/core/chre_message_hub_manager.h"
#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/platform/fatal_error.h"
#include "chre/util/status.h"
#include "chre/util/system/event_callbacks.h"
#include "chre/util/system/message_common.h"
#include "chre/util/unique_ptr.h"
#include "data_flow/queue.h"
#include "data_flow/untyped_queue.h"

using ::android::contexthub::data_flow::ConsumerPolicyBuilder;
using ::android::contexthub::data_flow::Region;
using ::android::contexthub::data_flow::RemoteEndpointId;
using ::android::contexthub::data_flow::RemoteNotifyArgs;
using ::android::contexthub::data_flow::UntypedProducer;
using ::android::contexthub::data_flow::VariableDataProducer;

namespace chre {

void DataFlowManager::init() {}

uint32_t DataFlowManager::createDataFlowAsync(
    Nanoapp *nanoapp, uint32_t sinkDomains, uint64_t minAverageWriteIntervalNs,
    uint32_t maxAverageWriteBandwidthBytesPerSecond, uint32_t sinkPermissions,
    uint32_t elementSize, uint32_t alignment, uint32_t minElementCount,
    uint32_t maxElementCount, const char *name, uint32_t *dataFlowId) {
  if (name == nullptr || dataFlowId == nullptr) {
    return CHRE_STATUS_INVALID_ARGUMENT;
  }
  *dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  if (mDataFlows.full()) {
    LOG_OOM();
    return CHRE_STATUS_RESOURCE_EXHAUSTED;
  }

  if (!nanoapp->hasPermissions(sinkPermissions)) {
    LOGE("Nanoapp with instance ID 0x%" PRIx16
         " does not have permissions for data flow "
         "%s",
         nanoapp->getInstanceId(), name);
    return CHRE_STATUS_PERMISSION_DENIED;
  }

  for (const NanoappDataFlow &dataFlow : mDataFlows) {
    if (strcmp(dataFlow.properties.name, name) == 0) {
      return CHRE_STATUS_ALREADY_EXISTS;
    }
  }

  BlockConfig config = calculateBlockConfig(minElementCount, maxElementCount);
  uint32_t size = config.blockCapacity * config.maxBlockCount *
                  (elementSize == 0 ? 1 : elementSize);
  pw::Result<uintptr_t> maybeCookie =
      EventLoopManagerSingleton::get()
          ->getSharedDataRegionManager()
          .allocateDataFlowRegionAsync(sinkDomains, size,
                                       minAverageWriteIntervalNs,
                                       maxAverageWriteBandwidthBytesPerSecond);
  if (!maybeCookie.ok()) {
    return maybeCookie.status() == pw::Status::InvalidArgument()
               ? CHRE_STATUS_INVALID_ARGUMENT
               : CHRE_STATUS_RESOURCE_EXHAUSTED;
  }

  NanoappDataFlow dataFlow;
  dataFlow.properties.name = name;
  dataFlow.properties.dataFlowId = mNextDataFlowId++;
  dataFlow.properties.sinkDomains = sinkDomains;
  dataFlow.properties.sinkPermissions = sinkPermissions;
  dataFlow.properties.dataFlowSize = size;
  dataFlow.properties.elementSize = elementSize;
  dataFlow.properties.alignment = alignment;
  dataFlow.properties.minElementCount = minElementCount;
  dataFlow.properties.maxElementCount = maxElementCount;
  dataFlow.properties.blockConfig = config;
  dataFlow.nanoappInstanceId = nanoapp->getInstanceId();
  dataFlow.regionId = kInvalidRegionId;
  dataFlow.cookie = maybeCookie.value();
  dataFlow.memoryAccess = nullptr;
  dataFlow.producer = std::monostate();
  *dataFlowId = dataFlow.properties.dataFlowId;
  mDataFlows.push_back(std::move(dataFlow));
  return CHRE_STATUS_OK;
}

uint32_t DataFlowManager::destroyDataFlow(Nanoapp *nanoapp,
                                          uint32_t dataFlowId) {
  NanoappDataFlow *dataFlow = nullptr;
  uint32_t status =
      getNanoappDataFlow(dataFlowId, nanoapp->getInstanceId(), &dataFlow);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  std::visit(
      [](auto &&producer) -> void {
        if constexpr (!std::is_same_v<std::decay_t<decltype(producer)>,
                                      std::monostate>) {
          producer.stop();
        }
      },
      dataFlow->producer);
  // TODO(b/457453613): Call reportDataFlowSinkUnregistered on the CHRE
  // Message Hub.
  int32_t regionId = dataFlow->regionId;
  mDataFlows.erase(dataFlow);
  EventLoopManagerSingleton::get()
      ->getSharedDataRegionManager()
      .handleDataFlowStopped(regionId);
  return CHRE_STATUS_OK;
}

uint32_t DataFlowManager::sourceAddSinkAsync(
    Nanoapp * /*nanoapp*/, uint64_t /*hubId*/, uint64_t /*endpointId*/,
    uint32_t /*dataFlowId*/,
    const struct chreDataFlowSinkPolicy * /*sinkPolicy*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sourceAddSinkOverSessionAsync(
    Nanoapp * /*nanoapp*/, uint64_t /*hubId*/, uint64_t /*endpointId*/,
    uint32_t /*dataFlowId*/,
    const struct chreDataFlowSinkPolicy * /*sinkPolicy*/, void * /*message*/,
    size_t /*messageSize*/, uint32_t /*messageType*/, uint16_t /*sessionId*/,
    uint32_t /*messagePermissions*/,
    chreMessageFreeFunction * /*freeCallback*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sourceConfigureSink(
    Nanoapp * /*nanoapp*/, uint64_t /*hubId*/, uint64_t /*endpointId*/,
    uint32_t /*dataFlowId*/,
    const struct chreDataFlowSinkPolicy * /*sinkPolicy*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sourceReserve(Nanoapp * /*nanoapp*/,
                                        uint32_t /*dataFlowId*/,
                                        uint32_t /*numBytes*/, void ** /*data*/,
                                        uint32_t * /*reservedBytes*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sourceCommit(Nanoapp * /*nanoapp*/,
                                       uint32_t /*dataFlowId*/,
                                       uint32_t /*numBytes*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sourcePush(Nanoapp * /*nanoapp*/,
                                     uint32_t /*dataFlowId*/,
                                     const void * /*data*/,
                                     uint32_t /*numBytes*/,
                                     bool /*allOrNothing*/,
                                     uint32_t * /*numberOfBytesPushed*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sourceGetSize(Nanoapp * /*nanoapp*/,
                                        uint32_t /*dataFlowId*/,
                                        bool /*includeReserved*/,
                                        uint32_t * /*size*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sourceGetCapacity(Nanoapp * /*nanoapp*/,
                                            uint32_t /*dataFlowId*/,
                                            uint32_t * /*capacity*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sinkEnable(Nanoapp * /*nanoapp*/, uint64_t /*hubId*/,
                                     uint32_t /*dataFlowId*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sinkDisable(Nanoapp * /*nanoapp*/, uint64_t /*hubId*/,
                                      uint32_t /*dataFlowId*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sinkGetState(Nanoapp * /*nanoapp*/,
                                       uint64_t /*hubId*/,
                                       uint32_t /*dataFlowId*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sinkPeek(Nanoapp * /*nanoapp*/, uint64_t /*hubId*/,
                                   uint32_t /*dataFlowId*/,
                                   uint32_t /*numRequestedBytes*/,
                                   const void ** /*data*/,
                                   uint32_t * /*numBytes*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sinkRelease(Nanoapp * /*nanoapp*/, uint64_t /*hubId*/,
                                      uint32_t /*dataFlowId*/,
                                      uint32_t /*numBytes*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sinkSeek(Nanoapp * /*nanoapp*/, uint64_t /*hubId*/,
                                   uint32_t /*dataFlowId*/,
                                   uint32_t /*offset*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sinkGetOffset(Nanoapp * /*nanoapp*/,
                                        uint64_t /*hubId*/,
                                        uint32_t /*dataFlowId*/,
                                        uint32_t * /*offset*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

void DataFlowManager::handleAllocateDataFlowRegionAsyncResult(
    uintptr_t cookie, pw::Status status, int32_t regionId,
    const android::contexthub::data_flow::AllocatorRegion &region,
    android::contexthub::data_flow::MemoryAccess *memoryAccess) {
  NanoappDataFlow *foundDataFlow = nullptr;
  for (NanoappDataFlow &dataFlow : mDataFlows) {
    if (dataFlow.cookie.has_value() && *dataFlow.cookie == cookie) {
      if (status.ok()) {
        foundDataFlow = &dataFlow;
        break;
      }
    }
  }
  if (foundDataFlow == nullptr) {
    LOGE("Received async result for unknown data flow with cookie: %" PRIuPTR
         " and status: %s",
         cookie, status.str());
    return;
  }

  if (status.ok()) {
    foundDataFlow->regionId = regionId;
    foundDataFlow->cookie = std::nullopt;
    foundDataFlow->allocatorRegion = region;
    foundDataFlow->memoryAccess = memoryAccess;
    status = createProducer(*foundDataFlow);
  }

  auto event = MakeUnique<chreDataFlowCreatedInfo>();
  event->status = toChreStatus(status);
  event->dataFlowId = foundDataFlow->properties.dataFlowId;
  event->size = foundDataFlow->properties.dataFlowSize;
  event->sinkDomains = foundDataFlow->properties.sinkDomains;
  event->permissions = foundDataFlow->properties.sinkPermissions;
  EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(
      CHRE_EVENT_DATA_FLOW_CREATED, event.release(), freeEventDataCallback,
      foundDataFlow->nanoappInstanceId);

  if (!status.ok()) {
    mDataFlows.erase(foundDataFlow);
  }
}

void DataFlowManager::onRegisterDataFlowSink(
    chre::message::DataFlowSinkRegistration && /*registration*/) {
  // TODO(b/457453613): Implement this function.
}

void DataFlowManager::onDataFlowSinkUnregistered(
    const chre::message::DataFlowSinkUnregistration & /*unregistration*/) {
  // TODO(b/457453613): Implement this function.
}

void DataFlowManager::onDataFlowStopped(
    const chre::message::DataFlowStopped & /*stopped*/) {
  // TODO(b/457453613): Implement this function.
}

void DataFlowManager::onDataFlowAlert(
    const chre::message::DataFlowAlert & /*alert*/) {
  // TODO(b/457453613): Implement this function.
}

uint32_t DataFlowManager::buildConsumerPolicy(
    const struct chreDataFlowSinkPolicy *sinkPolicy,
    ConsumerPolicyBuilder *policyBuilderOut) {
  switch (sinkPolicy->newDataAlertPolicy) {
    case CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_NEVER:
      policyBuilderOut->setNeverNotify();
      break;
    case CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_OPPORTUNISTIC:
      policyBuilderOut->setOpportunistic(
          sinkPolicy->newDataAlertPolicyData.lowWatermark);
      break;
    case CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_HIGH_WATER_MARK:
      policyBuilderOut->setHighWaterMark(
          sinkPolicy->newDataAlertPolicyData.highWatermark);
      break;
    case CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_PERIODIC:
      policyBuilderOut->setPeriodic(
          sinkPolicy->newDataAlertPolicyData.periodMs);
      break;
    case CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_STREAMING:
      policyBuilderOut->setStreaming();
      break;
    default:
      LOGE("Invalid new data alert policy: %" PRIu32,
           static_cast<uint32_t>(sinkPolicy->newDataAlertPolicy));
      return CHRE_STATUS_INVALID_ARGUMENT;
  }

  if (sinkPolicy->overwritePolicy ==
      CHRE_DATA_FLOW_SINK_OVERWRITE_POLICY_ALLOWED) {
    policyBuilderOut->setOverwritable();
  } else {
    policyBuilderOut->setNonOverwritable();
  }
  return CHRE_STATUS_OK;
}

DataFlowManager::BlockConfig DataFlowManager::calculateBlockConfig(
    uint32_t minElementCount, uint32_t maxElementCount) {
  constexpr size_t kMinBlockCount = 1;
  constexpr size_t kMaxBlockCountTarget = 4;

  size_t blockCapacity = maxElementCount / kMaxBlockCountTarget;
  if (maxElementCount % kMaxBlockCountTarget != 0) {
    blockCapacity += kMaxBlockCountTarget;
  }
  if (blockCapacity < minElementCount) {
    blockCapacity = minElementCount;
  }

  size_t maxBlockCount = maxElementCount / blockCapacity;
  if (maxElementCount % blockCapacity != 0) {
    ++maxBlockCount;
  }

  return {blockCapacity, kMinBlockCount, maxBlockCount};
}

void DataFlowManager::sendDataFlowAlertToRemoteSink(
    uint32_t /*dataFlowId*/, uint64_t /*sinkHubId*/,
    uint64_t /*sinkEndpointId*/) {
  // TODO(b/457453613): Implement this function. This may be called from other
  // DataFlowManager APIs.
}

pw::Status DataFlowManager::createProducer(NanoappDataFlow &dataFlow) {
  EventLoop *eventLoop =
      EventLoopManagerSingleton::get()->getEventLoopByInstanceId(
          dataFlow.nanoappInstanceId);
  if (eventLoop == nullptr) {
    LOGE("Event loop not found for data flow");
    return pw::Status::NotFound();
  }

  Nanoapp *nanoapp =
      eventLoop->findNanoappByInstanceId(dataFlow.nanoappInstanceId);
  if (nanoapp == nullptr) {
    LOGE("Nanoapp not found for data flow");
    return pw::Status::NotFound();
  }

  uint32_t dataFlowId = dataFlow.properties.dataFlowId;
  RemoteNotifyArgs notifyArgs = {
      .fn =
          [dataFlowId](const RemoteEndpointId &id) {
            EventLoopManagerSingleton::get()
                ->getDataFlowManager()
                .sendDataFlowAlertToRemoteSink(
                    dataFlowId, static_cast<uint64_t>(id.aidlId.hubId),
                    static_cast<uint64_t>(id.aidlId.endpointId));
          },
      .id =
          {
              .aidlId = {.hubId = static_cast<int64_t>(
                             ChreMessageHubManager::kChreMessageHubId),
                         .endpointId =
                             static_cast<int64_t>(nanoapp->getAppId())},
          },
  };

  if (dataFlow.properties.elementSize == 0) {
    pw::Result<VariableDataProducer> producer =
        VariableDataProducer::createRemote(
            dataFlow.allocatorRegion,
            dataFlow.properties.blockConfig.blockCapacity,
            dataFlow.properties.blockConfig.maxBlockCount,
            dataFlow.properties.blockConfig.minBlockCount, mDataNotifier,
            std::move(notifyArgs), dataFlow.memoryAccess);
    if (!producer.ok()) {
      LOGE("Failed to create variable producer: %s", producer.status().str());
      return producer.status();
    }
    dataFlow.producer = std::move(*producer);
  } else {
    pw::Result<UntypedProducer> producer = UntypedProducer::createRemote(
        dataFlow.allocatorRegion, dataFlow.properties.blockConfig.blockCapacity,
        dataFlow.properties.elementSize, dataFlow.properties.alignment,
        dataFlow.properties.blockConfig.maxBlockCount,
        dataFlow.properties.blockConfig.minBlockCount, mDataNotifier,
        std::move(notifyArgs), dataFlow.memoryAccess);
    if (!producer.ok()) {
      LOGE("Failed to create untyped producer: %s", producer.status().str());
      return producer.status();
    }
    dataFlow.producer = std::move(*producer);
  }

  return pw::OkStatus();
}

uint32_t DataFlowManager::getNanoappDataFlow(uint32_t dataFlowId,
                                             uint16_t nanoappInstanceId,
                                             NanoappDataFlow **dataFlowOut) {
  for (NanoappDataFlow &dataFlow : mDataFlows) {
    if (dataFlow.properties.dataFlowId == dataFlowId) {
      if (dataFlow.nanoappInstanceId != nanoappInstanceId) {
        LOGE("Nanoapp with instance ID 0x%" PRIx16
             " does not own data flow with ID %" PRIu32,
             nanoappInstanceId, dataFlowId);
        return CHRE_STATUS_PERMISSION_DENIED;
      }
      *dataFlowOut = &dataFlow;
      return CHRE_STATUS_OK;
    }
  }
  LOGE("Data flow %" PRIu32 " not found", dataFlowId);
  return CHRE_STATUS_NOT_FOUND;
}

}  // namespace chre

#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
