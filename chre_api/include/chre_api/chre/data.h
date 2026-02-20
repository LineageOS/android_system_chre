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

#ifndef _CHRE_DATA_H_
#define _CHRE_DATA_H_

/**
 * @file
 * This file defines the API for CHRE data flows, which are designed for
 * efficient high-throughput data transmission between a single source
 * and multiple sinks, which may include nanoapps and other endpoints. These
 * data flows enable the transfer of large amounts of data with minimal data
 * copies, leveraging shared memory regions. They provide a mechanism for
 * nanoapps to exchange data streams, supporting various new data alert and
 * overwrite policies to suit different batching use cases.
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
 *
 * - Sink nanoapp:
 *  - Receives the CHRE_EVENT_DATA_SINK_CREATED event
 *  - Calls chreDataFlowSinkEnable() to enable the sink
 *
 * - Source nanoapp:
 *  - Pushes data into the data flow
 *
 * - Sink nanoapp:
 *  - Receives the CHRE_EVENT_DATA_ALERT event, indicating data is
 *    available (only if never notify was NOT selected) or an underlying change
 *    in the underlying data flow
 *  - Alternatively may at any time use the sink API to poll the data flow
 *  - Processes the data and releases the elements
 *
 * - Either nanoapp:
 *  - Can disable the sink
 *  - Both nanoaps receive a CHRE_EVENT_DATA_SINK_STOPPED event, indicating
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
  CHRE_DATA_SINK_NEW_DATA_ALERT_POLICY_NEVER = 0x0,

  /**
   * The sink will be alerted opportunistically, e.g. when the system or
   * source deems that the sink can be alerted with minimal power impact.
   */
  CHRE_DATA_SINK_NEW_DATA_ALERT_POLICY_OPPORTUNISTIC = 0x1,

  /**
   * The sink will be alerted when the data flow has reached the high water
   * mark. The high water mark is provided when configuring the data flow
   * sink.
   */
  CHRE_DATA_SINK_NEW_DATA_ALERT_POLICY_HIGH_WATER_MARK = 0x2,

  /**
   * The sink will be alerted periodically, with the period specified by
   * the period parameter when configuring the data flow sink. The platform
   * will deliver these new data alerts as soon as possible, but there are no
   * guarantees on when exactly they will be delivered. If no data is
   * available when the new data alert is due, then no new data alert will be
   * sent.
   */
  CHRE_DATA_SINK_NEW_DATA_ALERT_POLICY_PERIODIC = 0x3,

  /**
   * The sink will be alerted when data is available in the data flow,
   * on every write into the data flow. The platform may coalesce these
   * new data alerts or throttle new data alerts to optimize performance or
   * power consumption.
   */
  CHRE_DATA_SINK_NEW_DATA_ALERT_POLICY_STREAMING = 0x4,
};

/**
 * The overwrite policy for a data flow sink. This determines whether the
 * source can overwrite data that is being currently read by the sink.
 *
 * Backing type: uint32_t.
 */
enum chreDataFlowSinkOverwritePolicy {
  /** The sink can be overwritten. */
  CHRE_DATA_SINK_OVERWRITE_POLICY_ALLOWED = 0x0,

  /** The sink can never be overwritten. */
  CHRE_DATA_SINK_OVERWRITE_POLICY_DISALLOWED = 0x1,
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
   *  - CHRE_DATA_SINK_NEW_DATA_ALERT_POLICY_OPPORTUNISTIC
   *    - newDataAlertPolicyData = low watermark value
   *  - CHRE_DATA_SINK_NEW_DATA_ALERT_POLICY_HIGH_WATER_MARK
   *    - newDataAlertPolicyData = high watermark value
   *  - CHRE_DATA_SINK_NEW_DATA_ALERT_POLICY_PERIODIC
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
   * CHRE_DATA_SINK_DOMAIN_* values.
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
 * Data provided in the CHRE_EVENT_DATA_SINK_CREATED event. Only the status and
 * hubId field are valid when this event is received by a source nanoapp.
 */
struct chreDataFlowSinkInfo {
  /** The status of the data flow sink creation request, one of chreStatus. */
  uint32_t status;

  /** The message hub ID of the source. */
  uint64_t hubId;

  /** The endpoint ID of the source. */
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
   * CHRE_DATA_ELEMENT_ALIGNMENT_UNALIGNED, the data flow will have no alignment
   * requirements.
   */
  uint32_t alignment;

