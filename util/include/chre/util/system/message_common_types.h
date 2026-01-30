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

#ifndef CHRE_UTIL_SYSTEM_MESSAGE_COMMON_TYPES_H_
#define CHRE_UTIL_SYSTEM_MESSAGE_COMMON_TYPES_H_

#include "pw_allocator/unique_ptr.h"
#include "pw_span/span.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace chre::message {

//! The ID of a MessageHub
using MessageHubId = uint64_t;

//! The ID of an endpoint
using EndpointId = uint64_t;

//! The ID of a session
using SessionId = uint16_t;

//! An invalid MessageHub ID
constexpr MessageHubId MESSAGE_HUB_ID_INVALID = 0;

//! A MessageHub ID that matches any MessageHub
constexpr MessageHubId MESSAGE_HUB_ID_ANY = MESSAGE_HUB_ID_INVALID;

//! An invalid endpoint ID
constexpr EndpointId ENDPOINT_ID_INVALID = 0;

//! An endpoint ID that matches any endpoint
constexpr EndpointId ENDPOINT_ID_ANY = ENDPOINT_ID_INVALID;

//! An invalid session ID
constexpr SessionId SESSION_ID_INVALID = UINT16_MAX;

//! Endpoint types
enum class EndpointType : uint8_t {
  HOST_FRAMEWORK = 1,
  HOST_APP = 2,
  HOST_NATIVE = 3,
  NANOAPP = 4,
  GENERIC = 5,
};

//! Endpoint permissions
//! This should match the CHRE_MESSAGE_PERMISSION_* constants.
enum class EndpointPermission : uint32_t {
  NONE = 0,
  AUDIO = 1,
  GNSS = 1 << 1,
  WIFI = 1 << 2,
  WWAN = 1 << 3,
  BLE = 1 << 4,
};

//! The reason for closing a session
enum class Reason : uint8_t {
  UNSPECIFIED = 0,
  OUT_OF_MEMORY,
  TIMEOUT,
  OPEN_ENDPOINT_SESSION_REQUEST_REJECTED,
  CLOSE_ENDPOINT_SESSION_REQUESTED,
  ENDPOINT_INVALID,
  ENDPOINT_GONE,
  ENDPOINT_CRASHED,
  HUB_RESET,
  PERMISSION_DENIED,
};

//! The format of an RPC message sent using a service
enum class RpcFormat : uint8_t {
  CUSTOM = 0,
  AIDL,
  PW_RPC_PROTOBUF,
};

//! Represents a single endpoint connected to a MessageHub
struct Endpoint {
  MessageHubId messageHubId;
  EndpointId endpointId;

  Endpoint()
      : messageHubId(MESSAGE_HUB_ID_INVALID), endpointId(ENDPOINT_ID_INVALID) {}

  Endpoint(MessageHubId initMessageHubId, EndpointId initEndpointId)
      : messageHubId(initMessageHubId), endpointId(initEndpointId) {}

  bool operator==(const Endpoint &other) const {
    return messageHubId == other.messageHubId && endpointId == other.endpointId;
  }

  bool operator!=(const Endpoint &other) const {
    return !(*this == other);
  }
};

//! Represents a session between two endpoints
struct Session {
  static constexpr size_t kMaxServiceDescriptorLength = 127;

  Session()
      : sessionId(SESSION_ID_INVALID),
        isActive(false),
        hasServiceDescriptor(false) {
    serviceDescriptor[0] = '\0';
  }

  Session(SessionId initSessionId, Endpoint initInitiator, Endpoint initPeer,
          const char *initServiceDescriptor)
      : sessionId(initSessionId),
        isActive(false),
        hasServiceDescriptor(initServiceDescriptor != nullptr),
        initiator(initInitiator),
        peer(initPeer) {
    if (initServiceDescriptor != nullptr) {
      std::strncpy(this->serviceDescriptor, initServiceDescriptor,
                   kMaxServiceDescriptorLength);
    } else {
      this->serviceDescriptor[0] = '\0';
    }
    this->serviceDescriptor[kMaxServiceDescriptorLength] = '\0';
  }

  SessionId sessionId;
  bool isActive;
  bool hasServiceDescriptor;
  Endpoint initiator;
  Endpoint peer;
  char serviceDescriptor[kMaxServiceDescriptorLength + 1];

