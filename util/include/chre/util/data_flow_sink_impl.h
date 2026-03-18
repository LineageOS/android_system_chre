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

#include "chre/util/data_flow_sink.h"
#include "chre/util/status.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace chre {

template <typename ElementType>
pw::Result<DataFlowSink<ElementType>> DataFlowSink<ElementType>::create(
    uint64_t hubId, uint32_t dataFlowId) {
  uint32_t status = chreDataFlowSinkEnable(hubId, dataFlowId);
  if (status == CHRE_STATUS_OK) {
    return DataFlowSink<ElementType>(hubId, dataFlowId);
  }
  return toPwStatus(status);
}

template <typename ElementType>
DataFlowSink<ElementType>::DataFlowSink(DataFlowSink &&other)
    : mHubId(other.mHubId), mDataFlowId(other.mDataFlowId) {
  other.mHubId = 0;
  other.mDataFlowId = CHRE_DATA_FLOW_ID_INVALID;
}

template <typename ElementType>
DataFlowSink<ElementType> &DataFlowSink<ElementType>::operator=(
    DataFlowSink &&other) {
  if (this != &other) {
    if (mDataFlowId != CHRE_DATA_FLOW_ID_INVALID) {
      chreDataFlowSinkDisable(mHubId, mDataFlowId);
    }
    mHubId = other.mHubId;
    mDataFlowId = other.mDataFlowId;
    other.mHubId = 0;
    other.mDataFlowId = CHRE_DATA_FLOW_ID_INVALID;
  }
  return *this;
}

template <typename ElementType>
DataFlowSink<ElementType>::~DataFlowSink() {
  if (mDataFlowId != CHRE_DATA_FLOW_ID_INVALID) {
    chreDataFlowSinkDisable(mHubId, mDataFlowId);
  }
}

template <typename ElementType>
pw::Status DataFlowSink<ElementType>::getState() const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSinkGetState(mHubId, mDataFlowId);
  return toPwStatus(status);
}

template <typename ElementType>
pw::Result<ElementType> DataFlowSink<ElementType>::pop() const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  // TODO(b/493930160): Implement this.
  return pw::Status::Unimplemented();
}

template <typename ElementType>
pw::Status DataFlowSink<ElementType>::pop(
    pw::span<ElementType> /*elements*/) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  // TODO(b/493930160): Implement this.
  return pw::Status::Unimplemented();
}

template <typename ElementType>
pw::Result<pw::span<const ElementType>> DataFlowSink<ElementType>::peek(
    uint32_t numElements) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  const void *data = nullptr;
  uint32_t numBytes = 0;
  uint32_t status = chreDataFlowSinkPeek(
      mHubId, mDataFlowId, numElements * sizeof(ElementType), &data, &numBytes);
  if (status == CHRE_STATUS_OK) {
    return pw::span<const ElementType>(static_cast<const ElementType *>(data),
                                       numBytes / sizeof(ElementType));
  }
  return toPwStatus(status);
}

template <typename ElementType>
pw::Status DataFlowSink<ElementType>::release(uint32_t numElements) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSinkRelease(mHubId, mDataFlowId,
                                            numElements * sizeof(ElementType));
  return toPwStatus(status);
}

template <typename ElementType>
pw::Status DataFlowSink<ElementType>::seek(uint32_t offsetElements) const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t status = chreDataFlowSinkSeek(mHubId, mDataFlowId,
                                         offsetElements * sizeof(ElementType));
  return toPwStatus(status);
}

template <typename ElementType>
pw::Result<uint32_t> DataFlowSink<ElementType>::getOffset() const {
  if (mDataFlowId == CHRE_DATA_FLOW_ID_INVALID) {
    return pw::Status::NotFound();
  }
  uint32_t offset = 0;
  uint32_t status = chreDataFlowSinkGetOffset(mHubId, mDataFlowId, &offset);
  if (status == CHRE_STATUS_OK) {
    return offset / sizeof(ElementType);
  }
  return toPwStatus(status);
}

}  // namespace chre
