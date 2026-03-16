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
#include "chre/util/system/message_common.h"
#include "chre_api/chre/data_flow.h"
#include "chre_api/chre/msg.h"
#include "data_flow/queue.h"
#include "data_flow/untyped_queue.h"
#include "pw_containers/vector.h"
#include "pw_status/status.h"

#include <variant>

namespace chre {

// TODO(b/457453613): Handle nanoapp unload -> cleanup state

/**
 * Manager class for data flow support in CHRE. All public APIs must be
 * executed on a CHRE event loop and hold the global API lock.
 */
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
   * @param dataFlowId Pointer to a uint32_t that will contain the ID of the
   * data flow on success.
   *
   * @return one of chreStatus codes.
   */
  uint32_t createDataFlowAsync(Nanoapp *nanoapp, uint32_t sinkDomains,
                               uint64_t minAverageWriteIntervalNs,
                               uint32_t maxAverageWriteBandwidthBytesPerSecond,
                               uint32_t sinkPermissions, uint32_t elementSize,
                               uint32_t alignment, uint32_t minElementCount,
                               uint32_t maxElementCount, const char *name,
                               uint32_t *dataFlowId);

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

  /**
   * Handles the result of an async allocation of a data flow region.
   * @param cookie The cookie returned by
   * PlatformSharedDataRegionManager::allocateDataFlowRegionAsync()
   * @param status The status of the request, pw::OkStatus() on success
   * @param regionId The ID of the region that was allocated
   * @param region Details of the allocated region including the allocator
   * used to manage it
   * @param [opt] memoryAccess If present, an object used to access the region
   */
  void handleAllocateDataFlowRegionAsyncResult(
      uintptr_t cookie, pw::Status status, int32_t regionId,
      const android::contexthub::data_flow::AllocatorRegion &region,
      android::contexthub::data_flow::MemoryAccess *memoryAccess);

  /**
   * Handles the registration of a nanoapp as the sink for a data flow.
   *
   * @param registration The registration to handle.
   */
  void onRegisterDataFlowSink(
      chre::message::DataFlowSinkRegistration &&registration);

  /**
   * Handles the unregistration of a nanoapp as the sink for a data flow.
   *
   * @param unregistration The unregistration to handle.
   */
  void onDataFlowSinkUnregistered(
      const chre::message::DataFlowSinkUnregistration &unregistration);

  /**
   * Handles a data flow stopped event where the nanoapp is a sink.
   *
   * @param stopped The stopped event to handle.
   */
  void onDataFlowStopped(const chre::message::DataFlowStopped &stopped);

  /**
   * Handles a data flow alert event where the nanoapp is a sink.
   *
   * @param alert The alert event to handle.
   */
  void onDataFlowAlert(const chre::message::DataFlowAlert &alert);

 private:
  //! The configuration for the block size and count for a data flow.
  struct BlockConfig {
    size_t blockCapacity;
    size_t minBlockCount;
    size_t maxBlockCount;
  };

  //! The properties of a data flow provided during creation.
  struct DataFlowProperties {
    const char *name;
    uint32_t dataFlowId;
    uint32_t sinkDomains;
    uint32_t sinkPermissions;
    uint32_t dataFlowSize;
    uint32_t elementSize;
    uint32_t alignment;
    uint32_t minElementCount;
    uint32_t maxElementCount;
    BlockConfig blockConfig;
  };

  //! A data flow owned by a nanoapp.
  struct NanoappDataFlow {
    //! The properties of the data flow.
    DataFlowProperties properties;

    //! The instance ID of the nanoapp that owns this data flow.
    uint16_t nanoappInstanceId;

    //! The ID of the data flow region.
    int32_t regionId;

    //! The cookie for the async allocation of the data flow region. If
    //! std::nullopt, then this data flow is active.
    std::optional<uintptr_t> cookie;

    //! The region that is allocated for this data flow.
    android::contexthub::data_flow::AllocatorRegion allocatorRegion;

    //! The memory access object for the data flow region. May be nullptr even
    //! if the data flow is active.
    android::contexthub::data_flow::MemoryAccess *memoryAccess;

    //! The producer instance for this data flow.
    std::variant<std::monostate,
                 android::contexthub::data_flow::UntypedProducer,
                 android::contexthub::data_flow::VariableDataProducer>
        producer;
  };

  //! The invalid region ID value.
  static constexpr int32_t kInvalidRegionId = -1;

  //! The maximum number of data flows that can be active or pending.
  static constexpr uint32_t kMaxDataFlows = 10;

  //! Calculates the block configuration for a data flow.
  //! @param minElementCount The minimum element count of the data flow.
  //! @param maxElementCount The maximum element count of the data flow.
  //! @return The block configuration for the data flow.
  static BlockConfig calculateBlockConfig(uint32_t minElementCount,
                                          uint32_t maxElementCount);

  //! The callback for remote notifications on a data flow. This is used to
  //! propagate an alert from a nanoapp source.
  // TODO(b/457453613): Make this static to ensure it accesses no state or
  // defer.
  void sendDataFlowAlertToRemoteSink(uint32_t dataFlowId, uint64_t sinkHubId,
                                     uint64_t sinkEndpointId);

  //! Creates the producer for the given data flow.
  //! @param dataFlow The active data flow for which to create a producer.
  //! @return pw::OkStatus() on success.
  pw::Status createProducer(NanoappDataFlow &dataFlow);

  //! The data flows owned by nanoapps.
  pw::Vector<NanoappDataFlow, kMaxDataFlows> mDataFlows;

  //! The next available data flow ID. CHRE_DATA_FLOW_ID_INVALID is 0.
  uint32_t mNextDataFlowId = 1;

  //! The data notifier used for nanoapp producers.
  android::contexthub::data_flow::DataNotifier mDataNotifier;
};

}  // namespace chre

#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED

#endif  // CHRE_CORE_DATA_FLOW_MANAGER_H_