  bool operator==(const Session &other) const {
    return sessionId == other.sessionId && initiator == other.initiator &&
           peer == other.peer && isActive == other.isActive &&
           hasServiceDescriptor == other.hasServiceDescriptor &&
           (!hasServiceDescriptor ||
            std::strncmp(serviceDescriptor, other.serviceDescriptor,
                         kMaxServiceDescriptorLength) == 0);
  }

  bool operator!=(const Session &other) const {
    return !(*this == other);
  }

  //! @return true if the two sessions are equivalent, i.e. they have the same
  //! endpoints and service descriptor (if present), false otherwise
  bool isEquivalent(const Session &other) const {
    bool sameEndpoints = (initiator == other.initiator && peer == other.peer) ||
                         (initiator == other.peer && peer == other.initiator);
    return hasServiceDescriptor == other.hasServiceDescriptor &&
           sameEndpoints &&
           (!hasServiceDescriptor ||
            std::strncmp(serviceDescriptor, other.serviceDescriptor,
                         kMaxServiceDescriptorLength) == 0);
  }
};

//! Represents a message sent using the MessageRouter
struct Message {
  Endpoint sender;
  Endpoint recipient;
  SessionId sessionId;
  pw::UniquePtr<std::byte[]> data;
  uint32_t messageType;
  uint32_t messagePermissions;

  Message()
      : sessionId(SESSION_ID_INVALID),
        data(nullptr),
        messageType(0),
        messagePermissions(0) {}

  Message(pw::UniquePtr<std::byte[]> &&ourData, uint32_t initMessageType,
          uint32_t initMessagePermissions, Session initSession,
          bool initSentBySessionInitiator)
      : sender(initSentBySessionInitiator ? initSession.initiator
                                          : initSession.peer),
        recipient(initSentBySessionInitiator ? initSession.peer
                                             : initSession.initiator),
        sessionId(initSession.sessionId),
        data(std::move(ourData)),
        messageType(initMessageType),
        messagePermissions(initMessagePermissions) {}

  Message(const Message &) = delete;
  Message &operator=(const Message &) = delete;

  Message(Message &&other)
      : sender(other.sender),
        recipient(other.recipient),
        sessionId(other.sessionId),
        data(std::move(other.data)),
        messageType(other.messageType),
        messagePermissions(other.messagePermissions) {}

  Message &operator=(Message &&other) {
    sender = other.sender;
    recipient = other.recipient;
    sessionId = other.sessionId;
    data = std::move(other.data);
    messageType = other.messageType;
    messagePermissions = other.messagePermissions;
    return *this;
  }
};

//! Represents an identifier for a shared data flow between endpoints.
struct DataFlowId {
  //! The ID of the hub the data flow source endpoint is on.
  MessageHubId hubId;

  //! The ID of the data flow scoped to hubId.
  uint32_t id;
};

//! Represents a data flow sink registration.
struct DataFlowSinkRegistration {
  //! Id of the data flow.
  DataFlowId dataFlowId;

  //! The data flow source endpoint.
  Endpoint sourceId;

  //! The endpoint being registered as a sink.
  Endpoint sinkId;

  //! Id of the primary region.
  int32_t primaryRegionId;

  //! Offset of the metadata in the primary region.
  uint32_t metadataOffset;

  //! Optional ID of the region containing the metadata of the new sink.
  int32_t sinkMetadataRegionId;

  //! Offset of the sink metadata.
  uint32_t sinkMetadataOffset;

  //! Optional message used to pass this registration over an existing session.
  std::optional<Message> sessionMessage;
};

//! @brief Represents a data flow sink unregistration.
struct DataFlowSinkUnregistration {
  //! Id of the data flow.
  DataFlowId dataFlowId;

  //! The endpoint being removed from the flow.
  Endpoint endpoint;
};

//! Represents a data flow stopped event.
struct DataFlowStopped {
  //! Id of the data flow that stopped.
  DataFlowId dataFlowId;

  //! Optional list of endpoints to notify.
  std::optional<pw::span<Endpoint>> destinationEndpoints;
};

//! Represents a data flow alert.
struct DataFlowAlert {
  //! Id of the data flow the alert is associated with.
  DataFlowId dataFlowId;

  //! The sending endpoint.
  Endpoint sender;

  //! The list of receiving endpoints.
  pw::span<Endpoint> receiverEndpoints;
};

}  // namespace chre::message

#endif  // CHRE_UTIL_SYSTEM_MESSAGE_COMMON_TYPES_H_
