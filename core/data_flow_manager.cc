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

#include <cstddef>
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED

#include "chre/core/chre_message_hub_manager.h"
#include "chre/core/data_flow_manager.h"
#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/core/shared_data_region_manager.h"
#include "chre/platform/context.h"
#include "chre/platform/fatal_error.h"
#include "chre/util/status.h"
#include "chre/util/system/event_callbacks.h"
#include "chre/util/system/message_common.h"
#include "chre/util/system/message_router.h"
#include "chre/util/unique_ptr.h"
#include "data_flow/queue.h"
#include "data_flow/untyped_queue.h"

#include <cinttypes>

namespace chre {

using ::android::contexthub::data_flow::ConsumerPolicyBuilder;
using ::android::contexthub::data_flow::Region;
using ::android::contexthub::data_flow::RemoteEndpointId;
using ::android::contexthub::data_flow::RemoteNotifyArgs;
using ::android::contexthub::data_flow::ScopedMemoryAccess;
using ::android::contexthub::data_flow::UntypedConsumer;
using ::android::contexthub::data_flow::UntypedProducer;
using ::android::contexthub::data_flow::VariableDataConsumer;
using ::android::contexthub::data_flow::VariableDataProducer;
using ::android::contexthub::data_flow::internal::ElementConfig;
using ::android::contexthub::data_flow::internal::fromOffset;
using ::android::contexthub::data_flow::internal::Queue;
using message::DataFlowAlert;
using message::DataFlowSinkRegistration;
using message::DataFlowSinkUnregistration;
using message::DataFlowStopped;
using message::Endpoint;

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

  DynamicVector<Endpoint> sinkEndpoints;
  std::visit(
      [&sinkEndpoints](auto &&producer) -> void {
        if constexpr (!std::is_same_v<std::decay_t<decltype(producer)>,
                                      std::monostate>) {
          producer.stop();
          sinkEndpoints.reserve(
              producer.getConsumerManager().getNumConsumers());
          producer.getConsumerManager().pruneConsumers(
              [&sinkEndpoints](const RemoteEndpointId &endpointId) -> bool {
                sinkEndpoints.emplace_back(endpointId.aidlId.hubId,
                                           endpointId.aidlId.endpointId);
                // Return false to not prune the consumer. It will be cleaned up
                // automatically either when it marks itself removed in shared
                // memory (since stop() was called) or when the data flow is
                // destroyed.
                return false;
              });
        }
      },
      dataFlow->producer);

  // Report data flow stopped event to the sinks
  DataFlowStopped stopped;
  stopped.dataFlowId.hubId = ChreMessageHubManager::kChreMessageHubId;
  stopped.dataFlowId.id = dataFlowId;
  stopped.destinationEndpoints = sinkEndpoints;
  EventLoopManagerSingleton::get()
      ->getChreMessageHubManager()
      .getMessageHub()
      .reportDataFlowStopped(stopped);

  int32_t regionId = dataFlow->regionId;
  mDataFlows.erase(dataFlow);
  EventLoopManagerSingleton::get()
      ->getSharedDataRegionManager()
      .handleDataFlowStopped(regionId);
  return CHRE_STATUS_OK;
}

uint32_t DataFlowManager::sourceAddSinkAsync(
    Nanoapp *nanoapp, uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy) {
  return sourceAddSinkAsyncCommon(nanoapp, hubId, endpointId, dataFlowId,
                                  sinkPolicy, /* hasMessage= */ false,
                                  /* message= */ nullptr, /* messageSize= */ 0,
                                  /* messageType= */ 0, /* sessionId= */ 0,
                                  /* messagePermissions= */ 0,
                                  /* freeCallback= */ nullptr);
}

uint32_t DataFlowManager::sourceAddSinkOverSessionAsync(
    Nanoapp *nanoapp, uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy, void *message,
    size_t messageSize, uint32_t messageType, uint16_t sessionId,
    uint32_t messagePermissions, chreMessageFreeFunction *freeCallback) {
  return sourceAddSinkAsyncCommon(nanoapp, hubId, endpointId, dataFlowId,
                                  sinkPolicy, /* hasMessage= */ true, message,
                                  messageSize, messageType, sessionId,
                                  messagePermissions, freeCallback);
}

