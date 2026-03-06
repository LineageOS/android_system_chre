/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef CHRE_UTIL_SYSTEM_MESSAGE_ROUTER_H_
#define CHRE_UTIL_SYSTEM_MESSAGE_ROUTER_H_

#include "chre/platform/mutex.h"
#include "chre/util/dynamic_vector.h"
#include "chre/util/lock_guard.h"
#include "chre/util/memory.h"
#include "chre/util/singleton.h"
#include "chre/util/system/intrusive_ref_base.h"
#include "chre/util/system/message_common.h"
#include "chre/util/system/message_hub.h"
#include "chre/util/system/message_hub_callback.h"
#include "chre/util/system/message_hub_callback_v2.h"

#include "pw_allocator/unique_ptr.h"
#include "pw_containers/vector.h"
#include "pw_function/function.h"
#include "pw_intrusive_ptr/intrusive_ptr.h"
#include "pw_intrusive_ptr/recyclable.h"
#include "pw_span/span.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace chre::message {

//! MessageRouter routes messages between endpoints connected to MessageHubs. It
//! provides an API for registering MessageHubs, opening and closing sessions,
//! and sending messages between endpoints. Each MessageHub is expected to
//! register a callback to handle messages sent to its endpoints and other
//! functions to provide information about the endpoints connected to it.
//!
//! MessageRouter is thread-safe.
//!
//! Usage:
//! 1. Create a MessageRouter instance.
//! 2. Register MessageHubs with the MessageRouter. Each MessageHub must have
//!    a unique ID and a callback to handle messages sent to its endpoints.
//! 3. Open sessions from endpoints connected to MessageHubs to endpoints
//!    connected to other MessageHubs.
//! 4. Send messages to endpoints using the MessageRouter API.
//! 5. Close sessions when they are no longer needed.
class MessageRouter {
 public:
  using MessageHubCallback = chre::message::MessageHubCallback;
  using MessageHubCallbackV2 = chre::message::MessageHubCallbackV2;
  using MessageHub = chre::message::MessageHub;

  //! Represents a MessageHub and its connected endpoints
  struct MessageHubRecord {
    MessageHubInfo info;
    pw::IntrusivePtr<MessageHubCallback> callback;
    uint8_t version;
  };

  //! The default reserved session ID value
  static constexpr SessionId kDefaultReservedSessionId = 0x8000;

  MessageRouter() = delete;

  //! Constructor for the MessageRouter.
  //! @param messageHubs The list of MessageHubs connected to the MessageRouter
  //! @param sessions The list of sessions connected to the MessageRouter
  //! @param reservedSessionId The first reserved session ID - MessageRouter
  //! will not assign session IDs greater than or equal to this value
  MessageRouter(pw::Vector<MessageHubRecord> &messageHubs,
                pw::Vector<Session> &sessions,
                SessionId reservedSessionId = kDefaultReservedSessionId)
      : kReservedSessionId(reservedSessionId),
        mMessageHubs(messageHubs),
        mSessions(sessions) {}

  //! Registers a MessageHub with the MessageRouter.
  //! The provided name must be unique and not registered before and be a valid
  //! C string. The data underlying name must outlive the MessageHub. The
  //! callback must outlive the MessageHub. The ID must be unique and not
  //! registered before. When the returned MessageHub is destroyed, it will
  //! unregister itself from the MessageRouter.
  //! @param name The name of the MessageHub
  //! @param id The ID of the MessageHub
  //! @param callback The callback to handle messages sent to the MessageHub
  //! @return The MessageHub API or std::nullopt if the MessageHub could not be
  //! registered
  std::optional<MessageHub> registerMessageHub(
      const char *name, MessageHubId id,
      pw::IntrusivePtr<MessageHubCallback> callback);

  //! Registers a V2 MessageHub with the MessageRouter.
  //! @see registerMessageHub
  std::optional<MessageHub> registerMessageHubV2(
      const char *name, MessageHubId id,
      pw::IntrusivePtr<MessageHubCallbackV2> callback);

