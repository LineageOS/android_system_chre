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

// IWYU pragma: private, include "chre_api/chre.h"
// IWYU pragma: friend chre/.*\.h

#ifndef _CHRE_DATA_FLOW_H_
#define _CHRE_DATA_FLOW_H_

/**
 * @file
 * This file defines the API for CHRE data flows, which are designed for
 * efficient high-throughput data transmission between a single source
 * and multiple sinks, which may include nanoapps and other endpoints. These
 * data flows enable the transfer of large amounts of data with minimal data
 * copies, leveraging shared memory regions. Data flows are uinquely identified
 * by the message hub ID of the source and the data flow ID. They provide a
 * mechanism for nanoapps to exchange data streams, supporting various new data
 * alert and overwrite policies to suit different batching use cases.
 *
 * Here is an example of a source nanoapp that creates a data flow and adds a
 * sink nanoapp:
 *
 * - Source nanoapp:
 *  - Creates a data flow and receives the CHRE_EVENT_DATA_FLOW_CREATED event
 *
 * - Sink nanoapp:
 *  - Uses endpoint messaging to request a sink be created on the data flow for
 *    the sink nanoapp.
 *
 * - Source nanoapp:
 *  - Responds to the request and creates a sink
 *  - The sink handle is sent to the sink nanoapp by the platform.
 *  - Receives the CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE event, indicating
 *    the sink was successfully created and notified
 *
 * - Sink nanoapp:
 *  - Receives the CHRE_EVENT_DATA_FLOW_SINK_CREATED event
 *  - Calls chreDataFlowSinkEnable() to enable the sink
 *
 * - Source nanoapp:
 *  - Pushes data into the data flow
 *
 * - Sink nanoapp:
 *  - Receives the CHRE_EVENT_DATA_FLOW_ALERT event, indicating data is
 *    available (only if never notify was NOT selected) or an underlying change
 *    in the underlying data flow
 *  - Alternatively may at any time use the sink API to poll the data flow
 *  - Processes the data and releases the elements
 *
 * - Either nanoapp:
 *  - Can disable the sink
 *  - Both nanoaps receive a CHRE_EVENT_DATA_FLOW_SINK_STOPPED event, indicating
 *    a state change in the underlying data flow (sink is gone)
 *
 * - Source nanoapp:
 *  - Destroys the data flow explicitly or on unload
 *
 * - Source nanoapp:
 *  - If it crashes, the sink nanoapp will receive a
 *    CHRE_EVENT_DATA_FLOW_STOPPED event.
 *
 * @since v1.12
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <chre/common.h>
#include <chre/event.h>
#include <chre/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The new data alert policy for a data flow sink.
 *
 * Backing type: uint32_t.
 */
enum chreDataFlowSinkNewDataAlertPolicy {
  /** The sink will never be alerted. */
  CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_NEVER = 0x0,

  /**
   * The sink will be alerted opportunistically, e.g. when the system or
   * source deems that the sink can be alerted with minimal power impact.
   */
  CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_OPPORTUNISTIC = 0x1,

  /**
   * The sink will be alerted when the data flow has reached the high water
   * mark. The high water mark is provided when configuring the data flow
   * sink.
   */
  CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_HIGH_WATER_MARK = 0x2,

  /**
   * The sink will be alerted periodically, with the period specified by
   * the period parameter when configuring the data flow sink. The platform
   * will deliver these new data alerts as soon as possible, but there are no
   * guarantees on when exactly they will be delivered. If no data is
   * available when the new data alert is due, then no new data alert will be
   * sent.
   */
  CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_PERIODIC = 0x3,

  /**
   * The sink will be alerted when data is available in the data flow,
   * on every write into the data flow. The platform may coalesce these
   * new data alerts or throttle new data alerts to optimize performance or
   * power consumption.
   */
  CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_STREAMING = 0x4,
};

/**
 * The overwrite policy for a data flow sink. This determines whether the
 * source can overwrite data that is being currently read by the sink.
 *
 * Backing type: uint32_t.
 */
enum chreDataFlowSinkOverwritePolicy {
  /** The sink can be overwritten. */
  CHRE_DATA_FLOW_SINK_OVERWRITE_POLICY_ALLOWED = 0x0,

  /** The sink can never be overwritten. */
  CHRE_DATA_FLOW_SINK_OVERWRITE_POLICY_DISALLOWED = 0x1,
};

/** The data flow sink policy for a data flow sink. */
struct chreDataFlowSinkPolicy {
  /**
   * The new data alert policy for the sink - one of
   * chreDataFlowSinkNewDataAlertPolicy.
   */
  uint32_t newDataAlertPolicy;

