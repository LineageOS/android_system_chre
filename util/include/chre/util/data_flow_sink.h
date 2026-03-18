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

#include <cstdint>
#include <type_traits>

#include "chre/util/non_copyable.h"
#include "chre_api/chre.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace chre {

/**
 * A wrapper around the CHRE API for data flow sinks. This class represents a
 * sink of fixed-size trivially copyable elements. This class is thread
 * hostile and expected to only be used in a single-threaded nanoapp context.
 */
template <typename ElementType>
class DataFlowSink : public NonCopyable {
  static_assert(std::is_trivially_copyable_v<ElementType>,
                "ElementType must be trivially copyable");

 public:
  /**
   * Enables this nanoapp to be a sink of the given data flow.
   *
   * See {@link chreDataFlowSinkEnable()} for more details.
   *
   * @param hubId The ID of the hub associated with the data flow source.
   * @param dataFlowId The ID of the data flow on which to enable the sink.
   * @return A Result containing the created DataFlowSink on success, or a
   *     status indicating the failure reason.
   */
  static pw::Result<DataFlowSink<ElementType>> create(uint64_t hubId,
                                                      uint32_t dataFlowId);

  DataFlowSink(DataFlowSink &&other);
  DataFlowSink &operator=(DataFlowSink &&other);

  /**
   * Disables this nanoapp as a sink of the data flow.
   *
   * See {@link chreDataFlowSinkDisable()} for more details.
   */
  ~DataFlowSink();

  /**
   * Gets the state of the sink on the data flow.
   *
   * See {@link chreDataFlowSinkGetState()} for more details.
   *
   * @return The state of the sink, or an error status.
   */
  pw::Status getState() const;

  /**
   * Pops the first element from the data flow.
   *
   * See {@link chreDataFlowSinkPop()} for more details.
   *
   * @return The element popped from the data flow on success, or an error
   *     status.
   */
  pw::Result<ElementType> pop() const;

  /**
   * Pops elements from the data flow into the given span.
   *
   * See {@link chreDataFlowSinkPop()} for more details.
   *
   * @param elements The span to pop elements into. The size will be the number
   * of elements popped on success.
   * @return OK on success, or an error status.
   */
  pw::Status pop(pw::span<ElementType> elements) const;

  /**
   * Returns a const view over the next contiguous block of data, up to
   * numElements.
   *
   * See {@link chreDataFlowSinkPeek()} for more details.
   *
   * @param numElements The requested number of elements to peek.
   * @return A span representing the peeked data on success, or an error status.
   *     The size of the span may be less than numElements.
   */
  pw::Result<pw::span<const ElementType>> peek(uint32_t numElements) const;

  /**
   * Releases the first numElements elements from the data flow.
   *
   * See {@link chreDataFlowSinkRelease()} for more details.
   *
   * @param numElements The number of elements to release.
   * @return OK on success, or an error status.
   */
  pw::Status release(uint32_t numElements) const;

  /**
   * Seeks the sink's read pointer on the given data flow.
   *
   * See {@link chreDataFlowSinkSeek()} for more details.
   *
   * @param offsetElements The number of elements behind the current write
   * index.
   * @return OK on success, or an error status.
   */
  pw::Status seek(uint32_t offsetElements) const;

  /**
   * Retrieve the number of elements available for this sink to read.
   *
   * See {@link chreDataFlowSinkGetOffset()} for more details.
   *
   * @return The offset in elements on success, or an error status.
   */
  pw::Result<uint32_t> getOffset() const;

  /** @return The hub ID associated with the data flow source. */
  uint64_t hubId() const {
    return mHubId;
  }

  /** @return The ID of this data flow. */
  uint32_t dataFlowId() const {
    return mDataFlowId;
  }

 private:
  DataFlowSink(uint64_t hubId, uint32_t dataFlowId)
      : mHubId(hubId), mDataFlowId(dataFlowId) {}

  uint64_t mHubId = 0;
  uint32_t mDataFlowId = CHRE_DATA_FLOW_ID_INVALID;
};

}  // namespace chre

#include "chre/util/data_flow_sink_impl.h"  // IWYU pragma: export
