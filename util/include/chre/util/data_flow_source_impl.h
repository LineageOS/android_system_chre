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

#include "chre_api/chre.h"

#include <cstdint>

#include "chre/util/data_flow_source.h"
#include "chre/util/status.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace chre {

template <typename ElementType>
pw::Result<DataFlowSource<ElementType>>
DataFlowSource<ElementType>::createAsync(
    uint32_t sinkDomains, uint64_t minAverageWriteIntervalNs,
    uint32_t maxAverageWriteBandwidthBytesPerSecond, uint32_t sinkPermissions,
    uint32_t minElementCount, uint32_t maxElementCount, const char *name) {
  uint32_t dataFlowId;
  uint32_t status = chreDataFlowCreateAsync(
      sinkDomains, minAverageWriteIntervalNs,
      maxAverageWriteBandwidthBytesPerSecond, sinkPermissions,
      sizeof(ElementType), alignof(ElementType), minElementCount,
      maxElementCount, name, &dataFlowId);
  if (status == CHRE_STATUS_OK) {
    return DataFlowSource<ElementType>(dataFlowId);
  }
  return toPwStatus(status);
}

template <typename ElementType>
DataFlowSource<ElementType>::DataFlowSource(DataFlowSource &&other)
    : mDataFlowId(other.mDataFlowId) {
  other.mDataFlowId = CHRE_DATA_FLOW_ID_INVALID;
}

template <typename ElementType>
DataFlowSource<ElementType> &DataFlowSource<ElementType>::operator=(
    DataFlowSource &&other) {
  if (this != &other) {
    if (mDataFlowId != CHRE_DATA_FLOW_ID_INVALID) {
      chreDataFlowDestroy(mDataFlowId);
    }
    mDataFlowId = other.mDataFlowId;
    other.mDataFlowId = CHRE_DATA_FLOW_ID_INVALID;
  }
  return *this;
}

template <typename ElementType>
DataFlowSource<ElementType>::~DataFlowSource() {
  if (mDataFlowId != CHRE_DATA_FLOW_ID_INVALID) {
    chreDataFlowDestroy(mDataFlowId);
  }
}

template <typename ElementType>
pw::Status DataFlowSource<ElementType>::addSinkAsync(
    uint64_t hubId, uint64_t endpointId,
    const chreDataFlowSinkPolicy &sinkPolicy) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSourceAddSinkAsync(hubId, endpointId,
                                                   mDataFlowId, &sinkPolicy);
  return toPwStatus(status);
}

template <typename ElementType>
pw::Status DataFlowSource<ElementType>::addSinkOverSessionAsync(
    uint64_t hubId, uint64_t endpointId,
    const chreDataFlowSinkPolicy &sinkPolicy, uint16_t sessionId,
    pw::ByteSpan message, uint32_t messageType, uint32_t messagePermissions,
    chreMessageFreeFunction *freeCallback) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSourceAddSinkOverSessionAsync(
      hubId, endpointId, mDataFlowId, &sinkPolicy, message.data(),
      message.size(), messageType, sessionId, messagePermissions, freeCallback);
  return toPwStatus(status);
}

template <typename ElementType>
pw::Status DataFlowSource<ElementType>::configureSink(
    uint64_t hubId, uint64_t endpointId,
    const chreDataFlowSinkPolicy &sinkPolicy) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSourceConfigureSink(hubId, endpointId,
                                                    mDataFlowId, &sinkPolicy);
  return toPwStatus(status);
}

template <typename ElementType>
pw::Status DataFlowSource<ElementType>::push(const ElementType &element) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t numberOfBytesPushed;
  uint32_t status =
      chreDataFlowSourcePush(mDataFlowId, &element, sizeof(ElementType),
                             /*allOrNothing=*/true, &numberOfBytesPushed);
  return toPwStatus(status);
}

template <typename ElementType>
pw::Result<uint32_t> DataFlowSource<ElementType>::push(
    pw::span<const ElementType> elements, bool allOrNothing) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t numberOfBytesPushed;
  uint32_t status = chreDataFlowSourcePush(mDataFlowId, elements.data(),
                                           elements.size_bytes(), allOrNothing,
                                           &numberOfBytesPushed);
  if (status == CHRE_STATUS_OK) {
    return numberOfBytesPushed / sizeof(ElementType);
  }
  return toPwStatus(status);
}

template <typename ElementType>
pw::Result<pw::span<ElementType>> DataFlowSource<ElementType>::reserve(
    uint32_t numElements) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  void *data = nullptr;
  uint32_t reservedBytes = 0;
  uint32_t status = chreDataFlowSourceReserve(
      mDataFlowId, numElements * sizeof(ElementType), &data, &reservedBytes);
  if (status == CHRE_STATUS_OK) {
    return pw::span<ElementType>(static_cast<ElementType *>(data),
                                 reservedBytes / sizeof(ElementType));
  }
  return toPwStatus(status);
}

template <typename ElementType>
pw::Status DataFlowSource<ElementType>::commit(uint32_t numElements) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status =
      chreDataFlowSourceCommit(mDataFlowId, numElements * sizeof(ElementType));
  return toPwStatus(status);
}

template <typename ElementType>
pw::Result<uint32_t> DataFlowSource<ElementType>::size(
    bool includeReserved) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t sizeInBytes = 0;
  uint32_t status =
      chreDataFlowSourceGetSize(mDataFlowId, includeReserved, &sizeInBytes);
  if (status == CHRE_STATUS_OK) {
    return sizeInBytes / sizeof(ElementType);
  }
  return toPwStatus(status);
}

