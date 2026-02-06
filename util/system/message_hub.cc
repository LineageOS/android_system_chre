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

#include "chre/util/system/message_common.h"
#include "chre/util/system/message_router.h"

#include <cstring>
#include <optional>
#include <utility>

namespace chre::message {

MessageHub::MessageHub() : mRouter(nullptr), mHubId(MESSAGE_HUB_ID_INVALID) {}

MessageHub::MessageHub(MessageRouter &router, MessageHubId id)
    : mRouter(&router), mHubId(id) {}

MessageHub::MessageHub(MessageHub &&other)
    : mRouter(other.mRouter), mHubId(other.mHubId) {
  other.mRouter = nullptr;
  other.mHubId = MESSAGE_HUB_ID_INVALID;
}

MessageHub &MessageHub::operator=(MessageHub &&other) {
  unregister();
  mRouter = other.mRouter;
  mHubId = other.mHubId;
  other.mRouter = nullptr;
  other.mHubId = MESSAGE_HUB_ID_INVALID;
  return *this;
}

MessageHub::~MessageHub() {
  unregister();
}

void MessageHub::onSessionOpenComplete(SessionId sessionId) {
  if (mRouter != nullptr) {
    mRouter->onSessionOpenComplete(mHubId, sessionId);
  }
}

SessionId MessageHub::openSession(EndpointId fromEndpointId,
                                  MessageHubId toMessageHubId,
                                  EndpointId toEndpointId,
                                  const char *serviceDescriptor,
                                  SessionId sessionId) {
  return mRouter == nullptr
             ? SESSION_ID_INVALID
             : mRouter->openSession(mHubId, fromEndpointId, toMessageHubId,
                                    toEndpointId, serviceDescriptor, sessionId);
}

bool MessageHub::closeSession(SessionId sessionId, Reason reason) {
  return mRouter != nullptr && mRouter->closeSession(mHubId, sessionId, reason);
}

std::optional<Session> MessageHub::getSessionWithId(SessionId sessionId) {
  return mRouter == nullptr ? std::nullopt
                            : mRouter->getSessionWithId(mHubId, sessionId);
}

bool MessageHub::sendMessage(pw::UniquePtr<std::byte[]> &&data,
                             uint32_t messageType, uint32_t messagePermissions,
                             SessionId sessionId, EndpointId fromEndpointId) {
  return mRouter != nullptr &&
         mRouter->sendMessage(std::move(data), messageType, messagePermissions,
                              sessionId, fromEndpointId, mHubId);
}

bool MessageHub::registerEndpoint(EndpointId endpointId) {
  return mRouter != nullptr && mRouter->registerEndpoint(mHubId, endpointId);
}

bool MessageHub::unregisterEndpoint(EndpointId endpointId) {
  return mRouter != nullptr && mRouter->unregisterEndpoint(mHubId, endpointId);
}

bool MessageHub::registerDataFlowSink(
    const DataFlowSinkRegistration &registration) {
  return mRouter != nullptr && mRouter->registerDataFlowSink(registration);
}

void MessageHub::reportDataFlowSinkUnregistered(
    const DataFlowSinkUnregistration &unregistration) {
  if (mRouter != nullptr) {
    mRouter->reportDataFlowSinkUnregistered(unregistration);
  }
}

void MessageHub::reportDataFlowStopped(const DataFlowStopped &stopped) {
  if (mRouter != nullptr) {
    mRouter->reportDataFlowStopped(stopped);
  }
}

void MessageHub::reportDataFlowAlert(const DataFlowAlert &alert) {
  if (mRouter != nullptr) {
    mRouter->reportDataFlowAlert(alert);
  }
}

MessageHubId MessageHub::getId() {
  return mHubId;
}

bool MessageHub::isRegistered() {
  return mRouter != nullptr;
}

void MessageHub::unregister() {
  if (mRouter != nullptr) {
    mRouter->unregisterMessageHub(mHubId);
  }
  mRouter = nullptr;
}

}  // namespace chre::message