  /**
   * The data associated with the new data alert policy. See the table:
   *  - CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_OPPORTUNISTIC
   *    - newDataAlertPolicyData = low watermark value
   *  - CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_HIGH_WATER_MARK
   *    - newDataAlertPolicyData = high watermark value
   *  - CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_PERIODIC
   *    - newDataAlertPolicyData = period in milliseconds
   *  - For all other values, this parameter is set to 0 and ignored.
   */
  union {
    uint32_t lowWatermark;
    uint32_t highWatermark;
    uint32_t periodMs;
    uint32_t reserved;
  } newDataAlertPolicyData;

  /**
   * The overwrite policy for the sink - one of
   * chreDataFlowSinkOverwritePolicy.
   */
  uint32_t overwritePolicy;
};

/**
 * Data provided in the CHRE_EVENT_DATA_FLOW_CREATED event.
 */
struct chreDataFlowCreatedInfo {
  /** The status of the data flow creation request, one of chreStatus. */
  uint32_t status;

  /** The data flow ID of the data flow that was created. */
  uint32_t dataFlowId;

  /** The total size of the data flow in bytes. */
  uint32_t size;

  /**
   * The supportable domains of the sinks of this data flow, a bitmask of
   * CHRE_DATA_FLOW_SINK_DOMAIN_* values.
   */
  uint32_t sinkDomains;

  /**
   * Bitmask of permissions that must be held to receive data from the data
   * flow, and will be attributed to the recipient. Primarily relevant when the
   * destination endpoint is an Android application. Refer to
   * CHRE_MESSAGE_PERMISSION_* values.
   */
  uint32_t permissions;
};

/**
 * Data provided in the CHRE_EVENT_DATA_FLOW_STOPPED event.
 */
struct chreDataFlowStoppedInfo {
  /** The message hub ID of the source. */
  uint64_t hubId;

  /** The data flow ID of the data flow that was stopped. */
  uint32_t dataFlowId;
};

/**
 * Data provided in the CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE event.
 */
struct chreDataFlowSinkConfigureInfo {
  /** The status of the data flow sink creation request, one of chreStatus. */
  uint32_t status;

  /**
   * The data flow ID of the data flow that was created. This is scoped to the
   * message hub ID of this source.
   */
  uint32_t dataFlowId;

  /** The message hub ID of the sink. */
  uint64_t hubId;

  /** The endpoint ID of the sink. */
  uint64_t endpointId;
};

/**
 * Data provided in the CHRE_EVENT_DATA_FLOW_SINK_CREATED event.
 */
struct chreDataFlowSinkInfo {
  /** The message hub ID of the source that created this sink. */
  uint64_t hubId;

  /** The endpoint ID of the source that created this sink. */
  uint64_t endpointId;

  /**
   * The data flow ID where the nanoapp is now a sink. Scoped to the hub
   * ID of the source.
   */
  uint32_t dataFlowId;

  /** The size of each element in bytes. */
  uint32_t elementSize;

  /**
   * The minimum alignment of each element in bytes. If
   * CHRE_DATA_FLOW_ELEMENT_ALIGNMENT_UNALIGNED, the data flow will have no
   * alignment requirements.
   */
  uint32_t alignment;

  /**
   * If this data flow sink was created with a message, this will be non-NULL
   * and contain the message, else it will be NULL. If this is received from a
   * CHRE_EVENT_DATA_FLOW_SINK_STOPPED event, this will be NULL.
   */
  struct chreMsgMessageFromEndpointData *messageFromEndpointData;
};

/**
 * Data provided in the CHRE_EVENT_DATA_FLOW_ALERT event.
 */
struct chreDataFlowNewDataAlert {
  /** The message hub ID associated with this data flow. */
  uint64_t hubId;

  /** The data flow ID associated with this new data alert. */
  uint32_t dataFlowId;
};

/**
 * Data flow sink domains, a bitmask of uint32_t.
 *
 * @defgroup CHRE_DATA_FLOW sink domains
 * @{
 */

#define CHRE_DATA_FLOW_SINK_DOMAIN_INVALID        (UINT32_C(0))
#define CHRE_DATA_FLOW_SINK_DOMAIN_LOCAL_NANOAPP  (UINT32_C(1) << 0)
#define CHRE_DATA_FLOW_SINK_DOMAIN_HOST_AVAILABLE (UINT32_C(1) << 1)
#define CHRE_DATA_FLOW_SINK_DOMAIN_VENDOR_START   (UINT32_C(1) << 21)
#define CHRE_DATA_FLOW_SINK_DOMAIN_VENDOR_END     (UINT32_C(1) << 31)

/** @} */

