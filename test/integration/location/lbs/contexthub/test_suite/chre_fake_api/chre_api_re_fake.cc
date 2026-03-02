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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_re_fake.h"

#include <cstdarg>
#include <cstdint>
#include <cstdlib>

#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/assert.h"
#include "chre/platform/memory.h"
#include "chre/platform/system_time.h"
#include "chre/util/macros.h"
#include "chre_api/chre/re.h"
#include "chre_api/chre/version.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

using chre::EventLoopManager;
using chre::EventLoopManagerSingleton;
using lbs::contexthub::FakeChreApiProvider;

// Export API functions that can be faked using FakeChreReApi
DLL_EXPORT void chreAbort(uint32_t abortCode) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreAbort(abortCode);
  FakeChreApiProvider::GetInstance()->GetChreApiReFunctions()->Abort(abortCode);
}

DLL_EXPORT uint32_t chreGetCapabilities() {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreGetCapabilities();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->GetCapabilities();
}

DLL_EXPORT uint32_t chreGetMessageToHostMaxSize() {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGetMessageToHostMaxSize();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->GetMessageToHostMaxSize();
}

DLL_EXPORT uint64_t chreGetTime() {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreGetTime();
  return FakeChreApiProvider::GetInstance()->GetChreApiReFunctions()->GetTime();
}

DLL_EXPORT int64_t chreGetEstimatedHostTimeOffset() {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreGetEstimatedHostTimeOffset();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->GetEstimatedHostTimeOffset();
}

DLL_EXPORT uint64_t chreGetAppId(void) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreGetAppId();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->GetAppId();
}

DLL_EXPORT uint32_t chreGetInstanceId(void) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreGetInstanceId();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->GetInstanceId();
}

DLL_EXPORT uint32_t chreTimerSet(uint64_t duration, const void *cookie,
                                 bool one_shot) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreTimerSet(
      /*duration=*/duration, /*cookie=*/cookie, /*oneShot=*/one_shot);
  return FakeChreApiProvider::GetInstance()->GetChreApiReFunctions()->TimerSet(
      duration, cookie, one_shot);
}

DLL_EXPORT bool chreTimerCancel(uint32_t timer_id) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreTimerCancel(
      /*timerId=*/timer_id);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->TimerCancel(timer_id);
}

// chreHeapAlloc and chreHeapFree should not be overridden, as it they can lead
// to test failures during test setup, such as in b/134640303.
DLL_EXPORT void *chreHeapAlloc(uint32_t bytes) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreHeapAlloc(
      /*bytes=*/bytes);
  return malloc(bytes);
}

DLL_EXPORT void chreHeapFree(void *ptr) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreHeapFree(
      /*ptr=*/ptr);
  free(ptr);
}

DLL_EXPORT void chreDebugDumpLog(const char *format_str, ...) {
  va_list arg_list;
  va_start(arg_list, format_str);
  va_list arg_copy;
  va_copy(arg_copy, arg_list);

  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreDebugDumpLog(
      format_str, arg_list);
  va_end(arg_list);

  vprintf(format_str, arg_copy);
  va_end(arg_copy);
}

DLL_EXPORT uint32_t chreGetApiVersion() {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreGetApiVersion();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->GetApiVersion();
}

DLL_EXPORT uint32_t chreGetVersion() {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreGetVersion();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->GetVersion();
}

DLL_EXPORT uint64_t chreGetPlatformId() {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreGetPlatformId();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->GetPlatformId();
}

namespace lbs {
namespace contexthub {

// Create the functions that perform what would actually be run in the linux
// simulator.

void ChreApiReFunctions::Abort(uint32_t /*abortCode*/) {
  abort();
}

uint32_t ChreApiReFunctions::GetCapabilities() {
  return CHRE_CAPABILITIES_RELIABLE_MESSAGES |
         CHRE_CAPABILITIES_GENERIC_ENDPOINT_MESSAGES;
}

uint32_t ChreApiReFunctions::GetMessageToHostMaxSize() {
  if (message_to_host_max_size_override_.has_value()) {
    return message_to_host_max_size_override_.value();
  }

// TODO(b/335636886): Allow for increased message size without enabling
// reliable messaging.
#ifdef CHRE_RELIABLE_MESSAGE_SUPPORT_ENABLED

#ifndef CHRE_LARGE_PAYLOAD_MAX_SIZE
  static_assert(false,
                "CHRE_LARGE_PAYLOAD_MAX_SIZE must be defined if "
                "CHRE_RELIABLE_MESSAGE_SUPPORT_ENABLED is enabled");
#else
  static_assert(CHRE_LARGE_PAYLOAD_MAX_SIZE >= CHRE_MESSAGE_TO_HOST_MAX_SIZE,
                "CHRE_LARGE_PAYLOAD_MAX_SIZE must be greater than or equal to "
                "CHRE_MESSAGE_TO_HOST_MAX_SIZE");

  static_assert(CHRE_LARGE_PAYLOAD_MAX_SIZE >= 32000,
                "CHRE_LARGE_PAYLOAD_MAX_SIZE must be greater than or equal to "
                "32000 when CHRE_RELIABLE_MESSAGE_SUPPORT_ENABLED is enabled");
  return CHRE_LARGE_PAYLOAD_MAX_SIZE;
#endif  // CHRE_LARGE_PAYLOAD_MAX_SIZE

#else
  return CHRE_MESSAGE_TO_HOST_MAX_SIZE;
#endif  // CHRE_RELIABLE_MESSAGE_SUPPORT_ENABLED
}

uint64_t ChreApiReFunctions::GetTime() {
  return chre::SystemTime::getMonotonicTime().toRawNanoseconds();
}

int64_t ChreApiReFunctions::GetEstimatedHostTimeOffset() {
  return chre::SystemTime::getEstimatedHostTimeOffset();
}

uint64_t ChreApiReFunctions::GetAppId() {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return nanoapp->getAppId();
}

uint32_t ChreApiReFunctions::GetInstanceId() {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return nanoapp->getInstanceId();
}

uint32_t ChreApiReFunctions::TimerSet(uint64_t duration, const void *cookie,
                                      bool one_shot) {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()->getTimerPool().setNanoappTimer(
      nanoapp, chre::Nanoseconds(duration), cookie, one_shot);
}

bool ChreApiReFunctions::TimerCancel(uint32_t timer_id) {
  chre::Nanoapp *nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()->getTimerPool().cancelNanoappTimer(
      nanoapp, timer_id);
}

uint32_t ChreApiReFunctions::GetApiVersion() {
  return CHRE_API_VERSION;
}

uint32_t ChreApiReFunctions::GetVersion() {
  // API version is in upper two bytes, lower two bytes is platform-defined
  return GetApiVersion() | 0x1337;
}

uint64_t ChreApiReFunctions::GetPlatformId() {
  // Use the same platform ID as the Linux simulator by default
  // (go/nanoapp-id-tracker)
  constexpr uint64_t kSimulatorInstanceId = 0x476f6f676c000001;
  return kSimulatorInstanceId;
}

void ChreApiReFunctions::SetMessageToHostMaxSize(
    uint32_t messageToHostMaxSize) {
  message_to_host_max_size_override_ = messageToHostMaxSize;
}

}  // namespace contexthub
}  // namespace lbs
