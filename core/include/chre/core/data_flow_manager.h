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

#include "chre/core/event_loop.h"
#include "chre/core/nanoapp.h"
#include "chre/core/shared_data_region_manager.h"
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
   * Truncates reserved space in a variable-size element data flow source.
   *
   * @param nanoapp The nanoapp truncating the space.
   * @param dataFlowId The ID of the data flow.
   * @param size The size to truncate to.
   *
   * @return one of chreStatus codes.
   */
  uint32_t variableSourceTruncate(Nanoapp *nanoapp, uint32_t dataFlowId,
                                  uint32_t size);

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
   * Pops data from a data flow sink.
   *
   * @param nanoapp The nanoapp popping the data.
   * @param hubId The ID of the hub hosting the sink.
   * @param dataFlowId The ID of the data flow.
   * @param data A pointer to the data.
   * @param[in,out] numBytes The number of bytes to pop.
   *
   * @return one of chreStatus codes.
   */
  uint32_t sinkPop(Nanoapp *nanoapp, uint64_t hubId, uint32_t dataFlowId,
                   void *data, uint32_t *numBytes);

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
   * Gets the size of the head element in a variable data flow sink.
   *
   * @param nanoapp The nanoapp getting the size.
   * @param hubId The ID of the hub hosting the sink.
   * @param dataFlowId The ID of the data flow.
   * @param size The size of the head element.
   *
   * @return one of chreStatus codes.
   */
  uint32_t variableSinkGetHeadSize(Nanoapp *nanoapp, uint64_t hubId,
                                   uint32_t dataFlowId, uint32_t *size);

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
  void onRegisterDataFlowSink(message::DataFlowSinkRegistration &&registration);

  /**
   * Handles the unregistration of a nanoapp as the sink for a data flow.
   *
   * @param unregistration The unregistration to handle.
   */
  void onDataFlowSinkUnregistered(
      const message::DataFlowSinkUnregistration &unregistration);

  /**
   * Handles a data flow stopped event where the nanoapp is a sink.
   *
   * @param stopped The stopped event to handle.
   */
  void onDataFlowStopped(const message::DataFlowStopped &stopped);

  /**
   * Handles a data flow alert event where the nanoapp is a sink.
   *
   * @param alert The alert event to handle.
   */
  void onDataFlowAlert(const message::DataFlowAlert &alert);

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

  //! A data flow sink registered by a nanoapp.
  struct NanoappDataFlowSink {
    //! The hub ID of the source.
    uint64_t sourceHubId;

    //! The endpoint ID of the source.
    uint64_t sourceEndpointId;

    //! The ID of the data flow.
    uint32_t dataFlowId;

    //! Offset of the metadata in the primary region.
    uint32_t metadataOffset;

    //! Offset of the sink metadata.
    uint32_t sinkMetadataOffset;

    //! The size of each element in the data flow.
    uint32_t elementSize;

    //! The alignment of each element in the data flow.
    uint32_t alignment;

    //! The instance ID of the nanoapp that is the sink.
    uint16_t nanoappInstanceId;

    //! Whether the sink is active.
    bool isActive;

    //! Guard for the primary region of the data flow.
    SharedDataRegionManager::RegionGuard primaryRegionGuard;

    //! Guard for the sink metadata region of the data flow, may be invalid.
    SharedDataRegionManager::RegionGuard sinkMetadataRegionGuard;

    //! The consumer instance for this data flow sink.
    std::variant<std::monostate,
                 android::contexthub::data_flow::UntypedConsumer,
                 android::contexthub::data_flow::VariableDataConsumer>
        consumer;
  };

  //! A struct containing data flow sink registration info and a session
  //! message.
  struct NanoappSinkRegistrationWithMessage {
    //! The info for the data flow sink registration.
    chreDataFlowSinkInfo info;

    //! The session message for the data flow sink registration.
    chreMsgMessageFromEndpointData sessionMessage;

    //! The message data.
    pw::UniquePtr<std::byte[]> messageData;

    //! The instance ID of the nanoapp that is the sink.
    uint16_t nanoappInstanceId;
  };

  //! The invalid region ID value.
  static constexpr int32_t kInvalidRegionId = -1;

  //! The maximum number of data flows that can be active or pending.
  static constexpr uint32_t kMaxDataFlows = 10;

  //! Builds a ConsumerPolicyBuilder from a chreDataFlowSinkPolicy.
  //! @param sinkPolicy The sink policy to build from.
  //! @param policyBuilderOut Pointer to the ConsumerPolicyBuilder to be
  //! populated.
  //! @return CHRE_STATUS_OK if successful, otherwise an error status.
  static uint32_t buildConsumerPolicy(
      const struct chreDataFlowSinkPolicy *sinkPolicy,
      android::contexthub::data_flow::ConsumerPolicyBuilder *policyBuilderOut);

  //! Calculates the block configuration for a data flow.
  //! @param minElementCount The minimum element count of the data flow.
  //! @param maxElementCount The maximum element count of the data flow.
  //! @return The block configuration for the data flow.
  static BlockConfig calculateBlockConfig(uint32_t minElementCount,
                                          uint32_t maxElementCount);

  //! Helper function to get a nanoapp from a hub ID and endpoint ID.
  //!
  //! @param hubId The hub ID.
  //! @param endpointId The endpoint ID.
  //! @param eventLoopOut Pointer to a pointer to the event loop to be
  //! populated if it is not nullptr. Will be set to nullptr if this function
  //! returns nullptr.
  //! @return The nanoapp if the hub ID is the CHRE hub ID and the nanoapp
  //! exists, otherwise nullptr.
  static Nanoapp *getNanoapp(uint64_t hubId, uint64_t endpointId,
                             EventLoop **eventLoopOut = nullptr);

  //! Sends a data flow alert to a remote source.
  //!
  //! @param dataFlowId The ID of the data flow.
  //! @param sourceHubId The hub ID of the source.
  //! @param sourceEndpointId The endpoint ID of the source.
  static void sendDataFlowAlertToRemoteSource(uint32_t dataFlowId,
                                              uint64_t sourceHubId,
                                              uint64_t sourceEndpointId);

  //! Sends a data flow alert to a remote sink.
  //!
  //! @param dataFlowId The ID of the data flow.
  //! @param sinkHubId The hub ID of the sink.
  //! @param sinkEndpointId The endpoint ID of the sink.
  static void sendDataFlowAlertToRemoteSink(uint32_t dataFlowId,
                                            uint64_t sinkHubId,
                                            uint64_t sinkEndpointId);

  //! Creates the producer for the given data flow.
  //! @param dataFlow The active data flow for which to create a producer.
  //! @return pw::OkStatus() on success.
  pw::Status createProducer(NanoappDataFlow &dataFlow);

  //! Creates the consumer for the given sink registration.
  //! @param registration The sink registration to create a consumer for.
  //! @param nanoapp The nanoapp that is the sink.
  //! @param primaryRegionGuard The region guard for the primary region.
  //! @param sinkMetadataRegionGuard The region guard for the sink metadata
  //! region.
  //! @param elementSize The size of each element in the data flow.
  //! @param alignment The alignment of each element in the data flow.
  //! @return The consumer for the given sink registration.
  std::variant<std::monostate, android::contexthub::data_flow::UntypedConsumer,
               android::contexthub::data_flow::VariableDataConsumer>
  createConsumer(const chre::message::DataFlowSinkRegistration &registration,
                 const Nanoapp *nanoapp,
                 SharedDataRegionManager::RegionGuard &primaryRegionGuard,
                 SharedDataRegionManager::RegionGuard &sinkMetadataRegionGuard,
                 uint32_t &elementSize, uint32_t &alignment);

  //! Helper function to find a data flow and check if the given nanoapp owns
  //! it.
  //! @param dataFlowId The ID of the data flow.
  //! @param nanoappInstanceId The instance ID of the nanoapp to check.
  //! @param dataFlowOut Pointer to a pointer to the NanoappDataFlow to be
  //! populated.
  //! @return CHRE_STATUS_OK if successful, otherwise an error status.
  uint32_t getNanoappDataFlow(uint32_t dataFlowId, uint16_t nanoappInstanceId,
                              NanoappDataFlow **dataFlowOut);

  //! Helper function to validate sink parameters, get the data flow, and build
  //! the consumer policy and remote endpoint ID.
  //! @return CHRE_STATUS_OK if successful, otherwise an error status.
  uint32_t validateAndGetSinkRequest(
      Nanoapp *nanoapp, uint64_t hubId, uint64_t endpointId,
      uint32_t dataFlowId, const struct chreDataFlowSinkPolicy *sinkPolicy,
      NanoappDataFlow **dataFlowOut,
      android::contexthub::data_flow::ConsumerPolicyBuilder *policyBuilderOut);

  //! Helper function shared by sourceAddSinkAsync and
  //! sourceAddSinkOverSessionAsync. If hasMessage is true, then the message
  //! parameters are used to create a session message to bundle with the
  //! registration.
  uint32_t sourceAddSinkAsyncCommon(
      Nanoapp *nanoapp, uint64_t hubId, uint64_t endpointId,
      uint32_t dataFlowId, const struct chreDataFlowSinkPolicy *sinkPolicy,
      bool hasMessage, void *message, size_t messageSize, uint32_t messageType,
      uint16_t sessionId, uint32_t messagePermissions,
      chreMessageFreeFunction *freeCallback);

  //! Helper to lookup the NanoappDataFlow for a given data flow ID.
  NanoappDataFlow *findNanoappDataFlow(message::DataFlowId dataFlowId);

  //! The data flows owned by nanoapps.
  pw::Vector<NanoappDataFlow, kMaxDataFlows> mDataFlows;

  //! The sinks registered for data flows.
  pw::Vector<NanoappDataFlowSink, kMaxDataFlows> mSinks;

  //! The next available data flow ID. CHRE_DATA_FLOW_ID_INVALID is 0.
  uint32_t mNextDataFlowId = 1;

  //! The data notifier used for nanoapp producers.
  android::contexthub::data_flow::DataNotifier mDataNotifier;
};

}  // namespace chre

#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED

#endif  // CHRE_CORE_DATA_FLOW_MANAGER_H_