  //! Executes the function for each endpoint connected to this MessageHub.
  //! If function returns true, the iteration will stop.
  //! @return true if the MessageHub is found, false otherwise
  bool forEachEndpointOfHub(
      MessageHubId messageHubId,
      const pw::Function<bool(const EndpointInfo &)> &function);

  //! Executes the function for each endpoint connected to all Message Hubs.
  //! @return true if successful, false otherwise
  bool forEachEndpoint(
      const pw::Function<void(const MessageHubInfo &, const EndpointInfo &)>
          &function);

  //! @return The EndpointInfo for the given hub and endpoint IDs
  std::optional<EndpointInfo> getEndpointInfo(MessageHubId messageHubId,
                                              EndpointId endpointId);

  //! @return The Endpoint for the given service descriptor. If multiple
  //! endpoints have the same service descriptor, the first one is returned.
  //! If the message hub ID is MESSAGE_HUB_ID_ANY, all message hubs are
  //! searched.
  std::optional<Endpoint> getEndpointForService(MessageHubId messageHubId,
                                                const char *serviceDescriptor);

  //! @return true if the endpoint has the given service descriptor, a
  //! null-terminated ASCII string, false otherwise.
  bool doesEndpointHaveService(MessageHubId messageHubId, EndpointId endpointId,
                               const char *serviceDescriptor);

  //! @return The first MessageHub ID for the given endpoint ID
  MessageHubId findDefaultMessageHubId(EndpointId endpointId);

  //! Searches for an endpoint with the given hub ID, endpoint ID, and service
  //! descriptor. The hubId can be MESSAGE_HUB_ID_ANY to search for the
  //! endpoint on any hub, the endpointId can be ENDPOINT_ID_ANY to search for
  //! the endpoint on any hub, or the service descriptor can be non-nullptr to
  //! search for any endpoint that has the service.
  //! @return the endpoint if found, std::nullopt otherwise.
  std::optional<Endpoint> searchForEndpoint(MessageHubId messageHubId,
                                            EndpointId endpointId,
                                            const char *serviceDescriptor);

  //! Executes the function for each service provided by an endpoint connected
  //! to this MessageHub. If function returns true, the iteration will stop.
  //! @return true if successful, false otherwise
  bool forEachService(
      const pw::Function<bool(const MessageHubInfo &, const EndpointInfo &,
                              const ServiceInfo &)> &function);

  //! Executes the function for each MessageHub connected to the
  //! MessageRouter. If function returns true, the iteration will stop.
  //! @return true if successful, false if failed
  bool forEachMessageHub(
      const pw::Function<bool(const MessageHubInfo &)> &function);

 private:
  friend class chre::message::MessageHub;

  //! Represents a list of endpoints connected to a MessageHub that will receive
  //! a callback.
  struct HubCallbackRecipients {
    MessageHubId hubId;
    pw::IntrusivePtr<MessageHubCallbackV2> callback;
    DynamicVector<Endpoint> endpoints;
  };

  //! Registers a MessageHub with the MessageRouter.
  //! @see registerMessageHub
  //! @param version The version of the callback to register.
  std::optional<MessageHub> registerMessageHub(
      const char *name, MessageHubId id,
      pw::IntrusivePtr<MessageHubCallback> callback, uint8_t version);

  //! Unregisters a MessageHub from the MessageRouter. This function will
  //! close all sessions that were initiated by or connected to the MessageHub
  //! and destroy the MessageHubRecord. This function will call the callback
  //! for each session that was closed only for the other message hub in the
  //! session.
  //! @return true if the MessageHub was unregistered, false if the MessageHub
  //! was not found.
  bool unregisterMessageHub(MessageHubId fromMessageHubId);

  //! Accepts the session open request from the peer message hub.
  //! onSessionOpened will be called on both hubs.
  void onSessionOpenComplete(MessageHubId fromMessageHubId,
                             SessionId sessionId);