/**
 * Special value for the minimum average write interval parameter (provided when
 * creating a data flow) that hints to the platform that the use case for this
 * data flow requires lower power memory and lower latency, because it writes
 * data frequently.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_FLOW_MIN_AVERAGE_WRITE_INTERVAL_LOW UINT64_C(0)

/**
 * Special value for the minimum average write interval parameter (provided when
 * creating a data flow) that hints to the platform that the use case for this
 * data flow can tolerate higher power memory and higher latency, because it
 * writes data infrequently.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_FLOW_MIN_AVERAGE_WRITE_INTERVAL_HIGH UINT64_MAX

/**
 * Special value for the maximum average write bandwidth parameter (provided
 * when creating a data flow) that hints to the platform that this data flow
 * does not require the fastest or most performant hardware configuration,
 * because it writes a low volume of data.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_FLOW_MAX_AVERAGE_WRITE_BANDWIDTH_LOW UINT32_C(0)

/**
 * Special value for the maximum average write bandwidth parameter (provided
 * when creating a data flow) that hints to the platform that this data flow
 * should use the fastest or most performant hardware configuration, because it
 * writes a high volume of data.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_FLOW_MAX_AVERAGE_WRITE_BANDWIDTH_HIGH UINT32_MAX

/**
 * Invalid and reserved data flow and sink IDs.
 *
 * @defgroup CHRE_DATA_FLOW IDs
 * @{
 */

#define CHRE_DATA_FLOW_ID_INVALID  UINT32_C(0)
#define CHRE_DATA_FLOW_ID_RESERVED UINT32_C(-1)

#define CHRE_DATA_FLOW_SINK_ID_INVALID  UINT32_C(0)
#define CHRE_DATA_FLOW_SINK_ID_RESERVED UINT32_C(-1)

/** @} */

/**
 * Used to signal a data flow has variable size elements.
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_FLOW_ELEMENT_SIZE_VARIABLE UINT32_C(0)

/**
 * Used to signal that the elements within a data flow have no alignment
 * requirements.
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_FLOW_ELEMENT_ALIGNMENT_UNALIGNED UINT32_C(0)

/**
 * Used to signal a data flow can grow dynamically, with CHRE setting the size.
 */
#define CHRE_DATA_FLOW_DYNAMIC_MAX_SIZE UINT32_C(-1)

/**
 * Produce an event ID in the block of IDs reserved for data flow events.
 *
 * Valid input range is [0, 15]. Do not add new events with ID > 15
 * (see chre/event.h)
 *
 * @param offset Index into DATA_FLOW event ID block; valid range is [0, 15].
 *
 * @defgroup CHRE_DATA_FLOW_EVENT_ID
 * @{
 */
#define CHRE_DATA_FLOW_EVENT_ID(offset) \
    (CHRE_EVENT_DATA_FLOW_FIRST_EVENT + (offset))

/**
 * nanoappHandleEvent argument: struct chreDataFlowCreatedInfo.
 *
 * Event sent when a data flow is created.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_EVENT_DATA_FLOW_CREATED CHRE_DATA_FLOW_EVENT_ID(0)

/**
 * nanoappHandleEvent argument: struct chreDataFlowStoppedInfo.
 *
 * Event sent when a data flow is destroyed.
 */
#define CHRE_EVENT_DATA_FLOW_STOPPED CHRE_DATA_FLOW_EVENT_ID(1)

/**
 * nanoappHandleEvent argument: struct chreDataFlowSinkConfigureInfo.
 *
 * Event sent when a data flow sink configuration is complete.
 *
 * @see chreDataFlowSourceAddSinkAsync
 */
#define CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE CHRE_DATA_FLOW_EVENT_ID(2)

/**
 * nanoappHandleEvent argument: struct chreDataFlowSinkInfo.
 *
 * Event sent to a sink nanoapp when a data flow sink is created for the
 * nanoapp. If the nanoapp wants to be a sink, it must call
 * chreDataFlowSinkEnable() to activate the sink. If chreDataFlowSinkEnable()
 * is not called, the sink will be disabled.
 *
 * @see chreDataFlowSinkEnable
 */
#define CHRE_EVENT_DATA_FLOW_SINK_CREATED CHRE_DATA_FLOW_EVENT_ID(3)

/**
 * nanoappHandleEvent argument: struct chreDataFlowSinkInfo.
 *
 * Event sent to a source nanoapp when a data flow sink is stopped.
 *
 * @see chreDataFlowSinkDisable
 */
#define CHRE_EVENT_DATA_FLOW_SINK_STOPPED CHRE_DATA_FLOW_EVENT_ID(4)

/**
 * nanoappHandleEvent argument: struct chreDataFlowNewDataAlert.
 *
 * Event sent when data is available in the data flow for consumption.
 */
#define CHRE_EVENT_DATA_FLOW_ALERT CHRE_DATA_FLOW_EVENT_ID(5)

// NOTE: Do not add new events with ID > 15
/** @} */

