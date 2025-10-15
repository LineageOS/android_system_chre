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

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/event_helpers.h"

#include <cstddef>
#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_event_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/helpers_internal.h"

namespace lbs::contexthub::helpers {

// SetEmptyEvent class & function
// SendMessageX should actually call the freeCallback function and return true
// because there might be messages being sent during shutdown, and the callback
// is required to avoid memory leaks.
class ChreEventEmptyClass : public ChreApiEventFunctionsImpl {
 public:
  static bool SendMessageToHost(void *message, uint32_t messageSize,
                                uint32_t /* messageType */,
                                MessageFreeFunction *freeCallback) {
    if (freeCallback != nullptr) {
      freeCallback(message, messageSize);
    }
    return true;
  }

  static bool SendMessageToHostEndpoint(void *message, size_t messageSize,
                                        uint32_t /* messageType */,
                                        uint16_t /* hostEndpoint */,
                                        MessageFreeFunction *freeCallback) {
    if (freeCallback != nullptr) {
      freeCallback(message, messageSize);
    }
    return true;
  }

  static bool SendMessageWithPermissions(void *message, size_t messageSize,
                                         uint32_t /* messageType */,
                                         uint16_t /* hostEndpoint */,
                                         uint32_t /* messagePermissions */,
                                         MessageFreeFunction *freeCallback) {
    if (freeCallback != nullptr) {
      freeCallback(message, messageSize);
    }
    return true;
  }

  static bool GetNanoappInfoByAppId(uint64_t /* appId */,
                                    NanoappInfo * /* info */) {
    return false;
  }

  static bool GetNanoappInfoByInstanceId(uint32_t /* instanceId */,
                                         NanoappInfo * /* info */) {
    return false;
  }

  static void ConfigureNanoappInfoEvents(bool /* enable */) {}

  static void ConfigureHostSleepStateEvents(bool /* enable */) {}

  static bool IsHostAwake() {
    return false;
  }

  static bool ConfigureHostEndpointNotifications(uint16_t /* hostEndpointId */,
                                                 bool /* enable */) {
    return true;
  }

  static bool GetHostEndpointInfo(uint16_t /* hostEndpointId */,
                                  HostEndpointInfo * /* info */) {
    return false;
  }
};

void SetEmptyEvent() {
  internal::ExtendHelperMethods<ChreEventEmptyClass>(
      ::lbs::contexthub::FakeChreApiProvider::GetInstance()
          ->GetChreApiEventFunctions());
}

// SetEventHostIsAwake class & function
class ChreEventHostIsAwake : public ChreApiEventFunctionsImpl {
 public:
  static bool IsHostAwake() {
    return true;
  }
};

void SetEventHostIsAwake() {
  internal::ExtendHelperMethods<ChreEventHostIsAwake>(
      ::lbs::contexthub::FakeChreApiProvider::GetInstance()
          ->GetChreApiEventFunctions());
}

// SetEventHostIsAsleep class & function
class ChreEventHostIsAsleep : public ChreApiEventFunctionsImpl {
 public:
  static bool IsHostAwake() {
    return false;
  }
};

void SetEventHostIsAsleep() {
  internal::ExtendHelperMethods<ChreEventHostIsAsleep>(
      ::lbs::contexthub::FakeChreApiProvider::GetInstance()
          ->GetChreApiEventFunctions());
}

class ChreEventGetNanoappInfoByAppId : public ChreApiEventFunctionsImpl {
 public:
  static bool GetNanoappInfoByAppId(uint64_t /* appId */, NanoappInfo *info) {
    info->instanceId = INSTANCE_ID;
    return true;
  }
};

void SetEventGetNanoappInfoByAppId() {
  internal::ExtendHelperMethods<ChreEventGetNanoappInfoByAppId>(
      ::lbs::contexthub::FakeChreApiProvider::GetInstance()
          ->GetChreApiEventFunctions());
}

void SetEventSettingState(absl::flat_hash_map<uint8_t, int8_t> overrides) {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiEventFunctions()
      ->UserSettingGetState = [overrides](uint8_t setting) {
    if (overrides.contains(setting)) {
      return overrides.at(setting);
    }
    return ChreApiEventFunctionsImpl::UserSettingGetState(setting);
  };
}

}  // namespace lbs::contexthub::helpers