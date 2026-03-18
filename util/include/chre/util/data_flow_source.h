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
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace chre {

/**
 * A wrapper around the CHRE API for data flow sources. This class represents a
 * source of fixed-size trivially copyable elements. This class is thread
 * hostile and expected to only be used in a single-threaded nanoapp context.
 */
template <typename ElementType>
class DataFlowSource : public NonCopyable {
  static_assert(std::is_trivially_copyable_v<ElementType>,
                "ElementType must be trivially copyable");

 public:
  /**
   * Creates a data flow with the given properties.
   *
   * NOTE: On success, the returned DataFlowSource is not valid until the
   * nanoapp receives a CHRE_EVENT_DATA_FLOW_CREATED event where
   * chreDataFlowCreatedInfo.dataFlowId == dataFlowId().
   *
   * See {@link chreDataFlowCreateAsync()} for more details.
   *
   * @param sinkDomains A bitmask of CHRE_DATA_FLOW_SINK_DOMAIN_* values.
   * @param minAverageWriteIntervalNs The expected minimum average interval
   *     between writes in nanoseconds.
   * @param maxAverageWriteBandwidthBytesPerSecond The expected maximum
   * average write bandwidth in bytes per second.
   * @param sinkPermissions Bitmask of permissions for receiving data.
   * @param minElementCount The minimum number of elements to allocate.
   * @param maxElementCount The maximum number of elements to allocate.
   * @param name A human-readable name for the data flow.
   * @return A Result containing the created DataFlowSource on success, or a
   *     status indicating the failure reason.
   */
  static pw::Result<DataFlowSource<ElementType>> createAsync(
      uint32_t sinkDomains, uint64_t minAverageWriteIntervalNs,
      uint32_t maxAverageWriteBandwidthBytesPerSecond, uint32_t sinkPermissions,
      uint32_t minElementCount, uint32_t maxElementCount, const char *name);

  DataFlowSource(DataFlowSource &&other);
  DataFlowSource &operator=(DataFlowSource &&other);

  /**
   * Destroys the data flow.
   *
   * See {@link chreDataFlowDestroy()} for more details.
   */
  ~DataFlowSource();

  /**
   * Creates a sink on the data flow owned by the nanoapp.
   *
   * See {@link chreDataFlowSourceAddSinkAsync()} for more details.
   *
   * @param hubId The sink's message hub ID.
   * @param endpointId The sink's endpoint ID.
   * @param sinkPolicy The sink policy for the sink.
   * @return OK if the request was successfully queued, or an error status.
   */
  pw::Status addSinkAsync(uint64_t hubId, uint64_t endpointId,
                          const chreDataFlowSinkPolicy &sinkPolicy) const;

  /**
   * Creates a sink on a data flow owned by the calling nanoapp, delivering the
   * metadata for the new sink alongside message payload.
   *
   * See {@link chreDataFlowSourceAddSinkOverSessionAsync()} for more details.
   *
   * @param hubId The sink's message hub ID.
   * @param endpointId The sink's endpoint ID.
   * @param sinkPolicy The sink policy for the sink.
   * @param sessionId The session over which to send this message.
   * @param message Span over the message payload.
   * @param messageType An opaque value passed along with the message payload.
   * @param messagePermissions Bitmask of permissions.
   * @param freeCallback Invoked when the system no longer needs the memory.
   * @return OK if the request was successfully queued, or an error status.
   */
  pw::Status addSinkOverSessionAsync(
      uint64_t hubId, uint64_t endpointId,
      const chreDataFlowSinkPolicy &sinkPolicy, uint16_t sessionId,
      pw::ByteSpan message, uint32_t messageType, uint32_t messagePermissions,
      chreMessageFreeFunction *freeCallback) const;

  /**
   * Configures an existing sink on a data flow owned by the calling nanoapp.
   *
   * See {@link chreDataFlowSourceConfigureSink()} for more details.
   *
   * @param hubId The sink's message hub ID.
   * @param endpointId The sink's endpoint ID.
   * @param sinkPolicy The sink policy for the sink.
   * @return OK if the request was successful, or an error status.
   */
  pw::Status configureSink(uint64_t hubId, uint64_t endpointId,
                           const chreDataFlowSinkPolicy &sinkPolicy) const;

  /**
   * Pushes a single element into the data flow.
   *
   * See {@link chreDataFlowSourcePush()} for more details.
   *
   * @param element The element to push.
   * @return OK on success, or an error status.
   */
  pw::Status push(const ElementType &element) const;

  /**
   * Pushes a span of elements into the data flow.
   *
   * See {@link chreDataFlowSourcePush()} for more details.
   *
   * @param elements The elements to push.
   * @param allOrNothing If true, either all or none of the elements are pushed.
   *     If false, as many elements as possible are pushed.
   * @return The number of elements pushed on success, or an error status.
   */
  pw::Result<uint32_t> push(pw::span<const ElementType> elements,
                            bool allOrNothing) const;

  /**
   * Reserves contiguous space in the data flow for numElements elements.
   *
   * See {@link chreDataFlowSourceReserve()} for more details.
   *
   * @param numElements The number of elements to reserve space for.
   * @return A span representing the reserved memory on success, or an error
   *     status. The size of the span may be less than numElements.
   */
  pw::Result<pw::span<ElementType>> reserve(uint32_t numElements) const;

  /**
   * Releases the first numElements elements reserved for writing.
   *
   * See {@link chreDataFlowSourceCommit()} for more details.
   *
   * @param numElements The number of elements to release.
   * @return OK on success, or an error status.
   */
  pw::Status commit(uint32_t numElements) const;

  /**
   * Returns the current depth of the data flow in number of elements.
   *
   * See {@link chreDataFlowSourceGetSize()} for more details.
   *
   * @param includeReserved If true, include reserved elements in the count.
   * @return The depth of the data flow on success, or an error status.
   */
  pw::Result<uint32_t> size(bool includeReserved) const;

  /**
   * Returns the capacity of the data flow in number of elements.
   *
   * See {@link chreDataFlowSourceGetCapacity()} for more details.
   *
   * @return The capacity of the data flow on success, or an error status.
   */
  pw::Result<uint32_t> capacity() const;

  /** @return The id of this data flow. */
  uint32_t dataFlowId() const {
    return mDataFlowId;
  }

 private:
  explicit DataFlowSource(uint32_t dataFlowId) : mDataFlowId(dataFlowId) {}

  uint32_t mDataFlowId = CHRE_DATA_FLOW_ID_INVALID;
};

/**
 * A wrapper around the CHRE API for data flow sources with variable size
 * elements. This class is thread hostile and expected to only be used in a
 * single-threaded nanoapp context.
 */
class VariableDataFlowSource : public NonCopyable {
 public:
  /**
   * Creates a data flow with the given properties.
   *
   * NOTE: On success, the returned VariableDataFlowSource is not valid until
   * the nanoapp receives a CHRE_EVENT_DATA_FLOW_CREATED event where
   * chreDataFlowCreatedInfo.dataFlowId == dataFlowId().
   *
   * See {@link chreDataFlowCreateAsync()} for more details.
   *
   * @param sinkDomains A bitmask of CHRE_DATA_FLOW_SINK_DOMAIN_* values.
   * @param minAverageWriteIntervalNs The expected minimum average interval
   *     between writes in nanoseconds.
   * @param maxAverageWriteBandwidthBytesPerSecond The expected maximum
   * average write bandwidth in bytes per second.
   * @param sinkPermissions Bitmask of permissions for receiving data.
   * @param minByteCount The minimum number of bytes to allocate.
   * @param maxByteCount The maximum number of bytes to allocate.
   * @param name A human-readable name for the data flow.
   * @return A Result containing the created VariableDataFlowSource on success,
   *     or a status indicating the failure reason.
   */
  static pw::Result<VariableDataFlowSource> createAsync(
      uint32_t sinkDomains, uint64_t minAverageWriteIntervalNs,
      uint32_t maxAverageWriteBandwidthBytesPerSecond, uint32_t sinkPermissions,
      uint32_t minByteCount, uint32_t maxByteCount, const char *name);

  VariableDataFlowSource(VariableDataFlowSource &&other);
  VariableDataFlowSource &operator=(VariableDataFlowSource &&other);

  /**
   * Destroys the data flow.
   *
   * See {@link chreDataFlowDestroy()} for more details.
   */
  ~VariableDataFlowSource();

  /**
   * Creates a sink on the data flow owned by the nanoapp.
   *
   * See {@link chreDataFlowSourceAddSinkAsync()} for more details.
   *
   * @param hubId The sink's message hub ID.
   * @param endpointId The sink's endpoint ID.
   * @param sinkPolicy The sink policy for the sink.
   * @return OK if the request was successfully queued, or an error status.
   */
  pw::Status addSinkAsync(uint64_t hubId, uint64_t endpointId,
                          const chreDataFlowSinkPolicy &sinkPolicy) const;

  /**
   * Creates a sink on a data flow owned by the calling nanoapp, delivering the
   * metadata for the new sink alongside message payload.
   *
   * See {@link chreDataFlowSourceAddSinkOverSessionAsync()} for more details.
   *
   * @param hubId The sink's message hub ID.
   * @param endpointId The sink's endpoint ID.
   * @param sinkPolicy The sink policy for the sink.
   * @param sessionId The session over which to send this message.
   * @param message Span over the message payload.
   * @param messageType An opaque value passed along with the message payload.
   * @param messagePermissions Bitmask of permissions.
   * @param freeCallback Invoked when the system no longer needs the memory.
   * @return OK if the request was successfully queued, or an error status.
   */
  pw::Status addSinkOverSessionAsync(
      uint64_t hubId, uint64_t endpointId,
      const chreDataFlowSinkPolicy &sinkPolicy, uint16_t sessionId,
      pw::ByteSpan message, uint32_t messageType, uint32_t messagePermissions,
      chreMessageFreeFunction *freeCallback) const;

  /**
   * Configures an existing sink on a data flow owned by the calling nanoapp.
   *
   * See {@link chreDataFlowSourceConfigureSink()} for more details.
   *
   * @param hubId The sink's message hub ID.
   * @param endpointId The sink's endpoint ID.
   * @param sinkPolicy The sink policy for the sink.
   * @return OK if the request was successful, or an error status.
   */
  pw::Status configureSink(uint64_t hubId, uint64_t endpointId,
                           const chreDataFlowSinkPolicy &sinkPolicy) const;

  /**
   * Pushes a single variable size element into the data flow.
   *
   * See {@link chreDataFlowSourcePush()} for more details.
   *
   * @param element The element to push.
   * @return OK on success, or an error status.
   */
  pw::Status push(pw::ConstByteSpan element) const;

  /**
   * Reserves up to numBytes bytes of contiguous space for the next element.
   *
   * This can also be used to increase the size of the current reserved element
   * by numBytes.
   *
   * See {@link chreDataFlowSourceReserve()} for more details.
   *
   * @param numBytes The number of bytes to reserve space for.
   * @return A span representing the reserved memory on success, or an error
   *     status. The size of the span may be less than numBytes, but success
   *     indicates that the requested space is available, just not contiguous.
   */
  pw::Result<pw::ByteSpan> reserve(uint32_t numBytes) const;

  /**
   * Truncates the size of the current reserved element to numBytes.
   *
   * NOTE: Truncating to 0 bytes abandons the current reserved element.
   *
   * See {@link chreDataFlowVariableSizeSourceTruncate()} for more details.
   *
   * @param numBytes The number of bytes to truncate to. Must be less than or
   * equal to the current reserved size.
   * @return OK on success, or an error status.
   */
  pw::Status truncate(uint32_t numBytes) const;

  /**
   * Commits the current reserved element to the data flow.
   *
   * See {@link chreDataFlowSourceCommit()} for more details.
   *
   * @return OK on success, or an error status.
   */
  pw::Status commit() const;

  /**
   * Returns the current depth of the data flow in number of bytes.
   *
   * See {@link chreDataFlowSourceGetSize()} for more details.
   *
   * @param includeReserved If true, include reserved bytes in the count.
   * @return The depth of the data flow on success, or an error status.
   */
  pw::Result<uint32_t> size(bool includeReserved) const;

  /**
   * Returns the capacity of the data flow in number of bytes.
   *
   * See {@link chreDataFlowSourceGetCapacity()} for more details.
   *
   * @return The capacity of the data flow on success, or an error status.
   */
  pw::Result<uint32_t> capacity() const;

  /** @return The id of this data flow. */
  uint32_t dataFlowId() const {
    return mDataFlowId;
  }

 private:
  explicit VariableDataFlowSource(uint32_t dataFlowId)
      : mDataFlowId(dataFlowId) {}

  uint32_t mDataFlowId = CHRE_DATA_FLOW_ID_INVALID;
};

}  // namespace chre

#include "chre/util/data_flow_source_impl.h"  // IWYU pragma: export