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

#ifndef CHRE_UTIL_SYSTEM_MESSAGE_HUB_CALLBACK_H_
#define CHRE_UTIL_SYSTEM_MESSAGE_HUB_CALLBACK_H_

#include "chre/util/system/intrusive_ref_base.h"
#include "chre/util/system/message_common.h"

#include "pw_allocator/unique_ptr.h"
#include "pw_function/function.h"
#include "pw_intrusive_ptr/recyclable.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace chre::message {

//! The callback used to register a MessageHub with the MessageRouter
class MessageHubCallback : public IntrusiveRefBase,
                           public pw::Recyclable<MessageHubCallback> {
 public:
  virtual ~MessageHubCallback() = default;

  //! Message processing callback. If this function returns true,
  //! the MessageHub received the message and will deliver it to the
  //! receiving endpoint, or close the session if an error occurs.
  //! @see sendMessage
  //! @param session The session that the message was sent on (this reference
  //!                is only valid for the duration of the callback)
  //! @param sentBySessionInitiator Whether the message was sent by the
  //! initiator of the session
  //! @return true if the message was accepted for processing
  virtual bool onMessageReceived(pw::UniquePtr<std::byte[]> &&data,
                                 uint32_t messageType,
                                 uint32_t messagePermissions,
                                 const Session &session,
                                 bool sentBySessionInitiator) = 0;

  //! Callback called when a session has been requested to be opened. The
  //! message hub should call onSessionOpenComplete or closeSession to
  //! accept or reject the session, respectively.
  //! This function is called before returning from openSession in the
  //! requestor hub.
  virtual void onSessionOpenRequest(const Session &session) = 0;

  //! Callback called when the peer message hub has accepted the session
  //! and the session is now open for messages
  virtual void onSessionOpened(const Session &session) = 0;

  //! Callback called when the session is closed
  virtual void onSessionClosed(const Session &session, Reason reason) = 0;

  //! Callback called to iterate over all endpoints connected to the
  //! MessageHub. Underlying endpoint storage must not change during this
  //! callback. If function returns true, the MessageHub can stop iterating
  //! over future endpoints.
  virtual void forEachEndpoint(
      const pw::Function<bool(const EndpointInfo &)> &function) = 0;

  //! @return The EndpointInfo for the given endpoint ID.
  virtual std::optional<EndpointInfo> getEndpointInfo(
      EndpointId endpointId) = 0;

  //! @return The first endpoint that has the given service descriptor, a
  //! null-terminated ASCII string. If no endpoint has the service descriptor,
  //! std::nullopt is returned.
  virtual std::optional<EndpointId> getEndpointForService(
      const char *serviceDescriptor) = 0;

  //! @return true if the endpoint has the given service descriptor, a
  //! null-terminated ASCII string, false otherwise.
  virtual bool doesEndpointHaveService(EndpointId endpointId,
                                       const char *serviceDescriptor) = 0;

  //! Callback called to iterate over all services provided by endpoints
  //! connected to the MessageHub. Underlying endpoint and service storage
  //! must not change during this callback. If function returns true, the
  //! MessageHub can stop iterating over future endpoints. The service
  //! descriptor must be valid for the duration of the callback.
  virtual void forEachService(
      const pw::Function<bool(const EndpointInfo &, const ServiceInfo &)>
          &function) = 0;

  //! Callback called when a message hub except this one is registered.
  virtual void onHubRegistered(const MessageHubInfo &info) = 0;

  //! Callback called when a message hub except this one is unregistered.
  virtual void onHubUnregistered(MessageHubId id) = 0;

  //! Callback called when an endpoint is registered to any MessageHub,
  //! except for this MessageHub.
  virtual void onEndpointRegistered(MessageHubId messageHubId,
                                    EndpointId endpointId) = 0;

  //! Callback called when an endpoint is unregistered from any MessageHub,
  //! except for this MessageHub.
  virtual void onEndpointUnregistered(MessageHubId messageHubId,
                                      EndpointId endpointId) = 0;

  //! Recycle function called by pw::IntrusivePtr when the MessageHubCallback
  //! is no longer in use. The default behavior in Pigweed is to `delete

  //! this`. The callbacks derived from this class should also inherit from
  //! pw::Recyclable and override this function.
  virtual void pw_recycle() = 0;
};

}  // namespace chre::message

#endif  // CHRE_UTIL_SYSTEM_MESSAGE_HUB_CALLBACK_H_