/**
 * Creates a data flow with the given properties. This data flow will be used
 * by the nanoapp as a source. An elementSize of 0 indicates variable size
 * elements with minElementCount and maxElementCount both representing bytes.
 * This function will return CHRE_STATUS_OK and the nanoapp will receive a
 * CHRE_EVENT_DATA_FLOW_CREATED event when the data flow is created or a failure
 * status if the data flow cannot be created.
 *
 * The data flow may be instantiated in a new or existing region, guaranteed
 * to have the provided domain and permission properties. The parameters
 * sinkDomains, minAverageWriteIntervalNs, and
 * maxAverageWriteBandwidthBytesPerSecond are used to guide the platform on
 * memory bank selection. Once a data flow is created, its memory region cannot
 * be changed without destroying and re-creating the flow, so nanoapps should
 * supply values that represent the desired performance and power attributes for
 * the lifetime of the flow.
 *
 * @param sinkDomains A bitmask of CHRE_DATA_FLOW_SINK_DOMAIN_* values,
 *     indicating the sink domains that this data flow must be able to support.
 * @param minAverageWriteIntervalNs The expected minimum average (sustained)
 *     interval between successive writes to the data flow, in nanoseconds.
 *     This guides the platform towards selecting a memory region with suitable
 *     power attributes.
 * @param maxAverageWriteBandwidthBytesPerSecond The expected maximum average
 *     (sustained) write bandwidth for the data flow, in bytes per second. This
 *     guides the platform towards selecting and/or configuring a memory region
 *     with suitable performance attributes.
 * @param sinkPermissions Bitmask of permissions that must be held to receive
 *     data from the data flow, and will be attributed to the recipient.
 *     Primarily relevant when the destination endpoint is an Android
 *     application. Refer to CHRE_MESSAGE_PERMISSION_* values. Both the source
 *     and sink nanoapps must have these permissions.
 * @param elementSize The size of each element in bytes. If
 *     CHRE_DATA_FLOW_ELEMENT_SIZE_VARIABLE, the data flow will have variable
 *     size elements.
 * @param alignment The alignment of each element in bytes. If
 *     CHRE_DATA_FLOW_ELEMENT_ALIGNMENT_UNALIGNED, the data flow will have no
 *     alignment requirements and elements will be packed, with no padding
 *     inserted to ensure alignment. In this case, special care must be taken on
 *     both the source and all sinks to ensure that safe methods for unaligned
 *     access are used, according to the requirements of the local processor.
 * @param minElementCount The minimum number of elements to allocate for the
 *     data flow. If this amount cannot be allocated, this function will return
 *     CHRE_STATUS_RESOURCE_EXHAUSTED.
 * @param maxElementCount The maximum number of elements to allocate for the
 *     data flow. Must be greater than or equal to minElementCount. The data
 *     flow will dynamically grow up to this size as needed. If this is
 *     CHRE_DATA_FLOW_DYNAMIC_MAX_SIZE, then the data flow will be created with
 *     a maximum size determined by CHRE. Note that the data flow may not be
 *     able to grow to this max size if other data flows in the same region grow
 *     to their maximum sizes and the region cannot accommodate more elements.
 * @param name A human-readable name for the data flow. This is used for
 *     debugging purposes and will not be shared with endpoints. This must not
 *     be NULL.
 * @return One of chreStatus
 *  - CHRE_STATUS_OK if the data flow was successfully created.
 *  - CHRE_STATUS_INVALID_ARGUMENT if any of the arguments are invalid or if
 *    the name is NULL.
 *  - CHRE_STATUS_RESOURCE_EXHAUSTED if the data flow cannot be created due to
 *    insufficient memory.
 *  - CHRE_STATUS_FAILED_PRECONDITION if the requested domains cannot be
 *    supported by the platform.
 *  - CHRE_STATUS_PERMISSION_DENIED if this source nanoapp does not have the
 *    sinkPermissions.
 *
 * @since v1.12
 */
uint32_t chreDataFlowCreateAsync(uint32_t sinkDomains,
    uint64_t minAverageWriteIntervalNs,
    uint32_t maxAverageWriteBandwidthBytesPerSecond,
    uint32_t sinkPermissions, uint32_t elementSize, uint32_t alignment,
    uint32_t minElementCount, uint32_t maxElementCount,
    const char *name);

/**
 * Destroys a data flow. This data flow must be owned by this nanoapp. If the
 * data flow is not owned by this nanoapp, this function will return an error
 * status and the data flow will not be destroyed. All nanoapp sinks of this
 * data flow will be notified with a CHRE_EVENT_DATA_FLOW_STOPPED event, and all
 * non-nanoapp sinks will be notified as well.
 *
 * It is safe to destroy a data flow while one or more sinks are enabled.
 *
 * @param dataFlowId The ID of the data flow to destroy.
 * @return One of chreStatus:
 *  - CHRE_STATUS_OK if the data flow was successfully destroyed.
 *  - CHRE_STATUS_PERMISSION_DENIED if the data flow is not owned by this
 *    nanoapp.
 *
 * @since v1.12
 */