  //! Opens a session from an endpoint connected to the current MessageHub
  //! to the listed MessageHub ID and endpoint ID, with the given service
  //! descriptor, a null-terminated ASCII string.
  //! onSessionOpenRequest will be called to request the session to be
  //! opened. Once the peer message hub calls onSessionOpenComplete or
  //! closeSession, onSessionOpened or onSessionClosed will be called,
  //! depending on the result. If the session ID is provided (not
  //! SESSION_ID_INVALID), it must be unique and from the reserved session ID
  //! range. MessageRouter does not guarantee anything about the session ID if
  //! it is provided in this API. If the session ID is not provided,
  //! MessageRouter will assign a session ID normally.
  //! @return The session ID or SESSION_ID_INVALID if the session could not be
  //! opened
  SessionId openSession(MessageHubId fromMessageHubId,
                        EndpointId fromEndpointId, MessageHubId toMessageHubId,
                        EndpointId toEndpointId,
                        const char *serviceDescriptor = nullptr,
                        SessionId sessionId = SESSION_ID_INVALID);

  //! Closes the session with sessionId and reason
  //! @return true if the session was closed, false if the session was not
  //! found
  bool closeSession(MessageHubId fromMessageHubId, SessionId sessionId,
                    Reason reason = Reason::CLOSE_ENDPOINT_SESSION_REQUESTED);

  //! Finalizes the session with sessionId and reason. If reason is provided,
  //! the session will be closed, else the session will be fully opened.
  //! @return true if the session was finalized, false if the session was not
  //! found or one of the message hubs were not found or not linked to the
  //! session.
  bool finalizeSession(MessageHubId fromMessageHubId, SessionId sessionId,
                       std::optional<Reason> reason);

  //! Returns a session if it exists
  //! @return The session or std::nullopt if the session was not found
  std::optional<Session> getSessionWithId(MessageHubId fromMessageHubId,
                                          SessionId sessionId);

  //! Sends a message to the session specified by sessionId.
  //! @see chreSendReliableMessageAsync. Sends the message in a reliable
  //! manner if possible. If the message cannot be delivered, the session
  //! is closed and subsequent calls to this function with the same sessionId
  //! will return false.
  //! @see MessageHub::sendMessage
  //! @return true if the message was sent, false if the message could not be
  //! sent
  bool sendMessage(pw::UniquePtr<std::byte[]> &&data, uint32_t messageType,
                   uint32_t messagePermissions, SessionId sessionId,
                   EndpointId fromEndpointId, MessageHubId fromMessageHubId);

  //! Registers an endpoint with the MessageHub.
  //! @return true if the endpoint was registered, otherwise false.
  bool registerEndpoint(MessageHubId messageHubId, EndpointId endpointId);

  //! Unregisters an endpoint from the MessageHub.
  //! @return true if the endpoint was unregistered, otherwise false.
  bool unregisterEndpoint(MessageHubId messageHubId, EndpointId endpointId);

  //! Helper function for registering or unregistering an endpoint with a
  //! MessageHub.
  //! @return true if the endpoint was registered or unregistered, otherwise
  //! false.
  bool onEndpointRegistrationStateChanged(MessageHubId messageHubId,
                                          EndpointId endpointId,
                                          bool isRegistered);

  //! Registers a data flow sink.
  //! @return true if the sink was registered, false otherwise.
  bool registerDataFlowSink(DataFlowSinkRegistration &&registration);

  //! Reports that a data flow sink has been unregistered.
  void reportDataFlowSinkUnregistered(
      const DataFlowSinkUnregistration &unregistration);

  //! Reports that a data flow has stopped.
  void reportDataFlowStopped(const DataFlowStopped &stopped);

  //! Reports a data flow alert.
  void reportDataFlowAlert(const DataFlowAlert &alert);

  //! @return The a copy of the list of MessageHubRecords
  std::optional<DynamicVector<MessageHubRecord>> getMessageHubRecords();

  //! @return A copy of the list of MessageHubRecords while holding mMutex.
  std::optional<DynamicVector<MessageHubRecord>> getMessageHubRecordsLocked();

  //! @return The MessageHubRecord for the given MessageHub ID
  const MessageHubRecord *getMessageHubRecordLocked(MessageHubId messageHubId);

  //! @return The index of the session if it exists
  //! Requires the caller to hold the mutex
  std::optional<size_t> findSessionIndexLocked(MessageHubId fromMessageHubId,
                                               SessionId sessionId);

