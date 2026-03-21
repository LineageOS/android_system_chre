/*
 * Copyright (C) 2026 The Android Open Source Project
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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_MSG_FAKE_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_MSG_FAKE_H_

#include <cstddef>
#include <cstdint>
#include <functional>

#include "chre_api/chre/msg.h"

namespace lbs::contexthub {

// This class can be used in conjunction with
// FakeChreApiProvider::SetChreApiFunctions to overwrite what the CHRE msg API
// functions return in unit tests and when run on the CHRE simulator.
class ChreApiMsgFunctions {
 public:
  ChreApiMsgFunctions();
  virtual ~ChreApiMsgFunctions() = default;

  // These APIs are defined at
  // chre_api/include/chre_api/chre/msg.h
  std::function<bool(uint64_t, uint64_t, chreMsgEndpointInfo*)>
      GetEndpointInfo;
  std::function<bool(uint64_t, uint64_t, bool)>
      ConfigureEndpointReadyEvents;
  std::function<bool(uint64_t, const char*, bool)>
      ConfigureServiceReadyEvents;
  std::function<bool(uint16_t, chreMsgSessionInfo*)> SessionGetInfo;
  std::function<bool(const chreMsgServiceInfo*, size_t)> PublishServices;
  std::function<bool(uint64_t, uint64_t, const char*)> SessionOpenAsync;
  std::function<bool(uint16_t)> SessionCloseAsync;
  std::function<bool(void*, size_t, uint32_t, uint16_t, uint32_t,
                     chreMessageFreeFunction*)>
      Send;
};

// This class is used to provide the implementation details for the above class.
// Classes can inherit and override any of its functions to provide new
// implementations for the APIs.
class ChreApiMsgFunctionsImpl {
 public:
  static bool GetEndpointInfo(uint64_t hubId, uint64_t endpointId,
                                chreMsgEndpointInfo* info);
  static bool ConfigureEndpointReadyEvents(uint64_t hubId,
                                             uint64_t endpointId, bool enable);
  static bool ConfigureServiceReadyEvents(uint64_t hubId,
                                          const char* serviceDescriptor,
                                          bool enable);
  static bool SessionGetInfo(uint16_t sessionId,
                               chreMsgSessionInfo* info);
  static bool PublishServices(const chreMsgServiceInfo* services,
                                size_t numServices);
  static bool SessionOpenAsync(uint64_t hubId, uint64_t endpointId,
                                 const char* serviceDescriptor);
  static bool SessionCloseAsync(uint16_t sessionId);
  static bool Send(void* message, size_t messageSize, uint32_t messageType,
                   uint16_t sessionId, uint32_t messagePermissions,
                   chreMessageFreeFunction* freeCallback);
};

}  // namespace lbs::contexthub

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_MSG_FAKE_H_
