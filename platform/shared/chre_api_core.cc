/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "chre_api/chre/event.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "chre/core/event_loop_manager.h"
#include "chre/core/host_comms_manager.h"
#include "chre/core/host_endpoint_manager.h"
#include "chre/platform/context.h"
#include "chre/platform/log.h"
#include "chre/util/macros.h"
#include "chre/util/system/napp_permissions.h"
#include "chre/util/unique_ptr.h"

using chre::Event;
using chre::EventLoop;
using chre::EventLoopManager;
using chre::EventLoopManagerSingleton;
using chre::getCurrentEventLoop;
using chre::GlobalApiLockGuard;
using chre::HostCommsManager;
using chre::MakeUnique;
using chre::Nanoapp;
using chre::UniquePtr;

namespace {

/**
 * Sends a message to the host.
 *
 * @param nanoapp The nanoapp sending the message.
 * @param message A pointer to the message buffer.
 * @param messageSize The size of the message.
 * @param hostEndpoint The host endpoint to send the message to.
 * @param messagePermissions Bitmasked CHRE_MESSAGE_PERMISSION_...
 * @param freeCallback The callback that will be invoked to free the message
 *        buffer.
 * @param isReliable Whether to send a reliable message.
 * @param cookie The cookie used when reporting reliable message status. It is
 *        only used for reliable messages.
 * @return Whether the message was accepted for transmission.
 */
bool sendMessageToHost(Nanoapp *nanoapp, void *message, size_t messageSize,
                       uint32_t messageType, uint16_t hostEndpoint,
                       uint32_t messagePermissions,
                       chreMessageFreeFunction *freeCallback, bool isReliable,
                       const void *cookie) {
  const EventLoop &eventLoop = *chre::getCurrentEventLoop();
  bool success = false;
  if (eventLoop.currentNanoappIsStopping()) {
    LOGW("Rejecting message to host from app instance %" PRIu16
         " because it's stopping",
         nanoapp->getInstanceId());
  } else {
    // Note that we place the GlobalApiLockGuard here to avoid holding
    // the lock when freeCallback is invoked.
    GlobalApiLockGuard lock;
    HostCommsManager &hostCommsManager =
        EventLoopManagerSingleton::get()->getHostCommsManager();
    success = hostCommsManager.sendMessageToHostFromNanoapp(
        nanoapp, message, messageSize, messageType, hostEndpoint,
        messagePermissions, freeCallback, isReliable, cookie);
  }

  if (!success && freeCallback != nullptr) {
    freeCallback(message, messageSize);
  }

  return success;
}

}  // namespace

DLL_EXPORT bool chreSendEvent(uint16_t eventType, void *eventData,
                              chreEventCompleteFunction *freeCallback,
                              uint32_t targetInstanceId) {
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);

  // Prevent an app that is in the process of being unloaded from generating new
  // events
  bool success = false;
  EventLoop *eventLoop = getCurrentEventLoop();
  CHRE_ASSERT(eventLoop != nullptr);
  CHRE_ASSERT_LOG(targetInstanceId <= UINT16_MAX,
                  "Invalid instance ID %" PRIu32 " provided", targetInstanceId);
  if (eventLoop->currentNanoappIsStopping()) {
    LOGW("Rejecting event from app instance %" PRIu16 " because it's stopping",
         nanoapp->getInstanceId());
  } else if (targetInstanceId <= UINT16_MAX) {
    // Note that we do not hold the global mutex here, because
    // postLowPriorityEventOrFree is already thread-safe.
    success = EventLoopManagerSingleton::get()->postLowPriorityEventOrFree(
        eventType, eventData, freeCallback, nanoapp->getInstanceId(),
        static_cast<uint16_t>(targetInstanceId));
  }
  return success;
}

DLL_EXPORT bool chreSendMessageToHost(void *message, uint32_t messageSize,
                                      uint32_t messageType,
                                      chreMessageFreeFunction *freeCallback) {
  return chreSendMessageToHostEndpoint(
      message, static_cast<size_t>(messageSize), messageType,
      CHRE_HOST_ENDPOINT_BROADCAST, freeCallback);
}

