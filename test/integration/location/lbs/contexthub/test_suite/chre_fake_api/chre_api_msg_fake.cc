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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_msg_fake.h"

#include <cstddef>
#include <cstdint>

#include "chre/util/macros.h"
#include "chre_api/chre/msg.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

DLL_EXPORT bool chreMsgGetEndpointInfo(uint64_t hubId, uint64_t endpointId,
                                       struct chreMsgEndpointInfo* info) {
  return lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiMsgFunctions()
      ->GetEndpointInfo(hubId, endpointId, info);
}

DLL_EXPORT bool chreMsgConfigureEndpointReadyEvents(uint64_t hubId,
                                                    uint64_t endpointId,
                                                    bool enable) {
  return lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiMsgFunctions()
      ->ConfigureEndpointReadyEvents(hubId, endpointId, enable);
}

DLL_EXPORT bool chreMsgConfigureServiceReadyEvents(
    uint64_t hubId, const char* serviceDescriptor, bool enable) {
  return lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiMsgFunctions()
      ->ConfigureServiceReadyEvents(hubId, serviceDescriptor, enable);
}

DLL_EXPORT bool chreMsgSessionGetInfo(uint16_t sessionId,
                                      struct chreMsgSessionInfo* info) {
  return lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiMsgFunctions()
      ->SessionGetInfo(sessionId, info);
}

DLL_EXPORT bool chreMsgPublishServices(const struct chreMsgServiceInfo* services,
                                       size_t numServices) {
  return lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiMsgFunctions()
      ->PublishServices(services, numServices);
}

DLL_EXPORT bool chreMsgSessionOpenAsync(uint64_t hubId, uint64_t endpointId,
                                        const char* serviceDescriptor) {
  return lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiMsgFunctions()
      ->SessionOpenAsync(hubId, endpointId, serviceDescriptor);
}

DLL_EXPORT bool chreMsgSessionCloseAsync(uint16_t sessionId) {
  return lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiMsgFunctions()
      ->SessionCloseAsync(sessionId);
}

DLL_EXPORT bool chreMsgSend(void* message, size_t messageSize,
                            uint32_t messageType, uint16_t sessionId,
                            uint32_t messagePermissions,
                            chreMessageFreeFunction* freeCallback) {
  return lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiMsgFunctions()
      ->Send(message, messageSize, messageType, sessionId, messagePermissions,
             freeCallback);
}

namespace lbs::contexthub {

// TODO(b/486958972): Implement functions for linux simulator.
bool ChreApiMsgFunctionsImpl::GetEndpointInfo(
    uint64_t /*hubId*/, uint64_t /*endpointId*/,
    chreMsgEndpointInfo* /*info*/) {
  return false;
}

bool ChreApiMsgFunctionsImpl::ConfigureEndpointReadyEvents(
    uint64_t /*hubId*/, uint64_t /*endpointId*/, bool /*enable*/) {
  return false;
}

bool ChreApiMsgFunctionsImpl::ConfigureServiceReadyEvents(
    uint64_t /*hubId*/, const char* /*serviceDescriptor*/, bool /*enable*/) {
  return false;
}

bool ChreApiMsgFunctionsImpl::SessionGetInfo(
    uint16_t /*sessionId*/, chreMsgSessionInfo* /*info*/) {
  return false;
}

bool ChreApiMsgFunctionsImpl::PublishServices(
    const chreMsgServiceInfo* /*services*/, size_t /*numServices*/) {
  return false;
}

bool ChreApiMsgFunctionsImpl::SessionOpenAsync(
    uint64_t /*hubId*/, uint64_t /*endpointId*/,
    const char* /*serviceDescriptor*/) {
  return false;
}

bool ChreApiMsgFunctionsImpl::SessionCloseAsync(uint16_t /*sessionId*/) {
  return false;
}

bool ChreApiMsgFunctionsImpl::Send(void* /*message*/, size_t /*messageSize*/,
                                   uint32_t /*messageType*/,
                                   uint16_t /*sessionId*/,
                                   uint32_t /*messagePermissions*/,
                                   chreMessageFreeFunction* /*freeCallback*/) {
  return false;
}

ChreApiMsgFunctions::ChreApiMsgFunctions() {
  GetEndpointInfo = ChreApiMsgFunctionsImpl::GetEndpointInfo;
  ConfigureEndpointReadyEvents =
      ChreApiMsgFunctionsImpl::ConfigureEndpointReadyEvents;
  ConfigureServiceReadyEvents =
      ChreApiMsgFunctionsImpl::ConfigureServiceReadyEvents;
  SessionGetInfo = ChreApiMsgFunctionsImpl::SessionGetInfo;
  PublishServices = ChreApiMsgFunctionsImpl::PublishServices;
  SessionOpenAsync = ChreApiMsgFunctionsImpl::SessionOpenAsync;
  SessionCloseAsync = ChreApiMsgFunctionsImpl::SessionCloseAsync;
  Send = ChreApiMsgFunctionsImpl::Send;
}

}  // namespace lbs::contexthub
