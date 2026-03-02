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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_event_fake.h"

#include <cinttypes>
#include <cstdint>
#include <cstdlib>

#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/core/settings.h"
#include "chre/target_platform/log.h"
#include "chre/util/macros.h"
#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

using chre::EventLoopManager;
using chre::EventLoopManagerSingleton;
using lbs::contexthub::FakeChreApiProvider;
using lbs::contexthub::HostEndpointInfo;
using lbs::contexthub::MessageFreeFunction;
using lbs::contexthub::NanoappInfo;

// Export API functions that can be faked using FakeChreEventApi
DLL_EXPORT bool chreSendEvent(uint16_t eventType, void *eventData,
                              chreEventCompleteFunction *freeCallback,
                              uint32_t targetInstanceId) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreSendEvent(
      eventType, eventData, freeCallback, targetInstanceId);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->SendEvent(eventType, eventData, freeCallback, targetInstanceId);
}

DLL_EXPORT bool chreSendMessageToHost(void *message, uint32_t messageSize,
                                      uint32_t messageType,
                                      chreMessageFreeFunction *freeCallback) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreSendMessageToHost(
      message, messageSize, messageType, freeCallback);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->SendMessageToHost(message, messageSize, messageType, freeCallback);
}

DLL_EXPORT bool chreSendMessageWithPermissions(
    void *message, size_t messageSize, uint32_t messageType,
    uint16_t hostEndpoint, uint32_t messagePermissions,
    chreMessageFreeFunction *freeCallback) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreSendMessageWithPermissions(message, messageSize, messageType,
                                       hostEndpoint, messagePermissions,
                                       freeCallback);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->SendMessageWithPermissions(message, messageSize, messageType,
                                   hostEndpoint, messagePermissions,
                                   freeCallback);
}

DLL_EXPORT bool chreSendMessageToHostEndpoint(
    void *message, size_t messageSize, uint32_t messageType,
    uint16_t hostEndpoint, chreMessageFreeFunction *freeCallback) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreSendMessageToHostEndpoint(message, messageSize, messageType,
                                      hostEndpoint, freeCallback);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->SendMessageToHostEndpoint(message, messageSize, messageType,
                                  hostEndpoint, freeCallback);
}

DLL_EXPORT bool chreGetNanoappInfoByAppId(uint64_t appId,
                                          struct chreNanoappInfo *info) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGetNanoappInfoByAppId(appId, info);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->GetNanoappInfoByAppId(appId, info);
}

DLL_EXPORT bool chreGetNanoappInfoByInstanceId(uint32_t instanceId,
                                               struct chreNanoappInfo *info) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGetNanoappInfoByInstanceId(instanceId, info);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->GetNanoappInfoByInstanceId(instanceId, info);
}

DLL_EXPORT void chreConfigureNanoappInfoEvents(bool enable) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreConfigureNanoappInfoEvents(enable);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->ConfigureNanoappInfoEvents(enable);
}

DLL_EXPORT void chreConfigureHostSleepStateEvents(bool enable) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreConfigureHostSleepStateEvents(enable);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->ConfigureHostSleepStateEvents(enable);
}

DLL_EXPORT bool chreIsHostAwake(void) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreIsHostAwake();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->IsHostAwake();
}

DLL_EXPORT void chreConfigureDebugDumpEvent(bool enable) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreConfigureDebugDumpEvent(enable);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->ConfigureDebugDumpEvent(enable);
}

DLL_EXPORT int8_t chreUserSettingGetState(uint8_t setting) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreUserSettingGetState(setting);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->UserSettingGetState(setting);
}

DLL_EXPORT void chreUserSettingConfigureEvents(uint8_t setting, bool enable) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreUserSettingConfigureEvents(setting, enable);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->UserSettingConfigureEvents(setting, enable);
}

DLL_EXPORT bool chreConfigureHostEndpointNotifications(uint16_t hostEndpointId,
                                                       bool enable) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreConfigureHostEndpointNotifications(hostEndpointId, enable);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->ConfigureHostEndpointNotifications(hostEndpointId, enable);
}

DLL_EXPORT bool chreGetHostEndpointInfo(uint16_t hostEndpointId,
                                        struct chreHostEndpointInfo *info) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGetHostEndpointInfo(hostEndpointId, info);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->GetHostEndpointInfo(hostEndpointId, info);
}

namespace lbs::contexthub {

// Create the functions that perform what would actually be run in the linux
// simulator.
// static
bool ChreApiEventFunctionsImpl::SendEvent(
    uint16_t eventType, void *eventData,
    chreEventCompleteFunction *freeCallback, uint32_t targetInstanceId) {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);

  // Prevent an app that is in the process of being unloaded from generating new
  // events
  bool success = false;
  chre::EventLoop &eventLoop = EventLoopManagerSingleton::get()->getEventLoop();
  if (eventLoop.currentNanoappIsStopping()) {
    LOGW("Rejecting event from app instance %" PRIu32 " because it's stopping",
         nanoapp->getInstanceId());
  } else {
    success = eventLoop.postLowPriorityEventOrFree(
        eventType, eventData, freeCallback, nanoapp->getInstanceId(),
        targetInstanceId);
  }
  return success;
}

// static
bool ChreApiEventFunctionsImpl::SendMessageToHost(
    void *message, uint32_t messageSize, uint32_t messageType,
    MessageFreeFunction *freeCallback) {
  return chreSendMessageToHostEndpoint(
      message, static_cast<size_t>(messageSize), messageType,
      CHRE_HOST_ENDPOINT_BROADCAST, freeCallback);
}

