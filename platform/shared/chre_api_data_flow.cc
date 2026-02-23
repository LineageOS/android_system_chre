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

#include "chre/util/macros.h"
#include "chre_api/chre/common.h"
#include "chre_api/chre/data_flow.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// TODO(b/457453613): Call this function before all CHRE data flow API calls.
// #ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
// namespace {
//
// Called prior to all CHRE data flow API calls.
// void chreDataFlowPreApiCall() {
// #ifdef CHRE_DATA_FLOW_HP_SUPPORT_ENABLED
//   forceDramAccess();
// #endif  // CHRE_DATA_FLOW_HP_SUPPORT_ENABLED
// }
//
// }  // namespace
// #endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED

DLL_EXPORT uint32_t chreDataFlowCreateAsync(
    uint32_t sinkDomains, uint64_t minAverageWriteIntervalNs,
    uint32_t maxAverageWriteBandwidthBytesPerSecond, uint32_t sinkPermissions,
    uint32_t elementSize, uint32_t alignment, uint32_t minElementCount,
    uint32_t maxElementCount, const char *name) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(sinkDomains);
  UNUSED_VAR(minAverageWriteIntervalNs);
  UNUSED_VAR(maxAverageWriteBandwidthBytesPerSecond);
  UNUSED_VAR(sinkPermissions);
  UNUSED_VAR(elementSize);
  UNUSED_VAR(alignment);
  UNUSED_VAR(minElementCount);
  UNUSED_VAR(maxElementCount);
  UNUSED_VAR(name);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowDestroy(uint32_t dataFlowId) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(dataFlowId);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSourceAddSinkAsync(
    uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(endpointId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(sinkPolicy);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSourceAddSinkOverSessionAsync(
    uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy, void *message,
    size_t messageSize, uint32_t messageType, uint16_t sessionId,
    uint32_t messagePermissions, chreMessageFreeFunction *freeCallback) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(endpointId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(sinkPolicy);
  UNUSED_VAR(message);
  UNUSED_VAR(messageSize);
  UNUSED_VAR(messageType);
  UNUSED_VAR(sessionId);
  UNUSED_VAR(messagePermissions);
  UNUSED_VAR(freeCallback);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSourceConfigureSink(
    uint64_t hubId, uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(endpointId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(sinkPolicy);
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t chreDataFlowSourceReserve(uint32_t dataFlowId, uint32_t numBytes,
                                   void **data, uint32_t *reservedBytes) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(numBytes);
  UNUSED_VAR(data);
  UNUSED_VAR(reservedBytes);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSourceCommit(uint32_t dataFlowId,
                                             uint32_t numBytes) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(numBytes);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSourcePush(uint32_t dataFlowId,
                                           const void *data, uint32_t numBytes,
                                           bool allOrNothing,
                                           uint32_t *numberOfBytesPushed) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(data);
  UNUSED_VAR(numBytes);
  UNUSED_VAR(allOrNothing);
  UNUSED_VAR(numberOfBytesPushed);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSourceGetSize(uint32_t dataFlowId,
                                              bool includeReserved,
                                              uint32_t *size) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(includeReserved);
  UNUSED_VAR(size);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSourceGetCapacity(uint32_t dataFlowId,
                                                  uint32_t *capacity) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(capacity);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSinkEnable(uint64_t hubId,
                                           uint32_t dataFlowId) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSinkDisable(uint64_t hubId,
                                            uint32_t dataFlowId) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSinkGetState(uint64_t hubId,
                                             uint32_t dataFlowId) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSinkPeek(uint64_t hubId, uint32_t dataFlowId,
                                         uint32_t numRequestedBytes,
                                         const void **data,
                                         uint32_t *numBytes) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(numRequestedBytes);
  UNUSED_VAR(data);
  UNUSED_VAR(numBytes);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSinkRelease(uint64_t hubId, uint32_t dataFlowId,
                                            uint32_t numBytes) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(numBytes);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSinkSeek(uint64_t hubId, uint32_t dataFlowId,
                                         uint32_t offset) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(offset);
  return CHRE_STATUS_UNIMPLEMENTED;
}

DLL_EXPORT uint32_t chreDataFlowSinkGetOffset(uint64_t hubId,
                                              uint32_t dataFlowId,
                                              uint32_t *offset) {
  // TODO(b/457453613): Implement this function

  UNUSED_VAR(hubId);
  UNUSED_VAR(dataFlowId);
  UNUSED_VAR(offset);
  return CHRE_STATUS_UNIMPLEMENTED;
}