  /**
   * If this data flow sink was created with a message, this will be non-NULL
   * and contain the message, else it will be NULL. If this is received by
   * a source nanoapp, this will be NULL.
   */
  struct chreMsgMessageFromEndpointData *messageFromEndpointData;
};

/**
 * Data provided in the CHRE_EVENT_DATA_ALERT event.
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
 * @defgroup CHRE_DATA sink domains
 * @{
 */

#define CHRE_DATA_SINK_DOMAIN_INVALID        (UINT32_C(0))
#define CHRE_DATA_SINK_DOMAIN_LOCAL_NANOAPP  (UINT32_C(1) << 0)
#define CHRE_DATA_SINK_DOMAIN_HOST_AVAILABLE (UINT32_C(1) << 1)
#define CHRE_DATA_SINK_DOMAIN_VENDOR_START   (UINT32_C(1) << 21)
#define CHRE_DATA_SINK_DOMAIN_VENDOR_END     (UINT32_C(1) << 31)

/** @} */

/**
 * Special value for the minimum average write interval parameter (provided when
 * creating a data flow) that hints to the platform that the use case for this
 * data flow requires lower power memory and lower latency, because it writes
 * data frequently.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_MIN_AVERAGE_WRITE_INTERVAL_LOW UINT64_C(0)

/**
 * Special value for the minimum average write interval parameter (provided when
 * creating a data flow) that hints to the platform that the use case for this
 * data flow can tolerate higher power memory and higher latency, because it
 * writes data infrequently.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_MIN_AVERAGE_WRITE_INTERVAL_HIGH UINT64_MAX

/**
 * Special value for the maximum average write bandwidth parameter (provided
 * when creating a data flow) that hints to the platform that this data flow
 * does not require the fastest or most performant hardware configuration,
 * because it writes a low volume of data.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_MAX_AVERAGE_WRITE_BANDWIDTH_LOW UINT32_C(0)

/**
 * Special value for the maximum average write bandwidth parameter (provided
 * when creating a data flow) that hints to the platform that this data flow
 * should use the fastest or most performant hardware configuration, because it
 * writes a high volume of data.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_MAX_AVERAGE_WRITE_BANDWIDTH_HIGH UINT32_MAX

/**
 * Invalid and reserved data flow and sink IDs.
 *
 * @defgroup CHRE_DATA IDs
 * @{
 */

#define CHRE_DATA_ID_INVALID  UINT32_C(0)
#define CHRE_DATA_ID_RESERVED UINT32_C(-1)

#define CHRE_DATA_SINK_ID_INVALID  UINT32_C(0)
#define CHRE_DATA_SINK_ID_RESERVED UINT32_C(-1)

/** @} */

/**
 * Used to signal a data flow has variable size elements.
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_ELEMENT_SIZE_VARIABLE UINT32_C(0)

/**
 * Used to signal that the elements within a data flow have no alignment
 * requirements.
 * @see chreDataFlowCreateAsync
 */
#define CHRE_DATA_ELEMENT_ALIGNMENT_UNALIGNED UINT32_C(0)

/**
 * Used to signal a data flow can grow dynamically, with CHRE setting the size.
 */
#define CHRE_DATA_DYNAMIC_MAX_SIZE UINT32_C(-1)

/**
 * Produce an event ID in the block of IDs reserved for data flow events.
 *
 * Valid input range is [0, 15]. Do not add new events with ID > 15
 * (see chre/event.h)
 *
 * @param offset Index into DATA event ID block; valid range is [0, 15].
 *
 * @defgroup CHRE_DATA_EVENT_ID
 * @{
 */
#define CHRE_DATA_EVENT_ID(offset) (CHRE_EVENT_DATA_FIRST_EVENT + (offset))

/**
 * nanoappHandleEvent argument: struct chreDataFlowCreatedInfo.
 *
 * Event sent when a data flow is created.
 *
 * @see chreDataFlowCreateAsync
 */
#define CHRE_EVENT_DATA_FLOW_CREATED CHRE_DATA_EVENT_ID(0)

/**
 * nanoappHandleEvent argument: struct chreDataFlowStoppedInfo.
 *
 * Event sent when a data flow is destroyed.
 */
#define CHRE_EVENT_DATA_FLOW_STOPPED CHRE_DATA_EVENT_ID(1)

/**
 * nanoappHandleEvent argument: struct chreDataFlowSinkInfo.
 *
 * Event sent to a sink nanoapp when a data flow sink is created for the
 * nanoapp. If the nanoapp wants to be a sink, it must call
 * chreDataFlowSinkEnable() to activate the sink.
 *
 * This event is also sent to the source nanoapp when a data flow sink is
 * created for the source nanoapp, where the source nanoapp is the owner of the
 * data flow.
 *
 * @see chreDataFlowSinkEnable
 */
#define CHRE_EVENT_DATA_SINK_CREATED CHRE_DATA_EVENT_ID(2)

/**
 * nanoappHandleEvent argument: struct chreDataFlowSinkInfo.
 *
 * Event sent to a source nanoapp when a data flow sink is stopped.
 *
 * @see chreDataFlowSinkDisable
 */
#define CHRE_EVENT_DATA_SINK_STOPPED CHRE_DATA_EVENT_ID(3)

/**
 * nanoappHandleEvent argument: struct chreDataFlowNewDataAlert.
 *
 * Event sent when data is available in the data flow for consumption.
 */
#define CHRE_EVENT_DATA_ALERT CHRE_DATA_EVENT_ID(4)

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
 * @param sinkDomains A bitmask of CHRE_DATA_SINK_DOMAIN_* values, indicating
 *     the sink domains that this data flow must be able to support.
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
 *     application. Refer to CHRE_MESSAGE_PERMISSION_* values.
 * @param elementSize The size of each element in bytes. If
 *     CHRE_DATA_ELEMENT_SIZE_VARIABLE, the data flow will have variable
 *     size elements.
 * @param alignment The alignment of each element in bytes. If
 *     CHRE_DATA_ELEMENT_ALIGNMENT_UNALIGNED, the data flow will have no
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
 *     CHRE_DATA_DYNAMIC_MAX_SIZE, then the data flow will be created with
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

#ifdef __cplusplus
}
#endif

#endif /* _CHRE_DATA_H_ */