template <typename ElementType>
pw::Result<uint32_t> DataFlowSource<ElementType>::capacity() const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t capacityInBytes = 0;
  uint32_t status =
      chreDataFlowSourceGetCapacity(mDataFlowId, &capacityInBytes);
  if (status == CHRE_STATUS_OK) {
    return capacityInBytes / sizeof(ElementType);
  }
  return toPwStatus(status);
}

inline pw::Result<VariableDataFlowSource> VariableDataFlowSource::createAsync(
    uint32_t sinkDomains, uint64_t minAverageWriteIntervalNs,
    uint32_t maxAverageWriteBandwidthBytesPerSecond, uint32_t sinkPermissions,
    uint32_t minByteCount, uint32_t maxByteCount, const char *name) {
  uint32_t dataFlowId;
  uint32_t status = chreDataFlowCreateAsync(
      sinkDomains, minAverageWriteIntervalNs,
      maxAverageWriteBandwidthBytesPerSecond, sinkPermissions,
      CHRE_DATA_FLOW_ELEMENT_SIZE_VARIABLE,
      CHRE_DATA_FLOW_ELEMENT_ALIGNMENT_UNALIGNED, minByteCount, maxByteCount,
      name, &dataFlowId);
  if (status == CHRE_STATUS_OK) {
    return VariableDataFlowSource(dataFlowId);
  }
  return toPwStatus(status);
}

inline VariableDataFlowSource::VariableDataFlowSource(
    VariableDataFlowSource &&other)
    : mDataFlowId(other.mDataFlowId) {
  other.mDataFlowId = CHRE_DATA_FLOW_ID_INVALID;
}

inline VariableDataFlowSource &VariableDataFlowSource::operator=(
    VariableDataFlowSource &&other) {
  if (this != &other) {
    if (mDataFlowId != CHRE_DATA_FLOW_ID_INVALID) {
      chreDataFlowDestroy(mDataFlowId);
    }
    mDataFlowId = other.mDataFlowId;
    other.mDataFlowId = CHRE_DATA_FLOW_ID_INVALID;
  }
  return *this;
}

inline VariableDataFlowSource::~VariableDataFlowSource() {
  if (mDataFlowId != CHRE_DATA_FLOW_ID_INVALID) {
    chreDataFlowDestroy(mDataFlowId);
  }
}

inline pw::Status VariableDataFlowSource::addSinkAsync(
    uint64_t hubId, uint64_t endpointId,
    const chreDataFlowSinkPolicy &sinkPolicy) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSourceAddSinkAsync(hubId, endpointId,
                                                   mDataFlowId, &sinkPolicy);
  return toPwStatus(status);
}

inline pw::Status VariableDataFlowSource::addSinkOverSessionAsync(
    uint64_t hubId, uint64_t endpointId,
    const chreDataFlowSinkPolicy &sinkPolicy, uint16_t sessionId,
    pw::ByteSpan message, uint32_t messageType, uint32_t messagePermissions,
    chreMessageFreeFunction *freeCallback) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSourceAddSinkOverSessionAsync(
      hubId, endpointId, mDataFlowId, &sinkPolicy, message.data(),
      message.size(), messageType, sessionId, messagePermissions, freeCallback);
  return toPwStatus(status);
}

inline pw::Status VariableDataFlowSource::configureSink(
    uint64_t hubId, uint64_t endpointId,
    const chreDataFlowSinkPolicy &sinkPolicy) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSourceConfigureSink(hubId, endpointId,
                                                    mDataFlowId, &sinkPolicy);
  return toPwStatus(status);
}

inline pw::Status VariableDataFlowSource::push(
    pw::ConstByteSpan element) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t numberOfBytesPushed;
  uint32_t status =
      chreDataFlowSourcePush(mDataFlowId, element.data(), element.size(),
                             /*allOrNothing=*/true, &numberOfBytesPushed);
  return toPwStatus(status);
}

inline pw::Result<pw::ByteSpan> VariableDataFlowSource::reserve(
    uint32_t numBytes) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  void *data = nullptr;
  uint32_t reservedBytes = 0;
  uint32_t status =
      chreDataFlowSourceReserve(mDataFlowId, numBytes, &data, &reservedBytes);
  if (status == CHRE_STATUS_OK) {
    return pw::ByteSpan(static_cast<std::byte *>(data), reservedBytes);
  }
  return toPwStatus(status);
}

inline pw::Status VariableDataFlowSource::truncate(
    uint32_t /*numBytes*/) const {
  // TODO(b/493930160): Implement this.
  return pw::Status::Unimplemented();
}

inline pw::Status VariableDataFlowSource::commit() const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSourceCommit(mDataFlowId, 0 /* numBytes */);
  return toPwStatus(status);
}

inline pw::Result<uint32_t> VariableDataFlowSource::size(
    bool includeReserved) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t sizeInBytes = 0;
  uint32_t status =
      chreDataFlowSourceGetSize(mDataFlowId, includeReserved, &sizeInBytes);
  if (status == CHRE_STATUS_OK) {
    return sizeInBytes;
  }
  return toPwStatus(status);
}

inline pw::Result<uint32_t> VariableDataFlowSource::capacity() const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t capacityInBytes = 0;
  uint32_t status =
      chreDataFlowSourceGetCapacity(mDataFlowId, &capacityInBytes);
  if (status == CHRE_STATUS_OK) {
    return capacityInBytes;
  }
  return toPwStatus(status);
}

}  // namespace chre