uint32_t chreDataFlowDestroy(uint32_t dataFlowId);

/**
 * Creates a sink on the data flow owned by the nanoapp. This function returns
 * CHRE_STATUS_OK if the request to create the sink was successfully queued for
 * processing by the platform. This source nanoapp will receive the
 * CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE event with a status indicating
 * whether the sink was successfully created and notified or an error status
 * otherwise. On sink creation, the sink handle is sent to the specified
 * endpoint. A nanoapp sink will receive the CHRE_EVENT_DATA_FLOW_SINK_CREATED
 * event.
 *
 * @param hubId The sink's message hub ID.
 * @param endpointId The sink's endpoint ID.
 * @param dataFlowId The ID of the data flow on which to create the sink.
 * @param sinkPolicy The sink policy for the sink. Must be non-NULL.
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if this nanoapp will receive the
 *    CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE event with a status indicating
 *    the request was successful.
 *  - CHRE_STATUS_INVALID_ARGUMENT if sinkPolicy is NULL.
 *  - CHRE_STATUS_FAILED_PRECONDITION if the sink cannot be added to the
 *    data flow because it cannot access the domain in which the data flow was
 *    created.
 *  - CHRE_STATUS_PERMISSION_DENIED if the source does not own the data
 *    flow or if the sink does not have permission to access the domain of the
 *    data flow.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSourceAddSinkAsync(uint64_t hubId,
    uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy);

/**
 * Creates a sink on a data flow owned by the calling nanoapp, delivering the
 * metadata for the new sink alongside message payload.
 *
 * The intended use case of this API is for scenarios where an endpoint sends
 * a message requesting a data flow, and the source would like to deliver the
 * data flow sink handle with custom response message (what would be sent
 * via chreMsgSend()), for example containing metadata related to the current
 * data flow contents.
 *
 * See chreDataFlowSourceAddSinkAsync() for more details.
 *
 * @param hubId The sink's message hub ID.
 * @param endpointId The sink's endpoint ID.
 * @param dataFlowId The ID of the data flow on which to create the sink.
 * @param sinkPolicy The sink policy for the sink. Must be non-NULL.
 * @param message Pointer to a block of memory to send to the other endpoint in
 *     this session. NULL is acceptable only if messageSize is 0. This function
 *     transfers ownership of the provided memory to the system, so the data
 *     must stay valid and unmodified until freeCallback is invoked.
 * @param messageSize The size, in bytes, of the given message. Maximum allowed
 *     size for the destination endpoint is provided in chreMsgEndpointInfo.
 * @param messageType An opaque value passed along with the message payload,
 *     using an application/service-defined scheme.
 * @param sessionId The session over which to send this message, which also
 *     implicitly identifies the destination service (if used), endpoint, and
 *     hub. Provided in chreMsgSessionInfo.
 * @param messagePermissions Bitmask of permissions that must be held to receive
 *     this message, and will be attributed to the recipient. Primarily relevant
 *     when the destination endpoint is an Android application. Refer to
 *     CHRE_MESSAGE_PERMISSION_* values.
 * @param freeCallback Invoked when the system no longer needs the memory
 *     holding the message. Note that this does not necessarily mean that the
 *     message has been delivered. If message is non-NULL, this must be
 *     non-NULL, and if message is NULL, this must be NULL.
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if the request was successfully queued for processing. The
 *    nanoapp will receive a CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE event with
 *    a status indicating whether the sink was successfully created and notified
 *    or an error status otherwise.
 *  - CHRE_STATUS_ALREADY_EXISTS if a sink already exists on the data flow.
 *  - CHRE_STATUS_INVALID_ARGUMENT if sinkPolicy is NULL or if the constraints
 *    specified in chreMsgSend() are not met for message, messageSize, and
 *    messageType.
 *  - CHRE_STATUS_FAILED_PRECONDITION if the sink cannot be added to the
 *    data flow because it cannot access the domain in which the data flow was
 *    created.
 *  - CHRE_STATUS_PERMISSION_DENIED if the source does not own the data
 *    flow.
 *
 * @see chreDataFlowSourceAddSinkAsync
 * @see chreMsgSend
 *
 * @since v1.12
 */
uint32_t chreDataFlowSourceAddSinkOverSessionAsync(uint64_t hubId,
    uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy, void *message,
    size_t messageSize, uint32_t messageType, uint16_t sessionId,
    uint32_t messagePermissions, chreMessageFreeFunction *freeCallback);