// static
bool ChreApiEventFunctionsImpl::SendMessageToHostEndpoint(
    void *message, size_t messageSize, uint32_t messageType,
    uint16_t hostEndpoint, MessageFreeFunction *freeCallback) {
  return SendMessageWithPermissions(
      message, messageSize, messageType, hostEndpoint,
      static_cast<uint32_t>(chre::NanoappPermissions::CHRE_PERMS_NONE),
      freeCallback);
}

bool ChreApiEventFunctionsImpl::SendMessageWithPermissions(
    void *message, size_t messageSize, uint32_t messageType,
    uint16_t hostEndpoint, uint32_t messagePermissions,
    chreMessageFreeFunction *freeCallback) {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);

  bool success = false;
  const chre::EventLoop &eventLoop =
      EventLoopManagerSingleton::get()->getEventLoop();
  if (eventLoop.currentNanoappIsStopping()) {
    LOGW("Rejecting message to host from app instance %" PRIu32
         " because it's stopping",
         nanoapp->getInstanceId());
  } else {
    auto &hostCommsManager =
        EventLoopManagerSingleton::get()->getHostCommsManager();
    success = hostCommsManager.sendMessageToHostFromNanoapp(
        nanoapp, message, messageSize, messageType, hostEndpoint,
        messagePermissions, freeCallback, false /*isReliable=*/,
        nullptr /*cookie*/);
  }

  if (!success && freeCallback != nullptr) {
    freeCallback(message, messageSize);
  }

  return success;
}

// static
bool ChreApiEventFunctionsImpl::GetNanoappInfoByAppId(uint64_t appId,
                                                      NanoappInfo *info) {
  return EventLoopManagerSingleton::get()
      ->getEventLoop()
      .populateNanoappInfoForAppId(appId, info);
}

// static
bool ChreApiEventFunctionsImpl::GetNanoappInfoByInstanceId(uint32_t instanceId,
                                                           NanoappInfo *info) {
  return EventLoopManagerSingleton::get()
      ->getEventLoop()
      .populateNanoappInfoForInstanceId(instanceId, info);
}

// static
void ChreApiEventFunctionsImpl::ConfigureNanoappInfoEvents(bool enable) {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  nanoapp->configureNanoappInfoEvents(enable);
}

// static
void ChreApiEventFunctionsImpl::ConfigureHostSleepStateEvents(bool enable) {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  nanoapp->configureHostSleepEvents(enable);
}

// static
bool ChreApiEventFunctionsImpl::IsHostAwake() {
  return EventLoopManagerSingleton::get()
      ->getPowerControlManager()
      .hostIsAwake();
}

// static
void ChreApiEventFunctionsImpl::ConfigureDebugDumpEvent(bool enable) {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  nanoapp->configureDebugDumpEvent(enable);
}

// static
int8_t ChreApiEventFunctionsImpl::UserSettingGetState(uint8_t setting) {
  return chre::EventLoopManagerSingleton::get()
      ->getSettingManager()
      .getSettingStateAsInt8(setting);
}

// static
void ChreApiEventFunctionsImpl::UserSettingConfigureEvents(uint8_t setting,
                                                           bool enable) {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  nanoapp->configureUserSettingEvent(setting, enable);
}

// static
bool ChreApiEventFunctionsImpl::ConfigureHostEndpointNotifications(uint16_t id,
                                                                   bool en) {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return nanoapp->configureHostEndpointNotifications(id, en);
}

// static
bool ChreApiEventFunctionsImpl::GetHostEndpointInfo(uint16_t hostEndpointId,
                                                    HostEndpointInfo *info) {
  (void)hostEndpointId;
  (void)info;
  return false;
}

// set the default value of the function class's std::functions to that of impl.
ChreApiEventFunctions::ChreApiEventFunctions() {
  SendEvent = ChreApiEventFunctionsImpl::SendEvent;
  SendMessageToHost = ChreApiEventFunctionsImpl::SendMessageToHost;
  SendMessageToHostEndpoint =
      ChreApiEventFunctionsImpl::SendMessageToHostEndpoint;
  SendMessageWithPermissions =
      ChreApiEventFunctionsImpl::SendMessageWithPermissions;
  GetNanoappInfoByAppId = ChreApiEventFunctionsImpl::GetNanoappInfoByAppId;
  GetNanoappInfoByInstanceId =
      ChreApiEventFunctionsImpl::GetNanoappInfoByInstanceId;
  ConfigureNanoappInfoEvents =
      ChreApiEventFunctionsImpl::ConfigureNanoappInfoEvents;
  ConfigureHostSleepStateEvents =
      ChreApiEventFunctionsImpl::ConfigureHostSleepStateEvents;
  IsHostAwake = ChreApiEventFunctionsImpl::IsHostAwake;
  UserSettingGetState = ChreApiEventFunctionsImpl::UserSettingGetState;
  ConfigureDebugDumpEvent = ChreApiEventFunctionsImpl::ConfigureDebugDumpEvent;
  UserSettingConfigureEvents =
      ChreApiEventFunctionsImpl::UserSettingConfigureEvents;
  ConfigureHostEndpointNotifications =
      ChreApiEventFunctionsImpl::ConfigureHostEndpointNotifications;
  GetHostEndpointInfo = ChreApiEventFunctionsImpl::GetHostEndpointInfo;
}

}  // namespace lbs::contexthub
