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

#ifndef CHRE_CORE_DATA_FLOW_MANAGER_H_
#define CHRE_CORE_DATA_FLOW_MANAGER_H_

#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED

#include "chre/core/nanoapp.h"
#include "chre/util/non_copyable.h"
#include "chre_api/chre/data_flow.h"
#include "chre_api/chre/msg.h"

namespace chre {

/** Manager class for data flow support in CHRE. */
class DataFlowManager : public NonCopyable {
 public:
  DataFlowManager() = default;
  ~DataFlowManager() = default;

  /** Initializes the DataFlowManager. */
  void init();

  /**
   * Creates a data flow with the given properties.
   *
   * @param nanoapp The nanoapp creating the data flow.
   * @param sinkDomains The sink domains for the data flow.
   * @param minAverageWriteIntervalNs The minimum average write interval.
   * @param maxAverageWriteBandwidthBytesPerSecond The maximum average write
   * bandwidth.
   * @param sinkPermissions The sink permissions.
   * @param elementSize The element size.
   * @param alignment The alignment.
   * @param minElementCount The minimum element count.
   * @param maxElementCount The maximum element count.
   * @param name The name of the data flow.
   *
   * @return one of chreStatus codes.
   */
  uint32_t createDataFlowAsync(Nanoapp *nanoapp, uint32_t sinkDomains,
                               uint64_t minAverageWriteIntervalNs,
                               uint32_t maxAverageWriteBandwidthBytesPerSecond,
                               uint32_t sinkPermissions, uint32_t elementSize,
                               uint32_t alignment, uint32_t minElementCount,
                               uint32_t maxElementCount, const char *name);

  /**
   * Destroys a data flow owned by the nanoapp.
   *
   * @param nanoapp The nanoapp destroying the data flow.
   * @param dataFlowId The ID of the data flow to destroy.
   *
   * @return one of chreStatus codes.
   */
  uint32_t destroyDataFlow(Nanoapp *nanoapp, uint32_t dataFlowId);