  //! @return The callback for the given MessageHub ID or nullptr if not found
  template <typename T = MessageHubCallback>
  pw::IntrusivePtr<MessageHubCallback> getCallbackFromMessageHubId(
      MessageHubId messageHubId) {
    LockGuard<Mutex> lock(mMutex);
    return getCallbackFromMessageHubIdLocked<T>(messageHubId);
  }

  //! @return The callback for the given MessageHub ID or nullptr if not found
  template <typename T = MessageHubCallback>
  pw::IntrusivePtr<T> getCallbackFromMessageHubIdLocked(
      MessageHubId messageHubId, uint8_t minVersion = 1) {
    const MessageHubRecord *record = getMessageHubRecordLocked(messageHubId);
    if (record == nullptr || record->version < minVersion) {
      return nullptr;
    }
    return pw::IntrusivePtr<T>(static_cast<T *>(record->callback.get()));
  }

  //! @return true if the endpoint exists in the MessageHub with the given
  //! callback
  bool checkIfEndpointExists(
      const pw::IntrusivePtr<MessageHubCallback> &callback,
      EndpointId endpointId);

  //! Verifies that the session with the given ID exists and is active.
  //! Also verifies that the sender endpoint is part of the session and that the
  //! recipient endpoint is on the toHubId if it is not MESSAGE_HUB_ID_ANY.
  //! @param sessionId The ID of the session
  //! @param fromMessageHubId The ID of the sender's message hub
  //! @param fromEndpointId The ID of the sender's endpoint. May be
  //! ENDPOINT_ID_ANY to infer the endpoint ID from the session.
  //! @param toHubId The ID of the recipient's message hub. If not set to
  //! MESSAGE_HUB_ID_ANY, will be used to verify the recipient hub.
  //! @param toEndpointId The ID of the recipient endpoint. If not set to
  //! ENDPOINT_ID_ANY, will be used to verify the recipient endpoint.
  //! @param errorTag The tag to use in log messages
  //! @return The session if it exists and is active and a boolean that is true
  //! iff the session initiator sent the message, error otherwise
  std::optional<std::pair<Session, bool>> verifyMessageSessionLocked(
      SessionId sessionId, MessageHubId fromMessageHubId,
      EndpointId fromEndpointId, MessageHubId toHubId, EndpointId toEndpointId,
      const char *errorTag);

  //! @return The list of HubCallbackRecipients for the given endpoints.
  chre::DynamicVector<HubCallbackRecipients> getHubCallbackRecipientList(
      pw::span<const Endpoint> endpoints);

  //! @return The next available Session ID. Will wrap around if needed and
  //! ensures the returned ID is not in the reserved range nor is it already in
  //! use. Requires the caller to hold the mutex.
  SessionId getNextSessionIdLocked();

  //! The mutex to protect MessageRouter state
  Mutex mMutex;

  //! The next available Session ID
  SessionId mNextSessionId = 0;

  //! The start of the reserved session ID range
  const SessionId kReservedSessionId;

  //! The list of MessageHubs connected to the MessageRouter
  pw::Vector<MessageHubRecord> &mMessageHubs;

  //! The list of sessions connected to the MessageRouter
  pw::Vector<Session> &mSessions;
};

//! Define the singleton instance of the MessageRouter
typedef Singleton<MessageRouter> MessageRouterSingleton;

//! Routes messages between MessageHubs
template <size_t kMaxMessageHubs, size_t kMaxSessions>
class MessageRouterWithStorage : public MessageRouter {
 public:
  MessageRouterWithStorage(
      SessionId reservedSessionId = MessageRouter::kDefaultReservedSessionId)
      : MessageRouter(mMessageHubs, mSessions, reservedSessionId) {}

 private:
  //! The list of MessageHubs connected to the MessageRouter
  pw::Vector<MessageHubRecord, kMaxMessageHubs> mMessageHubs;

  //! The list of sessions connected to the MessageRouter
  pw::Vector<Session, kMaxSessions> mSessions;
};

}  // namespace chre::message

#endif  // CHRE_UTIL_SYSTEM_MESSAGE_ROUTER_H_
