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

#ifndef CHRE_UTIL_SYSTEM_MESSAGE_HUB_H_
#define CHRE_UTIL_SYSTEM_MESSAGE_HUB_H_

#include "chre/util/system/message_common.h"

namespace chre::message {

class MessageRouter;

//! The API returned when registering a MessageHub with the MessageRouter.
class MessageHub {
 public:
  //! Creates an empty MessageHub that is not usable, similar to a moved-from
  //! MessageHub. Attempting to call any method on this object results in
  //! undefined behavior.
  MessageHub();

  // There can only be one live MessageHub instance for a given hub ID, so
  // only move operations are supported.
  MessageHub(const MessageHub &) = delete;
  MessageHub &operator=(const MessageHub &) = delete;
  MessageHub(MessageHub &&other);
  MessageHub &operator=(MessageHub &&other);

  //! Destructor. Unregisters the MessageHub from the MessageRouter.
  ~MessageHub();

  //! Accepts the session open request from the peer message hub.
  //! onSessionOpened will be called on both hubs.
  void onSessionOpenComplete(SessionId sessionId);

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
  //! @return The session ID or SESSION_ID_INVALID if the session could
  //! not be opened
  SessionId openSession(EndpointId fromEndpointId, MessageHubId toMessageHubId,
                        EndpointId toEndpointId,
                        const char *serviceDescriptor = nullptr,
                        SessionId sessionId = SESSION_ID_INVALID);

  //! Closes the session with sessionId and reason
  //! @return true if the session was closed, false if the session was not
  //! found
  bool closeSession(SessionId sessionId,
                    Reason reason = Reason::CLOSE_ENDPOINT_SESSION_REQUESTED);

  //! Returns a session if it exists
  //! @return The session or std::nullopt if the session was not found
  std::optional<Session> getSessionWithId(SessionId sessionId);

  //! Sends a message to the session specified by sessionId.
  //! @see chreSendReliableMessageAsync. Sends the message in a reliable
  //! manner if possible. If the message cannot be delivered, the session
  //! is closed and subsequent calls to this function with the same sessionId
  //! will return false.
  //! @param data The data to send
  //! @param messageType The type of the message, a bit flagged value
  //! @param messagePermissions The permissions of the message, a bit flagged
  //! value
  //! @param sessionId The session to send the message on
  //! @param fromEndpointId The endpoint ID of the sender or ENDPOINT_ID_ANY
  //! to allow MessageRouter to infer the sender endpoint ID. If the
  //! sender endpoint ID cannot be inferred, (i.e. the session is between
  //! endpoints on the same message hub), this function will return false.
  //! @return true if the message was sent, false if the message could not be
  //! sent
  bool sendMessage(pw::UniquePtr<std::byte[]> &&data, uint32_t messageType,
                   uint32_t messagePermissions, SessionId sessionId,
                   EndpointId fromEndpointId = ENDPOINT_ID_ANY);

  //! Registers an endpoint with the MessageHub.
  //! @return true if the endpoint was registered, otherwise false.
  bool registerEndpoint(EndpointId endpointId);

  //! Unregisters an endpoint from the MessageHub.
  //! @return true if the endpoint was unregistered, otherwise false.
  bool unregisterEndpoint(EndpointId endpointId);

  //! @return The MessageHub ID of the currently connected MessageHub
  MessageHubId getId();

  //! @return If the MessageHub is active and registered with the
  //! MessageRouter.
  bool isRegistered();

  //! Unregisters this MessageHub from the MessageRouter.
  void unregister();

 private:
  friend class MessageRouter;

  MessageHub(MessageRouter &router, MessageHubId id);

  //! The MessageRouter that this MessageHub is connected to
  MessageRouter *mRouter;

  //! The id of this message hub
  MessageHubId mHubId;
};

}  // namespace chre::message

#endif  // CHRE_UTIL_SYSTEM_MESSAGE_HUB_H_