uint32_t DataFlowManager::sourceConfigureSink(
    Nanoapp *nanoapp, uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy) {
  NanoappDataFlow *foundDataFlow = nullptr;
  android::contexthub::data_flow::ConsumerPolicyBuilder policyBuilder;
  RemoteEndpointId remoteId{
      .aidlId = {.hubId = static_cast<int64_t>(hubId),
                 .endpointId = static_cast<int64_t>(endpointId)},
  };
  uint32_t status =
      validateAndGetSinkRequest(nanoapp, hubId, endpointId, dataFlowId,
                                sinkPolicy, &foundDataFlow, &policyBuilder);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  pw::Status updateStatus = std::visit(
      [&remoteId, &policyBuilder](auto &&producer) -> pw::Status {
        if constexpr (std::is_same_v<std::decay_t<decltype(producer)>,
                                     std::monostate>) {
          return pw::Status::FailedPrecondition();
        } else {
          return producer.getConsumerManager().updateConsumerPolicy(
              remoteId, policyBuilder);
        }
      },
      foundDataFlow->producer);

  if (updateStatus.IsNotFound()) {
    LOGE("Failed to find consumer to update for data flow %" PRIu32,
         dataFlowId);
    return CHRE_STATUS_NOT_FOUND;
  } else if (!updateStatus.ok()) {
    LOGE("Failed to update consumer policy for data flow %" PRIu32 ": %s",
         dataFlowId, updateStatus.str());
    return toChreStatus(updateStatus);
  }

  return CHRE_STATUS_OK;
}

uint32_t DataFlowManager::sourceReserve(Nanoapp *nanoapp, uint32_t dataFlowId,
                                        uint32_t numBytes, void **data,
                                        uint32_t *reservedBytes) {
  if (data == nullptr || reservedBytes == nullptr) {
    return CHRE_STATUS_INVALID_ARGUMENT;
  }

  NanoappDataFlow *foundDataFlow = nullptr;
  uint32_t status =
      getNanoappDataFlow(dataFlowId, nanoapp->getInstanceId(), &foundDataFlow);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  pw::Result<pw::ByteSpan> reserveResult = std::visit(
      [numBytes](auto &&producer) -> pw::Result<pw::ByteSpan> {
        using T = std::decay_t<decltype(producer)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return pw::Status::FailedPrecondition();
        } else if constexpr (std::is_same_v<T, UntypedProducer>) {
          if (numBytes % producer.getElementSize() != 0) {
            return pw::Status::InvalidArgument();
          }
          return producer.reserve(numBytes / producer.getElementSize());
        } else {
          return producer.reserve(numBytes);
        }
      },
      foundDataFlow->producer);
  if (reserveResult.ok()) {
    *data = reserveResult.value().data();
    *reservedBytes = reserveResult.value().size();
    return CHRE_STATUS_OK;
  }

  *data = nullptr;
  *reservedBytes = 0;
  return toChreStatus(reserveResult.status());
}

uint32_t DataFlowManager::sourceCommit(Nanoapp *nanoapp, uint32_t dataFlowId,
                                       uint32_t numBytes) {
  NanoappDataFlow *foundDataFlow = nullptr;
  uint32_t status =
      getNanoappDataFlow(dataFlowId, nanoapp->getInstanceId(), &foundDataFlow);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  pw::Status commitStatus = std::visit(
      [numBytes](auto &&producer) -> pw::Status {
        using T = std::decay_t<decltype(producer)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return pw::Status::FailedPrecondition();
        } else if constexpr (std::is_same_v<T, UntypedProducer>) {
          if (numBytes % producer.getElementSize() != 0) {
            return pw::Status::InvalidArgument();
          }
          return producer.commit(numBytes / producer.getElementSize());
        } else {
          if (numBytes > 0) {
            return producer.commit();
          }
          return pw::OkStatus();
        }
      },
      foundDataFlow->producer);

  if (commitStatus.IsOutOfRange()) {
    return CHRE_STATUS_INVALID_ARGUMENT;
  }
  return toChreStatus(commitStatus);
}