DLL_EXPORT bool chreSendMessageWithPermissions(
    void *message, size_t messageSize, uint32_t messageType,
    uint16_t hostEndpoint, uint32_t messagePermissions,
    chreMessageFreeFunction *freeCallback) {
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return sendMessageToHost(nanoapp, message, messageSize, messageType,
                           hostEndpoint, messagePermissions, freeCallback,
                           /*isReliable=*/false, /*cookie=*/nullptr);
}

DLL_EXPORT bool chreSendReliableMessageAsync(
    void *message, size_t messageSize, uint32_t messageType,
    uint16_t hostEndpoint, uint32_t messagePermissions,
    chreMessageFreeFunction *freeCallback, const void *cookie) {
#ifdef CHRE_RELIABLE_MESSAGE_SUPPORT_ENABLED
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return sendMessageToHost(nanoapp, message, messageSize, messageType,
                           hostEndpoint, messagePermissions, freeCallback,
                           /*isReliable=*/true, cookie);
#else
  UNUSED_VAR(messageType);
  UNUSED_VAR(hostEndpoint);
  UNUSED_VAR(messagePermissions);
  UNUSED_VAR(cookie);
  if (freeCallback != nullptr) {
    freeCallback(message, messageSize);
  }
  return false;
#endif  // CHRE_RELIABLE_MESSAGE_SUPPORT_ENABLED
}

DLL_EXPORT bool chreSendMessageToHostEndpoint(
    void *message, size_t messageSize, uint32_t messageType,
    uint16_t hostEndpoint, chreMessageFreeFunction *freeCallback) {
  return chreSendMessageWithPermissions(
      message, messageSize, messageType, hostEndpoint,
      static_cast<uint32_t>(chre::NanoappPermissions::CHRE_PERMS_NONE),
      freeCallback);
}

DLL_EXPORT bool chreGetNanoappInfoByAppId(uint64_t appId,
                                          struct chreNanoappInfo *info) {
  EventLoop *eventLoop =
      EventLoopManagerSingleton::get()->getEventLoopByAppId(appId);
  if (eventLoop == nullptr) {
    return false;
  } else {
    // Note that populateNanoappInfoForAppId is thread-safe.
    return eventLoop->populateNanoappInfoForAppId(appId, info);
  }
}

DLL_EXPORT bool chreGetNanoappInfoByInstanceId(uint32_t instanceId,
                                               struct chreNanoappInfo *info) {
  CHRE_ASSERT(instanceId <= UINT16_MAX);
  if (instanceId <= UINT16_MAX) {
    uint16_t instanceIdUint16 = static_cast<uint16_t>(instanceId);
    EventLoop *eventLoop =
        EventLoopManagerSingleton::get()->getEventLoopByInstanceId(
            instanceIdUint16);
    if (eventLoop == nullptr) {
      return false;
    } else {
      // Note that populateNanoappInfoForInstanceId is thread-safe.
      return eventLoop->populateNanoappInfoForInstanceId(instanceIdUint16,
                                                         info);
    }
  }
  return false;
}

DLL_EXPORT void chreConfigureNanoappInfoEvents(bool enable) {
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  nanoapp->configureNanoappInfoEvents(enable);
}

DLL_EXPORT void chreConfigureHostSleepStateEvents(bool enable) {
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  nanoapp->configureHostSleepEvents(enable);
}

DLL_EXPORT bool chreIsHostAwake() {
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()
      ->getPowerControlManager()
      .hostIsAwake();
}

DLL_EXPORT void chreConfigureDebugDumpEvent(bool enable) {
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  nanoapp->configureDebugDumpEvent(enable);
}

DLL_EXPORT bool chreConfigureHostEndpointNotifications(uint16_t hostEndpointId,
                                                       bool enable) {
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return nanoapp->configureHostEndpointNotifications(hostEndpointId, enable);
}

DLL_EXPORT bool chrePublishRpcServices(struct chreNanoappRpcService *services,
                                       size_t numServices) {
  Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  GlobalApiLockGuard lock;
  return nanoapp->publishRpcServices(services, numServices);
}

DLL_EXPORT bool chreGetHostEndpointInfo(uint16_t hostEndpointId,
                                        struct chreHostEndpointInfo *info) {
  GlobalApiLockGuard lock;
  return EventLoopManagerSingleton::get()
      ->getHostEndpointManager()
      .getHostEndpointInfo(hostEndpointId, info);
}