/**
 * Synchronously configures an existing sink on a data flow owned by the
 * calling nanoapp.
 *
 * @param hubId The sink's message hub ID.
 * @param endpointId The sink's endpoint ID.
 * @param dataFlowId The ID of the data flow on which to create the sink.
 * @param sinkPolicy The sink policy for the sink. Must be non-NULL.
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if the request was successful. The sink is configured
 *    immediately.
 *  - CHRE_STATUS_INVALID_ARGUMENT if sinkPolicy is NULL.
 *  - CHRE_STATUS_NOT_FOUND if the sink does not exist on the data flow.
 *  - CHRE_STATUS_PERMISSION_DENIED if the source does not own the data
 *    flow.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSourceConfigureSink(uint64_t hubId,
    uint64_t endpointId, uint32_t dataFlowId,
    const struct chreDataFlowSinkPolicy *sinkPolicy);

/**
 * Reserves contiguous space in the data flow for numBytes bytes. This
 * function returns the number of bytes that were successfully reserved,
 * which can be 0 or fewer than numBytes. *data will point to
 * the reserved memory if successful or NULL if this function returns 0.
 *
 * If there is enough memory available to write all of numBytes, but in
 * different contiguous blocks, this function will return the number of bytes
 * that were successfully reserved in a single contiguous block. The nanoapp
 * should call this function again to reserve the remaining space.
 *
 * Reservations allow the nanoapp to reserve a chunk of data in the data flow
 * and then write to it later. This allows the platform to know how much space
 * to reserve for the data, which can then be used to provide flow control
 * feedback to the source.
 *
 * @param dataFlowId The ID of the data flow on which to reserve space.
 * @param numBytes The number of bytes for which to reserve space.
 * @param data A pointer to the reserved memory if successful, otherwise
 *     NULL.
 * @param reservedBytes A pointer to an integer to store the number of bytes
 *     that were successfully reserved.
 * @return One of chreStatus:
 *  - CHRE_STATUS_OK on success.
 *  - CHRE_STATUS_PERMISSION_DENIED if the data flow is not owned by this
 *    nanoapp.
 *  - CHRE_STATUS_INVALID_ARGUMENT if numBytes is not a multiple of the element
 *    size for only a fixed-size data flow or if data or reservedBytes is
 *    NULL.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSourceReserve(uint32_t dataFlowId, uint32_t numBytes,
                                   void **data, uint32_t *reservedBytes);

/**
 * Releases the first numBytes bytes reserved for writing. This function
 * returns CHRE_STATUS_OK if the request was successful, i.e. if the
 * data flow is valid and there are numBytes that can be released after being
 * reserved. If the data flow is not owned by this nanoapp, this function will
 * return CHRE_STATUS_PERMISSION_DENIED.
 *
 * @param dataFlowId The ID of the data flow on which to release space.
 * @param numBytes The number of bytes to release. Must be a multiple of the
 *     element size for only a fixed-size data flow. Must be less than or equal
 *     to the number of bytes reserved for writing.
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if the request was successful.
 *  - CHRE_STATUS_PERMISSION_DENIED if the data flow is not owned by this
 *    nanoapp.
 *  - CHRE_STATUS_INVALID_ARGUMENT if numBytes is not a multiple of the element
 *    size for only a fixed-size data flow or if numBytes is greater than the
 *    number of bytes reserved for writing.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSourceCommit(uint32_t dataFlowId, uint32_t numBytes);

/**
 * Pushes the given data into the data flow. *numberOfBytesPushed will be
 * populated with the number of bytes that were successfully pushed.
 *
 * @param dataFlowId The ID of the data flow on which to push elements.
 * @param data The data to push into the data flow. Must be at least numBytes in
 *     size. Cannot be NULL.
 * @param numBytes The number of bytes in data to push. Must be a multiple of
 *     the element size for only a fixed-size data flow.
 * @param allOrNothing If true, either all or none of the bytes will be
 *     pushed. If false, any number of bytes may be pushed, depending on the
 *     available space in the data flow.
 * @param numberOfBytesPushed A pointer to an integer to store the number of
 *     bytes that were successfully pushed. This value will be less than or
 *     equal to numBytes. Cannot be NULL.
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK on success.
 *  - CHRE_STATUS_PERMISSION_DENIED if the data flow is not owned by this
 *    nanoapp.
 *  - CHRE_STATUS_RESOURCE_EXHAUSTED if the data flow is full and
 *    allOrNothing is true.
 *  - CHRE_STATUS_FAILED_PRECONDITION if there is an active reservation.
 *  - CHRE_STATUS_INVALID_ARGUMENT if numBytes is 0 or not a multiple of the
 *    element size for only a fixed-size data flow, if data or
 *    numberOfBytesPushed is NULL.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSourcePush(uint32_t dataFlowId, const void *data,
                                uint32_t numBytes, bool allOrNothing,
                                uint32_t *numberOfBytesPushed);

/**
 * Returns the current depth of the data flow in bytes with respect to the
 * furthest behind not-overwritten sink. If includeReserved is true, the
 * size of data that has been reserved but not yet committed is added to the
 * returned value.
 *
 * @param dataFlowId The ID of the data flow on which to get the size.
 * @param includeReserved If true, include reserved bytes in the count.
 * @param size A pointer to an integer to store the depth of the data flow in
 *     bytes. Cannot be NULL.
 *
 * @return One of chreStatus:
 *  - CHRE_STATUS_OK on success.
 *  - CHRE_STATUS_PERMISSION_DENIED if the data flow is not owned by this
 *    nanoapp.
 *  - CHRE_STATUS_INVALID_ARGUMENT if size is NULL.
 *
 * @see chreDataFlowSourceGetCapacity
 *
 * @since v1.12
 */
