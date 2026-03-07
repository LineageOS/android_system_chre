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

namespace chre {

void DataFlowManager::init() {}

uint32_t DataFlowManager::createDataFlowAsync(
    Nanoapp * /*nanoapp*/, uint32_t /*sinkDomains*/,
    uint64_t /*minAverageWriteIntervalNs*/,
    uint32_t /*maxAverageWriteBandwidthBytesPerSecond*/,
    uint32_t /*sinkPermissions*/, uint32_t /*elementSize*/,
    uint32_t /*alignment*/, uint32_t /*minElementCount*/,
    uint32_t /*maxElementCount*/, const char * /*name*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
}

uint32_t DataFlowManager::destroyDataFlow(Nanoapp * /*nanoapp*/,
                                          uint32_t /*dataFlowId*/) {
  // TODO(b/457453613): Implement this function
  return CHRE_STATUS_UNIMPLEMENTED;
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

}  // namespace chre

#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
