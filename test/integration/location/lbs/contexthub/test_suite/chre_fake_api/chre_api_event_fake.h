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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_EVENT_FAKE_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_EVENT_FAKE_H_

#include <cstddef>
#include <cstdint>
#include <functional>

#include "chre_api/chre/event.h"

namespace lbs::contexthub {

using std::function;

using MessageFreeFunction = chreMessageFreeFunction;
using NanoappInfo = chreNanoappInfo;
using HostEndpointInfo = chreHostEndpointInfo;

// This class can be used in conjunction with
// FakeChreApiProvider::SetChreApiFunctions to overwrite what the CHRE event API
// functions return in unit tests and when run on the CHRE simulator.
class ChreApiEventFunctions {
 public:
  ChreApiEventFunctions();
  virtual ~ChreApiEventFunctions() = default;

  // These APIs are defined at
  // chre_api/include/chre_api/chre/event.h
  // and
  // chre_api/include/chre_api/chre/user_settings.h
  function<bool(uint16_t, void *, chreEventCompleteFunction *, uint32_t)>
      SendEvent;
  function<bool(void *, uint32_t, uint32_t, MessageFreeFunction *)>
      SendMessageToHost;
  function<bool(void *, size_t, uint32_t, uint16_t, MessageFreeFunction *)>
      SendMessageToHostEndpoint;
  function<bool(void *, size_t, uint32_t, uint16_t, uint32_t,
                MessageFreeFunction *)>
      SendMessageWithPermissions;
  function<bool(uint64_t, NanoappInfo *)> GetNanoappInfoByAppId;
  function<bool(uint32_t, NanoappInfo *)> GetNanoappInfoByInstanceId;
  function<void(bool)> ConfigureNanoappInfoEvents;
  function<void(bool)> ConfigureHostSleepStateEvents;
  function<bool()> IsHostAwake;
  function<void(bool)> ConfigureDebugDumpEvent;
  function<int8_t(int8_t)> UserSettingGetState;
  function<void(uint8_t, bool)> UserSettingConfigureEvents;
  function<bool(uint16_t, bool)> ConfigureHostEndpointNotifications;
  function<bool(uint16_t, HostEndpointInfo *)> GetHostEndpointInfo;
};

// This class is used to provide the implementation details for the above class.
// Classes can inherit and override any of its functions to provide new
// implementations for the APIs.
class ChreApiEventFunctionsImpl {
 public:
  static bool SendEvent(uint16_t eventType, void *eventData,
                        chreEventCompleteFunction *freeCallback,
                        uint32_t targetInstanceId);
  static bool SendMessageToHost(void *message, uint32_t messageSize,
                                uint32_t messageType,
                                MessageFreeFunction *freeCallback);
  static bool SendMessageToHostEndpoint(void *message, size_t messageSize,
                                        uint32_t messageType,
                                        uint16_t hostEndpoint,
                                        MessageFreeFunction *freeCallback);
  static bool SendMessageWithPermissions(void *message, size_t messageSize,
                                         uint32_t messageType,
                                         uint16_t hostEndpoint,
                                         uint32_t messagePermissions,
                                         chreMessageFreeFunction *freeCallback);
  static bool GetNanoappInfoByAppId(uint64_t appId, NanoappInfo *info);
  static bool GetNanoappInfoByInstanceId(uint32_t instanceId,
                                         NanoappInfo *info);
  static void ConfigureNanoappInfoEvents(bool enable);
  static void ConfigureHostSleepStateEvents(bool enable);
  static bool IsHostAwake();
  static void ConfigureDebugDumpEvent(bool enable);
  static int8_t UserSettingGetState(uint8_t);
  static void UserSettingConfigureEvents(uint8_t, bool);
  static bool ConfigureHostEndpointNotifications(uint16_t, bool);
  static bool GetHostEndpointInfo(uint16_t hostEndpointId,
                                  HostEndpointInfo *info);
};

}  // namespace lbs::contexthub

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_EVENT_FAKE_H_