uint32_t chreDataFlowSourceGetSize(uint32_t dataFlowId, bool includeReserved,
                                   uint32_t *size);

/**
 * Returns the capacity of the data flow in number of bytes. This is the maximum
 * number of bytes that can be pushed into the data flow and is fixed at data
 * flow creation.
 *
 * @param dataFlowId The ID of the data flow on which to get the capacity.
 * @param capacity A pointer to an integer to store the capacity of the data
 *     flow in number of bytes. Cannot be NULL.
 * @return One of chreStatus:
 *  - CHRE_STATUS_OK on success.
 *  - CHRE_STATUS_PERMISSION_DENIED if the data flow is not owned by this
 *    nanoapp.
 *  - CHRE_STATUS_INVALID_ARGUMENT if capacity is NULL.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSourceGetCapacity(uint32_t dataFlowId, uint32_t *capacity);

/**
 * Enables this nanoapp to be a sink of the given data flow. This function
 * returns CHRE_STATUS_OK if the sink is enabled, or an error
 * status otherwise. If the sink is enabled, the nanoapp will receive data
 * flow events for this data flow and can start using the data flow sink
 * API. This function should be called during the handling of the
 * CHRE_EVENT_DATA_FLOW_SINK_CREATED event.
 *
 * @param hubId The ID of the hub associated with the data flow source.
 * @param dataFlowId The ID of the data flow on which to enable the sink.
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if the sink is enabled.
 *  - CHRE_STATUS_ALREADY_EXISTS if this nanoapp is already a sink of the
 *    data flow.
 *  - CHRE_STATUS_NOT_FOUND if the source did not create a sink for this
 *    nanoapp.
 *
 * @see chreDataFlowSinkDisable
 *
 * @since v1.12
 */
uint32_t chreDataFlowSinkEnable(uint64_t hubId, uint32_t dataFlowId);

/**
 * Disables this nanoapp as a sink of the given data flow. If the sink is
 * disabled, the nanoapp will not receive any more data flow events for this
 * data flow. This operation is final, and any subsequent calls to
 * chreDataFlowSinkEnable() will fail. To re-enable the sink, the source must
 * re-create the sink in the same manner as when it was first created.
 *
 * If the source is a nanoapp, it will receive a
 * CHRE_EVENT_DATA_FLOW_SINK_STOPPED event, indicating this sink has been
 * disabled.
 *
 * @param hubId The ID of the hub associated with the data flow source.
 * @param dataFlowId The ID of the data flow on which to disable the sink.
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if the sink is disabled.
 *  - CHRE_STATUS_NOT_FOUND if this nanoapp is not a sink of the data
 *    flow.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSinkDisable(uint64_t hubId, uint32_t dataFlowId);

/**
 * Gets the state of the sink on the given data flow.
 *
 * @param hubId The ID of the hub associated with the data flow.
 * @param dataFlowId The ID of the data flow on which to get the state.
 * @return The state of the sink, one of chreStatus.
 *  - CHRE_STATUS_OK if the sink is enabled.
 *  - CHRE_STATUS_ABORTED if the data flow has been destroyed.
 *  - CHRE_STATUS_NOT_FOUND if this nanoapp is not an active sink of the
 *    data flow.
 *  - CHRE_STATUS_DATA_LOSS if the source overwrote the sink's position. This
 *    status is purely informational. The data flow is still usable, but this
 *    sink nanoapp's position has been moved forward. This status remains until
 *    the sink reads data from the data flow.
 *
 * @see chreStatus
 *
 * @since v1.12
 */
uint32_t chreDataFlowSinkGetState(uint64_t hubId, uint32_t dataFlowId);

