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

#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/platform/shared/memory.h"
#include "chre/util/macros.h"
#include "chre_api/chre/common.h"
#include "chre_api/chre/data_flow.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

using ::chre::EventLoopManager;
using ::chre::EventLoopManagerSingleton;
using ::chre::forceDramAccess;
using ::chre::GlobalApiLockGuard;
using ::chre::Nanoapp;

#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
namespace {

//! Called prior to all CHRE data flow API calls.
void chreDataFlowPreApiCall() {
#ifdef CHRE_DATA_FLOW_HP_SUPPORT_ENABLED
  forceDramAccess();
#endif  // CHRE_DATA_FLOW_HP_SUPPORT_ENABLED
}

}  // namespace
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED

DLL_EXPORT uint32_t chreDataFlowCreateAsync(
    uint32_t sinkDomains, uint64_t minAverageWriteIntervalNs,
    uint32_t maxAverageWriteBandwidthBytesPerSecond, uint32_t sinkPermissions,
    uint32_t elementSize, uint32_t alignment, uint32_t minElementCount,
    uint32_t maxElementCount, const char *name, uint32_t *dataFlowId) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()
      ->getDataFlowManager()
      .createDataFlowAsync(nanoapp, sinkDomains, minAverageWriteIntervalNs,
                           maxAverageWriteBandwidthBytesPerSecond,
                           sinkPermissions, elementSize, alignment,
                           minElementCount, maxElementCount, name, dataFlowId);
#else
  UNUSED_VAR(sinkDomains);
  UNUSED_VAR(minAverageWriteIntervalNs);
  UNUSED_VAR(maxAverageWriteBandwidthBytesPerSecond);
  UNUSED_VAR(sinkPermissions);
  UNUSED_VAR(elementSize);
  UNUSED_VAR(alignment);
  UNUSED_VAR(minElementCount);
  UNUSED_VAR(maxElementCount);
  UNUSED_VAR(name);
  UNUSED_VAR(dataFlowId);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowDestroy(uint32_t dataFlowId) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().destroyDataFlow(
      nanoapp, dataFlowId);
#else
  UNUSED_VAR(dataFlowId);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSourceAddSinkAsync(
    uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()
      ->getDataFlowManager()
      .sourceAddSinkAsync(nanoapp, hubId, endpointId, dataFlowId, sinkPolicy);
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(endpointId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(sinkPolicy);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSourceAddSinkOverSessionAsync(
    uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy, void *message,
    size_t messageSize, uint32_t messageType, uint16_t sessionId,
    uint32_t messagePermissions, chreMessageFreeFunction *freeCallback) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  uint32_t success = CHRE_STATUS_UNIMPLEMENTED;
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  {
    GlobalApiLockGuard lock;
    success = EventLoopManagerSingleton::get()
                  ->getDataFlowManager()
                  .sourceAddSinkOverSessionAsync(
                      nanoapp, hubId, endpointId, dataFlowId, sinkPolicy,
                      message, messageSize, messageType, sessionId,
                      messagePermissions, freeCallback);
  }

  if (success != CHRE_STATUS_OK && freeCallback != nullptr) {
    freeCallback(message, messageSize);
  }
  return success;
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(endpointId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(sinkPolicy);
  UNUSED_VAR(message);
  UNUSED_VAR(messageSize);
  UNUSED_VAR(messageType);
  UNUSED_VAR(sessionId);
  UNUSED_VAR(messagePermissions);
  if (freeCallback != nullptr) {
    freeCallback(message, messageSize);
  }
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSourceConfigureSink(
    uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()
      ->getDataFlowManager()
      .sourceConfigureSink(nanoapp, hubId, endpointId, dataFlowId, sinkPolicy);
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(endpointId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(sinkPolicy);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

uint32_t chreDataFlowSourceReserve(uint32_t dataFlowId, uint32_t numBytes,
                                   void **data, uint32_t *reservedBytes) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sourceReserve(
      nanoapp, dataFlowId, numBytes, data, reservedBytes);
#else
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(numBytes);
  UNUSED_VAR(data);
  UNUSED_VAR(reservedBytes);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSourceCommit(uint32_t dataFlowId,
                                             uint32_t numBytes) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sourceCommit(
      nanoapp, dataFlowId, numBytes);
#else
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(numBytes);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSourcePush(uint32_t dataFlowId,
                                           const void *data, uint32_t numBytes,
                                           bool allOrNothing,
                                           uint32_t *numberOfBytesPushed) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sourcePush(
      nanoapp, dataFlowId, data, numBytes, allOrNothing, numberOfBytesPushed);
#else
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(data);
  UNUSED_VAR(numBytes);
  UNUSED_VAR(allOrNothing);
  UNUSED_VAR(numberOfBytesPushed);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSourceGetSize(uint32_t dataFlowId,
                                              bool includeReserved,
                                              uint32_t *size) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sourceGetSize(
      nanoapp, dataFlowId, includeReserved, size);
#else
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(includeReserved);
  UNUSED_VAR(size);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSourceGetCapacity(uint32_t dataFlowId,
                                                  uint32_t *capacity) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()
      ->getDataFlowManager()
      .sourceGetCapacity(nanoapp, dataFlowId, capacity);
#else
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(capacity);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSinkEnable(uint64_t hubId,
                                           uint32_t dataFlowId) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sinkEnable(
      nanoapp, hubId, dataFlowId);
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSinkDisable(uint64_t hubId,
                                            uint32_t dataFlowId) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sinkDisable(
      nanoapp, hubId, dataFlowId);
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSinkGetState(uint64_t hubId,
                                             uint32_t dataFlowId) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sinkGetState(
      nanoapp, hubId, dataFlowId);
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSinkPeek(uint64_t hubId, uint32_t dataFlowId,
                                         uint32_t numRequestedBytes,
                                         const void **data,
                                         uint32_t *numBytes) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sinkPeek(
      nanoapp, hubId, dataFlowId, numRequestedBytes, data, numBytes);
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(numRequestedBytes);
  UNUSED_VAR(data);
  UNUSED_VAR(numBytes);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSinkRelease(uint64_t hubId, uint32_t dataFlowId,
                                            uint32_t numBytes) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sinkRelease(
      nanoapp, hubId, dataFlowId, numBytes);
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(numBytes);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSinkSeek(uint64_t hubId, uint32_t dataFlowId,
                                         uint32_t offset) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sinkSeek(
      nanoapp, hubId, dataFlowId, offset);
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(offset);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

DLL_EXPORT uint32_t chreDataFlowSinkGetOffset(uint64_t hubId,
                                              uint32_t dataFlowId,
                                              uint32_t *offset) {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  chreDataFlowPreApiCall();
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()->getDataFlowManager().sinkGetOffset(
      nanoapp, hubId, dataFlowId, offset);
#else
  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(offset);
  return CHRE_STATUS_UNIMPLEMENTED;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}