  /**
   * Adds a sink to a data flow.
   *
   * @param nanoapp The nanoapp adding the sink.
   * @param hubId The ID of the hub hosting the sink.
   * @param endpointId The ID of the endpoint hosting the sink.
   * @param dataFlowId The ID of the data flow.
   * @param sinkPolicy The policy for the sink.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sourceAddSinkAsync(Nanoapp *nanoapp, uint64_t hubId,
                              uint64_t endpointId, uint32_t dataFlowId,
                              const struct chreDataFlowSinkPolicy *sinkPolicy);

  /**
   * Adds a sink to a data flow over a session.
   *
   * @param nanoapp The nanoapp adding the sink.
   * @param hubId The ID of the hub hosting the sink.
   * @param endpointId The ID of the endpoint hosting the sink.
   * @param dataFlowId The ID of the data flow.
   * @param sinkPolicy The policy for the sink.
   * @param message The message to send.
   * @param messageSize The size of the message.
   * @param messageType The type of the message.
   * @param sessionId The session ID.
   * @param messagePermissions The permissions for the message.
   * @param freeCallback The callback to free the message.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sourceAddSinkOverSessionAsync(
      Nanoapp *nanoapp, uint64_t hubId, uint64_t endpointId,
      uint32_t dataFlowId, const struct chreDataFlowSinkPolicy *sinkPolicy,
      void *message, size_t messageSize, uint32_t messageType,
      uint16_t sessionId, uint32_t messagePermissions,
      chreMessageFreeFunction *freeCallback);

  /**
   * Configures an existing data flow sink.
   *
   * @param nanoapp The nanoapp configuring the sink.
   * @param hubId The ID of the hub hosting the sink.
   * @param endpointId The ID of the endpoint hosting the sink.
   * @param dataFlowId The ID of the data flow.
   * @param sinkPolicy The policy for the sink.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sourceConfigureSink(Nanoapp *nanoapp, uint64_t hubId,
                               uint64_t endpointId, uint32_t dataFlowId,
                               const struct chreDataFlowSinkPolicy *sinkPolicy);

  /**
   * Reserves space in a data flow source.
   *
   * @param nanoapp The nanoapp reserving the space.
   * @param dataFlowId The ID of the data flow.
   * @param numBytes The number of bytes to reserve.
   * @param data A pointer to the reserved data.
   * @param reservedBytes The actual number of bytes reserved.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sourceReserve(Nanoapp *nanoapp, uint32_t dataFlowId,
                         uint32_t numBytes, void **data,
                         uint32_t *reservedBytes);

  /**
   * Commits reserved space in a data flow source.
   *
   * @param nanoapp The nanoapp committing the space.
   * @param dataFlowId The ID of the data flow.
   * @param numBytes The number of bytes to commit.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sourceCommit(Nanoapp *nanoapp, uint32_t dataFlowId,
                        uint32_t numBytes);

  /**
   * Pushes data to a data flow source.
   *
   * @param nanoapp The nanoapp pushing the data.
   * @param dataFlowId The ID of the data flow.
   * @param data The data to push.
   * @param numBytes The number of bytes to push.
   * @param allOrNothing Whether to push all or nothing.
   * @param numberOfBytesPushed The number of bytes actually pushed.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sourcePush(Nanoapp *nanoapp, uint32_t dataFlowId, const void *data,
                      uint32_t numBytes, bool allOrNothing,
                      uint32_t *numberOfBytesPushed);

  /**
   * Gets the size of a data flow available for writing by the source.
   *
   * @param nanoapp The nanoapp getting the size.
   * @param dataFlowId The ID of the data flow.
   * @param includeReserved Whether to include reserved space.
   * @param size The size of the data flow.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sourceGetSize(Nanoapp *nanoapp, uint32_t dataFlowId,
                         bool includeReserved, uint32_t *size);

  /**
   * Gets the capacity of a data flow source.
   *
   * @param nanoapp The nanoapp getting the capacity.
   * @param dataFlowId The ID of the data flow.
   * @param capacity The capacity of the data flow.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sourceGetCapacity(Nanoapp *nanoapp, uint32_t dataFlowId,
                             uint32_t *capacity);

  /**
   * Enables a data flow sink previously created by a source.
   *
   * @param nanoapp The nanoapp enabling the sink.
   * @param hubId The ID of the hub hosting the sink.
   * @param dataFlowId The ID of the data flow.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sinkEnable(Nanoapp *nanoapp, uint64_t hubId, uint32_t dataFlowId);

  /**
   * Disables a data flow sink previously enabled by this nanoapp.
   *
   * @param nanoapp The nanoapp disabling the sink.
   * @param hubId The ID of the hub hosting the sink.
   * @param dataFlowId The ID of the data flow.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sinkDisable(Nanoapp *nanoapp, uint64_t hubId, uint32_t dataFlowId);

  /**
   * Gets the state of a data flow for a sink.
   *
   * @param nanoapp The nanoapp getting the state.
   * @param hubId The ID of the hub hosting the sink.
   * @param dataFlowId The ID of the data flow.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sinkGetState(Nanoapp *nanoapp, uint64_t hubId, uint32_t dataFlowId);

  /**
   * Peeks at data in a data flow sink.
   *
   * @param nanoapp The nanoapp peeking at the data.
   * @param hubId The ID of the hub hosting the sink.
   * @param dataFlowId The ID of the data flow.
   * @param numRequestedBytes The number of bytes requested.
   * @param data A pointer to the data.
   * @param numBytes The actual number of bytes peeked.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sinkPeek(Nanoapp *nanoapp, uint64_t hubId, uint32_t dataFlowId,
                    uint32_t numRequestedBytes, const void **data,
                    uint32_t *numBytes);

  /**
   * Releases data in a data flow.
   *
   * @param nanoapp The nanoapp releasing the data.
   * @param hubId The ID of the hub hosting the sink.
   * @param dataFlowId The ID of the data flow.
   * @param numBytes The number of bytes to release.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sinkRelease(Nanoapp *nanoapp, uint64_t hubId, uint32_t dataFlowId,
                       uint32_t numBytes);

  /**
   * Seeks to an offset in the data flow.
   *
   * @param nanoapp The nanoapp seeking.
   * @param hubId The ID of the hub hosting the sink.
   * @param dataFlowId The ID of the data flow.
   * @param offset The offset to seek to.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sinkSeek(Nanoapp *nanoapp, uint64_t hubId, uint32_t dataFlowId,
                    uint32_t offset);

  /**
   * Gets the offset in the data flow of the sink.
   *
   * @param nanoapp The nanoapp getting the offset.
   * @param hubId The ID of the hub hosting the sink.
   * @param dataFlowId The ID of the data flow.
   * @param offset The offset.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sinkGetOffset(Nanoapp *nanoapp, uint64_t hubId, uint32_t dataFlowId,
                         uint32_t *offset);
};

}  // namespace chre

#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED

#endif  // CHRE_CORE_DATA_FLOW_MANAGER_H_