/**
 * Returns a const view over the next contiguous block of data, up to
 * numRequestedBytes. This function returns CHRE_STATUS_OK if the
 * request was successful, i.e. if the sink is valid on a valid data flow
 * and there are greater than zero bytes available to consume. *data and
 * *numBytes will contain the available data to consume if successful, otherwise
 * they will be unchanged. This data will follow the data previously peeked.
 *
 * WARNING: If the source configured this sink to be overwritable, it is
 * expected that the source may overwrite this sink. The contents of a
 * peek are only guaranteed to have been valid if the subsequent call to
 * chreDataFlowSinkRelease() succeeded. chreDataFlowSinkGetState() may
 * be used to confirm the validity of the data in the middle of a long-running
 * operation without calling chreDataFlowSinkRelease().
 *
 * @param hubId The ID of the hub associated with the data flow.
 * @param dataFlowId The ID of the data flow on which to get the available
 *     count.
 * @param numRequestedBytes The requested number of bytes to peek.
 * @param data A pointer to a buffer to store the peeked bytes if
 *     successful, otherwise unchanged. This pointer is only valid in the
 *     nanoapp event context in which this function is called.
 * @param numBytes A pointer to an integer to store the number of bytes
 *     available to peek if successful, otherwise unchanged. This value will
 *     be less than or equal to numRequestedBytes.
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if the request was successful, i.e. if the sink is
 *    valid on a valid data flow and there are greater than zero bytes
 *    available to consume.
 *  - CHRE_STATUS_INVALID_ARGUMENT if numRequestedBytes is not a multiple of
 *    the element size.
 *  - CHRE_STATUS_NOT_FOUND if the sink is not enabled.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSinkPeek(uint64_t hubId, uint32_t dataFlowId,
                              uint32_t numRequestedBytes,
                              const void **data, uint32_t *numBytes);

/**
 * Releases the first numBytes bytes from the data flow. This function returns
 * true if the request was successful, i.e. if the sink is valid on a valid
 * data flow and there are numBytes bytes that can be released after being
 * consumed.
 *
 * NOTE: This invalidates the pointers and associated values previously returned
 * by chreDataFlowSinkPeek() if the request was successful.
 *
 * If numBytes is not a multiple of the element size provided to the sink
 * nanoapp by the CHRE_DATA_FLOW_SINK_CREATED event, this function will
 * return CHRE_STATUS_INVALID_ARGUMENT.
 *
 * @param hubId The ID of the hub associated with the data flow.
 * @param dataFlowId The ID of the data flow on which to release the consumed
 *                   bytes.
 * @param numBytes The number of bytes to release.
 *
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if the request was successful.
 *  - CHRE_STATUS_NOT_FOUND if the sink is not enabled.
 *  - CHRE_STATUS_INVALID_ARGUMENT if numBytes is not a multiple of the
 *    element size.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSinkRelease(uint64_t hubId, uint32_t dataFlowId,
                                 uint32_t numBytes);

/**
 * Seeks the sink's read pointer on the given data flow to an offset defined as
 * the number of bytes behind the current write index. An offset of zero will
 * seek the sink to the current write index of the source, skipping over any and
 * all data currently available in the flow.
 *
 * NOTE: The sink's read pointer is initialized to the current write index
 * during sink creation.
 *
 * @param hubId The ID of the hub associated with the data flow.
 * @param dataFlowId The ID of the data flow.
 * @param offset The number of bytes behind the current write index of which to
 *     seek the sink.
 *
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if the request was successful and the sink was
 *    seeked to the specified offset.
 *  - CHRE_STATUS_INVALID_ARGUMENT if the offset is not a multiple of the
 *    element size for a fixed-size data flow, or if the offset is greater
 *    than the current size of the data flow.
 *  - CHRE_STATUS_NOT_FOUND if the sink is not enabled.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSinkSeek(uint64_t hubId, uint32_t dataFlowId,
                              uint32_t offset);

/**
 * Retrieve the number of bytes available for this sink to read, i.e. the
 * distance between the data flow's write index and this sink's read index.
 *
 * @param hubId The ID of the hub associated with the data flow.
 * @param dataFlowId The ID of the data flow.
 * @param offset A pointer to an integer to store the offset in bytes.
 *     Cannot be NULL.
 *
 * @return one of chreStatus:
 *  - CHRE_STATUS_OK if the request was successful and offset was populated.
 *  - CHRE_STATUS_INVALID_ARGUMENT if offset is NULL.
 *  - CHRE_STATUS_NOT_FOUND if the sink is not enabled.
 *
 * @since v1.12
 */
uint32_t chreDataFlowSinkGetOffset(uint64_t hubId, uint32_t dataFlowId,
                                   uint32_t *offset);

#ifdef __cplusplus
}
#endif

#endif /* _CHRE_DATA_FLOW_H_ */