uint32_t DataFlowManager::variableSourceTruncate(Nanoapp * /*nanoapp*/,
                                                 uint32_t /*dataFlowId*/,
                                                 uint32_t /*size*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::sourcePush(Nanoapp *nanoapp, uint32_t dataFlowId,
                                     const void *data, uint32_t numBytes,
                                     bool allOrNothing,
                                     uint32_t *numberOfBytesPushed) {
  if (data == nullptr || numberOfBytesPushed == nullptr || numBytes == 0) {
    return CHRE_STATUS_INVALID_ARGUMENT;
  }

  NanoappDataFlow *foundDataFlow = nullptr;
  uint32_t status =
      getNanoappDataFlow(dataFlowId, nanoapp->getInstanceId(), &foundDataFlow);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  pw::Result<size_t> pushResult = std::visit(
      [data, numBytes, allOrNothing](auto &&producer) -> pw::Result<size_t> {
        using T = std::decay_t<decltype(producer)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return pw::Status::FailedPrecondition();
        } else if constexpr (std::is_same_v<T, UntypedProducer>) {
          if (numBytes % producer.getElementSize() != 0) {
            return pw::Status::InvalidArgument();
          }
          auto res =
              producer.push(pw::span<const std::byte>(
                                static_cast<const std::byte *>(data), numBytes),
                            allOrNothing);
          if (res.ok()) {
            return res.value() * producer.getElementSize();
          }
          return res.status();
        } else {
          pw::Status producerStatus = producer.push(pw::span<const std::byte>(
              static_cast<const std::byte *>(data), numBytes));
          if (producerStatus.ok()) {
            return numBytes;
          }
          return producerStatus;
        }
      },
      foundDataFlow->producer);

  if (pushResult.ok()) {
    *numberOfBytesPushed = pushResult.value();
    return CHRE_STATUS_OK;
  }

  if (pushResult.status().IsUnavailable() ||
      pushResult.status().IsResourceExhausted()) {
    if (allOrNothing) {
      return CHRE_STATUS_RESOURCE_EXHAUSTED;
    } else {
      *numberOfBytesPushed = 0;
      return CHRE_STATUS_OK;
    }
  }

  return toChreStatus(pushResult.status());
}

uint32_t DataFlowManager::sourceGetSize(Nanoapp *nanoapp, uint32_t dataFlowId,
                                        bool includeReserved, uint32_t *size) {
  if (size == nullptr) {
    return CHRE_STATUS_INVALID_ARGUMENT;
  }

  NanoappDataFlow *foundDataFlow = nullptr;
  uint32_t status =
      getNanoappDataFlow(dataFlowId, nanoapp->getInstanceId(), &foundDataFlow);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  pw::Result<size_t> result = std::visit(
      [includeReserved](auto &&producer) -> pw::Result<size_t> {
        using T = std::decay_t<decltype(producer)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return pw::Status::FailedPrecondition();
        } else if constexpr (std::is_same_v<T, UntypedProducer>) {
          return producer.size(includeReserved) * producer.getElementSize();
        } else {
          return producer.size(includeReserved);
        }
      },
      foundDataFlow->producer);

  if (!result.ok()) {
    return toChreStatus(result.status());
  }

  *size = result.value();
  return CHRE_STATUS_OK;
}

uint32_t DataFlowManager::sourceGetCapacity(Nanoapp *nanoapp,
                                            uint32_t dataFlowId,
                                            uint32_t *capacity) {
  if (capacity == nullptr) {
    return CHRE_STATUS_INVALID_ARGUMENT;
  }

  NanoappDataFlow *foundDataFlow = nullptr;
  uint32_t status =
      getNanoappDataFlow(dataFlowId, nanoapp->getInstanceId(), &foundDataFlow);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  pw::Result<size_t> result = std::visit(
      [](auto &&producer) -> pw::Result<size_t> {
        using T = std::decay_t<decltype(producer)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return pw::Status::FailedPrecondition();
        } else if constexpr (std::is_same_v<T, UntypedProducer>) {
          return producer.capacity() * producer.getElementSize();
        } else {
          return producer.capacity();
        }
      },
      foundDataFlow->producer);

  if (!result.ok()) {
    return toChreStatus(result.status());
  }

  *capacity = result.value();
  return CHRE_STATUS_OK;
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

uint32_t DataFlowManager::sinkPop(Nanoapp * /*nanoapp*/, uint64_t /*hubId*/,
                                  uint32_t /*dataFlowId*/, void * /*data*/,
                                  uint32_t * /*numBytes*/) {
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

uint32_t DataFlowManager::variableSinkGetHeadSize(Nanoapp * /*nanoapp*/,
                                                  uint64_t /*hubId*/,
                                                  uint32_t /*dataFlowId*/,
                                                  uint32_t * /*size*/) {
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
    DataFlowSinkRegistration &&registration) {
  if (mSinks.full()) {
    LOG_OOM();
    return;
  }

  EventLoop *eventLoop = nullptr;
  Nanoapp *nanoapp = getNanoapp(registration.sinkId.messageHubId,
                                registration.sinkId.endpointId, &eventLoop);
  if (nanoapp == nullptr) {
    LOGE("Received sink registration for unknown nanoapp 0x%" PRIx64
         ": 0x%" PRIx64,
         registration.sinkId.messageHubId, registration.sinkId.endpointId);
    return;
  }

  UniquePtr<NanoappSinkRegistrationWithMessage> registrationWithMessage =
      nullptr;
  UniquePtr<chreDataFlowSinkInfo> registrationInfo = nullptr;
  if (registration.sessionMessage.has_value()) {
    registrationWithMessage = MakeUnique<NanoappSinkRegistrationWithMessage>();
    if (registrationWithMessage == nullptr) {
      LOG_OOM();
      return;
    }
  } else {
    registrationInfo = MakeUnique<chreDataFlowSinkInfo>();
    if (registrationInfo == nullptr) {
      LOG_OOM();
      return;
    }
  }

  SharedDataRegionManager::RegionGuard primaryRegionGuard(
      registration.primaryRegionId);
  if (!primaryRegionGuard.isValid()) {
    LOGE(
        "Failed to get primary region for sink registration with region ID: "
        "%" PRId32,
        registration.primaryRegionId);
    return;
  }

  SharedDataRegionManager::RegionGuard sinkMetadataRegionGuard;
  if (registration.sinkMetadataRegionId != kInvalidRegionId &&
      registration.sinkMetadataRegionId != registration.primaryRegionId) {
    sinkMetadataRegionGuard =
        SharedDataRegionManager::RegionGuard(registration.sinkMetadataRegionId);
    if (!sinkMetadataRegionGuard.isValid()) {
      LOGE(
          "Failed to get sink metadata region for sink registration with "
          "region "
          "ID: %" PRId32,
          registration.sinkMetadataRegionId);
      return;
    }
  }

  uint32_t elementSize = 0;
  uint32_t alignment = 0;
  std::variant<std::monostate, UntypedConsumer, VariableDataConsumer> consumer =
      createConsumer(registration, nanoapp, primaryRegionGuard,
                     sinkMetadataRegionGuard, elementSize, alignment);
  if (std::holds_alternative<std::monostate>(consumer)) {
    return;
  }

  uint16_t instanceId = nanoapp->getInstanceId();
  NanoappDataFlowSink sink = {
      .sourceHubId = registration.sourceId.messageHubId,
      .sourceEndpointId = registration.sourceId.endpointId,
      .dataFlowId = registration.dataFlowId.id,
      .metadataOffset = registration.metadataOffset,
      .sinkMetadataOffset = registration.sinkMetadataOffset,
      .elementSize = elementSize,
      .alignment = alignment,
      .nanoappInstanceId = instanceId,
      .isActive = false,
      .primaryRegionGuard = std::move(primaryRegionGuard),
      .sinkMetadataRegionGuard = std::move(sinkMetadataRegionGuard),
      .consumer = std::move(consumer),
  };

  // Populate the registration event data.
  if (registration.sessionMessage.has_value()) {
    registrationWithMessage->info.hubId = sink.sourceHubId;
    registrationWithMessage->info.endpointId = sink.sourceEndpointId;
    registrationWithMessage->info.dataFlowId = sink.dataFlowId;
    registrationWithMessage->info.elementSize = sink.elementSize;
    registrationWithMessage->info.alignment = sink.alignment;
    registrationWithMessage->info.messageFromEndpointData =
        &registrationWithMessage->sessionMessage;
    registrationWithMessage->sessionMessage.messageType =
        registration.sessionMessage->messageType;
    registrationWithMessage->sessionMessage.messagePermissions =
        registration.sessionMessage->messagePermissions;
    registrationWithMessage->sessionMessage.message =
        registration.sessionMessage->data.get();
    registrationWithMessage->sessionMessage.messageSize =
        registration.sessionMessage->data.size();
    registrationWithMessage->sessionMessage.sessionId =
        registration.sessionMessage->sessionId;
    registrationWithMessage->messageData =
        std::move(registration.sessionMessage->data);
    registrationWithMessage->nanoappInstanceId = instanceId;
  } else {
    registrationInfo->hubId = sink.sourceHubId;
    registrationInfo->endpointId = sink.sourceEndpointId;
    registrationInfo->dataFlowId = sink.dataFlowId;
    registrationInfo->elementSize = sink.elementSize;
    registrationInfo->alignment = sink.alignment;
    registrationInfo->messageFromEndpointData = nullptr;
  }

  mSinks.push_back(std::move(sink));

  // Send the registration event to the nanoapp.
  if (registration.sessionMessage.has_value()) {
    EventLoopManagerSingleton::get()->deferCallback(
        SystemCallbackType::DataFlowSinkRegisteredWithMessageEvent,
        std::move(registrationWithMessage),
        [](SystemCallbackType /* type */,
           UniquePtr<NanoappSinkRegistrationWithMessage> &&data) {
          EventLoop *eventLoop = getCurrentEventLoop();
          CHRE_ASSERT(eventLoop != nullptr);
          eventLoop->distributeEventSync(CHRE_EVENT_DATA_FLOW_SINK_CREATED,
                                         &data->info, data->nanoappInstanceId);
          // The destructor of NanoappSinkRegistrationWithMessage will be
          // called, which will release the message data.
        },
        eventLoop);
  } else {
    EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(
        CHRE_EVENT_DATA_FLOW_SINK_CREATED, registrationInfo.release(),
        freeEventDataCallback, instanceId);
  }
}

void DataFlowManager::onDataFlowSinkUnregistered(
    const chre::message::DataFlowSinkUnregistration &unregistration) {
  auto *nanoappDataFlow = findNanoappDataFlow(unregistration.dataFlowId);
  if (!nanoappDataFlow) {
    LOGE(
        "Received data flow sink unregistration for unknown data flow: "
        "%" PRIu32,
        unregistration.dataFlowId.id);
    return;
  }

  UniquePtr<chreDataFlowSinkInfo> event = MakeUnique<chreDataFlowSinkInfo>();
  if (event == nullptr) {
    LOG_OOM();
    return;
  }
  *event = {.hubId = unregistration.endpoint.messageHubId,
            .endpointId = unregistration.endpoint.endpointId,
            .dataFlowId = unregistration.dataFlowId.id};

  EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(
      CHRE_EVENT_DATA_FLOW_SINK_STOPPED, event.release(), freeEventDataCallback,
      nanoappDataFlow->nanoappInstanceId);
}

void DataFlowManager::onDataFlowStopped(
    const chre::message::DataFlowStopped & /*stopped*/) {
  // TODO(b/457453613): Implement this function.
}

void DataFlowManager::onDataFlowAlert(const DataFlowAlert &alert) {
  for (const Endpoint &endpoint : alert.receiverEndpoints) {
    uint16_t instanceId;
    auto nanoappDataFlow = findNanoappDataFlow(alert.dataFlowId);
    if (!nanoappDataFlow) {
      LOGE("Received data flow alert for unknown source: 0x%" PRIx64
           " : 0x%" PRIx64 " : on data flow: %" PRIu32,
           endpoint.messageHubId, endpoint.endpointId, alert.dataFlowId.id);
      continue;
    }
    instanceId = nanoappDataFlow->nanoappInstanceId;
    // TODO(b/457453613): Handle data flow alerts for nanoapp sinks.

    UniquePtr<chreDataFlowNewDataAlert> event =
        MakeUnique<chreDataFlowNewDataAlert>();
    if (event == nullptr) {
      LOG_OOM();
      continue;
    }
    event->hubId = alert.dataFlowId.hubId;
    event->dataFlowId = alert.dataFlowId.id;

    EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(
        CHRE_EVENT_DATA_FLOW_ALERT, event.release(), freeEventDataCallback,
        instanceId);
  }
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

Nanoapp *DataFlowManager::getNanoapp(uint64_t hubId, uint64_t endpointId,
                                     EventLoop **eventLoopOut) {
  if (hubId != ChreMessageHubManager::kChreMessageHubId) {
    return nullptr;
  }

  EventLoop *eventLoop =
      EventLoopManagerSingleton::get()->getEventLoopByAppId(endpointId);
  if (eventLoopOut != nullptr) {
    *eventLoopOut = eventLoop;
  }
  if (eventLoop == nullptr) {
    return nullptr;
  }

  return eventLoop->findNanoappByAppId(endpointId);
}

void DataFlowManager::sendDataFlowAlertToRemoteSource(
    uint32_t dataFlowId, uint64_t sourceHubId, uint64_t sourceEndpointId) {
  Endpoint receiverEndpoint(sourceHubId, sourceEndpointId);
  DataFlowAlert alert = {
      .dataFlowId =
          {
              .hubId = sourceHubId,
              .id = dataFlowId,
          },
      .receiverEndpoints = pw::span<Endpoint>(&receiverEndpoint, 1),
      .waking = true,
  };
  EventLoopManagerSingleton::get()
      ->getChreMessageHubManager()
      .getMessageHub()
      .reportDataFlowAlert(alert);
}

void DataFlowManager::sendDataFlowAlertToRemoteSink(uint32_t dataFlowId,
                                                    uint64_t sinkHubId,
                                                    uint64_t sinkEndpointId) {
  Endpoint receiverEndpoint(sinkHubId, sinkEndpointId);
  DataFlowAlert alert = {
      .dataFlowId =
          {
              .hubId = ChreMessageHubManager::kChreMessageHubId,
              .id = dataFlowId,
          },
      .receiverEndpoints = pw::span<Endpoint>(&receiverEndpoint, 1),
      .waking = true,
  };
  EventLoopManagerSingleton::get()
      ->getChreMessageHubManager()
      .getMessageHub()
      .reportDataFlowAlert(alert);
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

std::variant<std::monostate, UntypedConsumer, VariableDataConsumer>
DataFlowManager::createConsumer(
    const DataFlowSinkRegistration &registration, const Nanoapp *nanoapp,
    SharedDataRegionManager::RegionGuard &primaryRegionGuard,
    SharedDataRegionManager::RegionGuard &sinkMetadataRegionGuard,
    uint32_t &elementSize, uint32_t &alignment) {
  // TODO(b/449573597): Expose non-host helper for extracting consumer
  // information from shared memory.
  ScopedMemoryAccess scope(primaryRegionGuard.getMemoryAccess());
  auto *queue = fromOffset<Queue>(primaryRegionGuard.getRegion(),
                                  registration.metadataOffset);
  if (queue == nullptr) {
    LOGE(
        "Failed to map queue metadata for sink registration with region ID: "
        "%" PRId32,
        registration.primaryRegionId);
    return std::monostate{};
  }

  auto tag = queue->elementConfig.getTag();
  uint32_t dataFlowId = registration.dataFlowId.id;
  RemoteNotifyArgs notifyArgsOptions = {
      .fn =
          [dataFlowId](const RemoteEndpointId &remoteId) {
            EventLoopManagerSingleton::get()
                ->getDataFlowManager()
                .sendDataFlowAlertToRemoteSource(dataFlowId,
                                                 remoteId.aidlId.hubId,
                                                 remoteId.aidlId.endpointId);
          },
      .id =
          {
              .aidlId = {.hubId = static_cast<int64_t>(
                             ChreMessageHubManager::kChreMessageHubId),
                         .endpointId =
                             static_cast<int64_t>(nanoapp->getAppId())},
          },
  };

  std::optional<Region> descRegion =
      sinkMetadataRegionGuard.isValid()
          ? std::make_optional(sinkMetadataRegionGuard.getRegion())
          : std::nullopt;
  if (tag == ElementConfig::Tag::fixedSize) {
    auto consumerResult =
        android::contexthub::data_flow::UntypedConsumer::createRemote(
            primaryRegionGuard.getRegion(), descRegion,
            registration.metadataOffset, registration.sinkMetadataOffset,
            std::move(notifyArgsOptions), primaryRegionGuard.getMemoryAccess());
    if (consumerResult.ok()) {
      elementSize = consumerResult->getElementSize();
      alignment = consumerResult->getElementAlignment();
      return std::move(consumerResult.value());
    } else {
      LOGE("Failed to create untyped consumer for nanoapp 0x%" PRIx64
           " with status: %s",
           registration.sinkId.endpointId, consumerResult.status().str());
    }
  } else if (tag == ElementConfig::Tag::variableSize) {
    auto consumerResult =
        android::contexthub::data_flow::VariableDataConsumer::createRemote(
            primaryRegionGuard.getRegion(), descRegion,
            registration.metadataOffset, registration.sinkMetadataOffset,
            std::move(notifyArgsOptions), primaryRegionGuard.getMemoryAccess());
    if (consumerResult.ok()) {
      elementSize = CHRE_DATA_FLOW_ELEMENT_SIZE_VARIABLE;
      alignment = CHRE_DATA_FLOW_ELEMENT_ALIGNMENT_UNALIGNED;
      return std::move(consumerResult.value());
    } else {
      LOGE("Failed to create variable consumer for nanoapp 0x%" PRIx64
           " with status: %s",
           registration.sinkId.endpointId, consumerResult.status().str());
    }
  }

  return std::monostate{};
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

uint32_t DataFlowManager::validateAndGetSinkRequest(
    Nanoapp *nanoapp, uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy,
    NanoappDataFlow **dataFlowOut, ConsumerPolicyBuilder *policyBuilderOut) {
  if (sinkPolicy == nullptr) {
    return CHRE_STATUS_INVALID_ARGUMENT;
  }

  if (!chre::message::MessageRouterSingleton::get()
           ->getEndpointInfo(hubId, endpointId)
           .has_value()) {
    return CHRE_STATUS_INVALID_ARGUMENT;
  }

  uint32_t status =
      getNanoappDataFlow(dataFlowId, nanoapp->getInstanceId(), dataFlowOut);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  status = buildConsumerPolicy(sinkPolicy, policyBuilderOut);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  return CHRE_STATUS_OK;
}

uint32_t DataFlowManager::sourceAddSinkAsyncCommon(
    Nanoapp *nanoapp, uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy, bool hasMessage,
    void *message, size_t messageSize, uint32_t messageType, uint16_t sessionId,
    uint32_t messagePermissions, chreMessageFreeFunction *freeCallback) {
  NanoappDataFlow *foundDataFlow = nullptr;
  ConsumerPolicyBuilder policyBuilder;
  RemoteEndpointId remoteId{
      .aidlId = {.hubId = static_cast<int64_t>(hubId),
                 .endpointId = static_cast<int64_t>(endpointId)},
  };
  uint32_t status =
      validateAndGetSinkRequest(nanoapp, hubId, endpointId, dataFlowId,
                                sinkPolicy, &foundDataFlow, &policyBuilder);
  if (status != CHRE_STATUS_OK) {
    return status;
  }

  std::optional<message::Message> sessionMessage = std::nullopt;
  if (hasMessage) {
    sessionMessage =
        EventLoopManagerSingleton::get()
            ->getChreMessageHubManager()
            .createSessionMessage(message, messageSize, messageType, sessionId,
                                  messagePermissions, freeCallback,
                                  nanoapp->getAppId());
    if (!sessionMessage.has_value()) {
      return CHRE_STATUS_INVALID_ARGUMENT;
    }
  }

  auto event = MakeUnique<chreDataFlowSinkConfigureInfo>();
  if (event.isNull()) {
    LOG_OOM();
    return CHRE_STATUS_RESOURCE_EXHAUSTED;
  }

  uint32_t consumerOffset = 0;
  uint32_t metadataOffset = 0;
  pw::Status consumerStatus = std::visit(
      [&metadataOffset, &consumerOffset, &remoteId,
       &policyBuilder](auto &&producer) -> pw::Status {
        if constexpr (std::is_same_v<std::decay_t<decltype(producer)>,
                                     std::monostate>) {
          return pw::Status::FailedPrecondition();
        } else {
          metadataOffset = producer.getQueueOffset();
          auto result = producer.getConsumerManager().addConsumer(
              remoteId, policyBuilder);
          if (result.ok()) {
            consumerOffset = result.value();
          } else {
            LOGE("Failed to add consumer with hub ID 0x%" PRIx64
                 " and endpoint ID 0x%" PRIx64 ": %s",
                 remoteId.aidlId.hubId, remoteId.aidlId.endpointId,
                 result.status().str());
          }
          return result.status();
        }
      },
      foundDataFlow->producer);
  if (!consumerStatus.ok()) {
    LOGE("Failed to add consumer to data flow %" PRIu32 ": %s", dataFlowId,
         consumerStatus.str());
    return toChreStatus(consumerStatus);
  }

  // Send the registration over MessageRouter.
  chre::message::DataFlowSinkRegistration registration;
  registration.dataFlowId.hubId = ChreMessageHubManager::kChreMessageHubId;
  registration.dataFlowId.id = dataFlowId;
  registration.sourceId.messageHubId = ChreMessageHubManager::kChreMessageHubId;
  registration.sourceId.endpointId = nanoapp->getAppId();
  registration.sinkId.messageHubId = hubId;
  registration.sinkId.endpointId = endpointId;
  registration.primaryRegionId = foundDataFlow->regionId;
  registration.metadataOffset = metadataOffset;
  registration.sinkMetadataRegionId = foundDataFlow->regionId;
  registration.sinkMetadataOffset = consumerOffset;
  registration.sessionMessage = std::move(sessionMessage);
  if (!EventLoopManagerSingleton::get()
           ->getChreMessageHubManager()
           .getMessageHub()
           .registerDataFlowSink(std::move(registration))) {
    LOGE("Failed to register data flow sink with MessageRouter");

    // Prune the consumer we just added due to the failure.
    std::visit(
        [&remoteId](auto &&producer) -> void {
          if constexpr (!std::is_same_v<std::decay_t<decltype(producer)>,
                                        std::monostate>) {
            auto result = producer.getConsumerManager().pruneConsumers(
                [&remoteId](const RemoteEndpointId &matchId) {
                  return matchId.aidlId.hubId == remoteId.aidlId.hubId &&
                         matchId.aidlId.endpointId ==
                             remoteId.aidlId.endpointId;
                });
            if (!result.ok()) {
              FATAL_ERROR(
                  "Failed to prune consumer during sink registration cleanup");
            };
          }
        },
        foundDataFlow->producer);
    return CHRE_STATUS_INTERNAL;
  }

  // Send the event to the nanoapp.
  event->status = CHRE_STATUS_OK;
  event->dataFlowId = dataFlowId;
  event->hubId = hubId;
  event->endpointId = endpointId;
  EventLoopManagerSingleton::get()->postEventOrDie(
      CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE, event.release(),
      freeEventDataCallback, nanoapp->getInstanceId());

  return CHRE_STATUS_OK;
}

DataFlowManager::NanoappDataFlow *DataFlowManager::findNanoappDataFlow(
    message::DataFlowId dataFlowId) {
  if (dataFlowId.hubId != ChreMessageHubManager::kChreMessageHubId) {
    return nullptr;
  }
  for (NanoappDataFlow &dataFlow : mDataFlows) {
    if (dataFlow.properties.dataFlowId == dataFlowId.id) {
      return &dataFlow;
    }
  }
  return nullptr;
}

}  // namespace chre

#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
