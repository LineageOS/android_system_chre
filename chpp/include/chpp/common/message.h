/*
 * Copyright (C) 2025 The Android Open Source Project
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

#ifndef CHPP_MESSAGE_COMMON_H_
#define CHPP_MESSAGE_COMMON_H_

/**
 * @file
 * CHPP Message Service provides a mechanism for components connected
 * via CHPP to tap into the Context Hub Session-Based Messaging network,
 * enabling communication with other endpoints in the network, like nanoapps
 * and GMS Core. It's a bidirectional/peer-to-peer interface, however by
 * convention the service is exposed on a side with the primary CHRE instance,
 * and the clients on the peripherial side of CHPP. Clients should integrate
 * by implementing the API and callbacks defined in chpp/msg/endpoint.h.
 *
 * Defines the commands and common data structure used for client-servicde
 * communication.
 *
 * See chre_api/chre/msg.h for key concepts and usage flows.
 * Also see android.hardware.contexthub.HubEndpoint and related Android APIs
 * for more details.
 */

#include <stdint.h>

#include "chpp/app.h"
#include "chpp/macros.h"
#include "chpp/msg/endpoint.h"
#include "chre_api/chre/msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************************************
 *  Public Definitions
 ***********************************************/
/**
 * The maximum time the CHRE implementation is allowed to elapse before sending
 * an event with the result of an asynchronous request, unless specified
 * otherwise
 */
#define CHRE_MESSAGE_ASYNC_RESULT_TIMEOUT_NS (5 * CHRE_NSEC_PER_SEC)

/**
 * Vendor Message  UUID
 */
#define CHPP_UUID_MESSAGE_STANDARD                 \
  {0xbe, 0x71, 0x22, 0x1e, 0xd3, 0xf5, 0x40, 0x11, \
   0xb5, 0x93, 0xc0, 0x24, 0x6e, 0x10, 0x6e, 0xf9}

/**
 * Data structures for endpoint ready.
 */
CHPP_PACKED_START
struct ChppMsgEndpointReadyParameters {
  uint64_t fromEndpointId;
  uint64_t hubId;
  uint64_t endpointId;
  bool enable;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

CHPP_PACKED_START
struct ChppMsgConfigureEndpointReadyRequest {
  struct ChppAppHeader header;
  ChppMsgEndpointReadyParameters params;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

CHPP_PACKED_START
struct ChppMsgOnEndpointReadyNotification {
  struct ChppAppHeader header;
  ChppMsgEndpointReadyParameters params;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * Data structures for service ready.
 */
CHPP_PACKED_START
struct ChppMsgServiceReadyParameters {
  uint64_t fromEndpointId;
  uint64_t hubId;
  uint64_t endpointId;
  char serviceDescriptor[CHRE_MSG_MAX_SERVICE_DESCRIPTOR_LEN];
  bool enable;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

CHPP_PACKED_START
struct ChppMsgConfigureServiceReadyRequest {
  struct ChppAppHeader header;
  ChppMsgServiceReadyParameters params;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

CHPP_PACKED_START
struct ChppMsgOnServiceReadyNotification {
  struct ChppAppHeader header;
  ChppMsgServiceReadyParameters params;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * Data structures used by open session request.
 */
CHPP_PACKED_START
struct ChppMsgOpenSessionParameters {
  uint64_t fromEndpointId;
  uint64_t hubId;
  uint64_t endpointId;
  char serviceDescriptor[CHRE_MSG_MAX_SERVICE_DESCRIPTOR_LEN];
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

CHPP_PACKED_START
struct ChppMsgOpenSessionRequest {
  struct ChppAppHeader header;
  ChppMsgOpenSessionParameters params;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * Data structures used by other session requests.
 */
CHPP_PACKED_START
struct ChppMsgSessionRequestParameters {
  uint16_t sessionId;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

CHPP_PACKED_START
struct ChppMsgSessionRequest {
  struct ChppAppHeader header;
  struct ChppMsgSessionRequestParameters params;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * Data structures used by send/receive messages.
 */
CHPP_PACKED_START
struct ChppMsgDataParameters {
  uint64_t fromEndpointId;
  uint32_t messageType;
  uint32_t messagePermissions;
  uint16_t sessionId;
  uint32_t size;
  uint8_t data[1];
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

CHPP_PACKED_START
struct ChppMsgSendMessageRequest {
  struct ChppAppHeader header;
  struct ChppMsgDataParameters params;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

CHPP_PACKED_START
struct ChppMsgOnMessageReceivedNotification {
  struct ChppAppHeader header;
  struct ChppMsgDataParameters params;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * Data structures used by session callbacks.
 */
CHPP_PACKED_START
struct ChppMsgSessionNotificationParameters {
  uint64_t hubId;
  uint64_t endpointId;
  char serviceDescriptor[CHRE_MSG_MAX_SERVICE_DESCRIPTOR_LEN];
  uint16_t sessionId;
  uint8_t reason;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

CHPP_PACKED_START
struct ChppMsgSessionNotification {
  struct ChppAppHeader header;
  struct ChppMsgSessionNotificationParameters params;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * Commands used by the Message Service.
 */
enum ChppMsgCommands {
  //! Initializes the service.
  CHPP_MESSAGE_OPEN = 0x0000,

  //! Deinitializes the service.
  CHPP_MESSAGE_CLOSE = 0x0001,

  CHPP_MESSAGE_UNUSED_2 = 0x0002,

  //! Publish services.
  CHPP_MESSAGE_PUBLISH_SERVICES = 0x0003,

  //! Open a session.
  CHPP_MESSAGE_CONFIGURE_ENDPOINT_READY_EVENTS = 0x0004,

  //! Open a session.
  CHPP_MESSAGE_CONFIGURE_SERVICE_READY_EVENTS = 0x0005,

  //! Open a session.
  CHPP_MESSAGE_OPEN_SESSION = 0x0006,

  //! Close a session.
  CHPP_MESSAGE_CLOSE_SESSION = 0x0007,

  //! Complete an open session request.
  CHPP_MESSAGE_OPEN_SESSION_COMPLETE = 0x0008,

  //! Send a message.
  CHPP_MESSAGE_SEND_MESSAGE = 0x0009,

  //! A requested endpoint is ready.
  CHPP_MESSAGE_ON_ENDPOINT_READY = 0x000a,

  //! A requested service is ready.
  CHPP_MESSAGE_ON_SERVICE_READY = 0x000b,

  //! A session is opend.
  CHPP_MESSAGE_ON_SESSION_OPENED = 0x000c,

  //! A session is closed.
  CHPP_MESSAGE_ON_SESSION_CLOSED = 0x000d,

  //! A session is being requested.
  CHPP_MESSAGE_ON_SESSION_OPEN_REQUEST = 0x000e,

  //! A message is received.
  CHPP_MESSAGE_ON_MESSAGE_RECEIVED = 0x000f,
};
#define CHPP_MESSAGE_CLIENT_REQUEST_MAX CHPP_MESSAGE_ON_MESSAGE_RECEIVED

#ifdef __cplusplus
}
#endif

#endif  // CHPP_MESSAGE_COMMON_H_
