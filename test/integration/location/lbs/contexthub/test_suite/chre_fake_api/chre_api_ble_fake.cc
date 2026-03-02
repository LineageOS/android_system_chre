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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_ble_fake.h"

#include "absl/base/nullability.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/util/macros.h"
#include "chre_api/chre/ble.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_fake_provider.h"

using chre::EventLoopManager;
using chre::EventLoopManagerSingleton;
using lbs::contexthub::BleScanFilter;
using lbs::contexthub::BleScanMode;
using lbs::contexthub::FakeChreApiProvider;

// Export API functions that can be faked using FakeChreBleApi

DLL_EXPORT uint32_t chreBleGetCapabilities() {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreBleGetCapabilities();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->GetCapabilities();
}

DLL_EXPORT uint32_t chreBleGetFilterCapabilities() {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreBleGetFilterCapabilities();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->GetFilterCapabilities();
}

DLL_EXPORT bool chreBleStartScanAsync(enum chreBleScanMode mode,
                                      uint32_t reportDelayMs,
                                      const struct chreBleScanFilter* filter) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreBleStartScanAsync(
      mode, reportDelayMs, filter);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->StartScanAsync(mode, reportDelayMs, filter);
}

DLL_EXPORT bool chreBleStartScanAsyncV1_9(
    enum chreBleScanMode mode, uint32_t reportDelayMs,
    const struct chreBleScanFilterV1_9* filter, const void* cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreBleStartScanAsyncV1_9(mode, reportDelayMs, filter, cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->StartScanAsyncV1_9(mode, reportDelayMs, filter, cookie);
}

DLL_EXPORT bool chreBleStopScanAsync() {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreBleStopScanAsync();
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->StopScanAsync();
}

DLL_EXPORT bool chreBleStopScanAsyncV1_9(const void* cookie) {
  FakeChreApiProvider::GetInstance()
      ->GetFakeDetector()
      ->chreBleStopScanAsyncV1_9(cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->StopScanAsyncV1_9(cookie);
}

DLL_EXPORT bool chreBleFlushAsync(const void* absl_nullable cookie) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreBleFlushAsync(
      cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->FlushAsync(cookie);
}

DLL_EXPORT bool chreBleReadRssiAsync(uint16_t connectionHandle,
                                     const void* cookie) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreBleReadRssiAsync(
      connectionHandle, cookie);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->ReadRssiAsync(connectionHandle, cookie);
}

DLL_EXPORT bool chreBleGetScanStatus(struct chreBleScanStatus* status) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreBleGetScanStatus(
      status);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->GetScanStatus(status);
}

DLL_EXPORT bool chreBleSocketAccept(uint64_t socketId) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreBleSocketAccept(
      socketId);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->SocketAccept(socketId);
}

DLL_EXPORT int32_t
chreBleSocketSend(uint64_t socketId, const void* data, uint16_t length,
                  chreBleSocketPacketFreeFunction* freeCallback) {
  FakeChreApiProvider::GetInstance()->GetFakeDetector()->chreBleSocketSend(
      socketId, data, length, freeCallback);
  return FakeChreApiProvider::GetInstance()
      ->GetChreApiBleFunctions()
      ->SocketSend(socketId, data, length, freeCallback);
}

namespace lbs {
namespace contexthub {

// Create the functions that perform what would actually be run in the linux
// simulator.
uint32_t ChreApiBleFunctions::GetCapabilities() {
#ifdef CHRE_BLE_SUPPORT_ENABLED
  return EventLoopManagerSingleton::get()
      ->getBleRequestManager()
      .getCapabilities();
#else
  return CHRE_BLE_CAPABILITIES_NONE;
#endif  // CHRE_BLE_SUPPORT_ENABLED
}

uint32_t ChreApiBleFunctions::GetFilterCapabilities() {
#ifdef CHRE_BLE_SUPPORT_ENABLED
  return EventLoopManagerSingleton::get()
      ->getBleRequestManager()
      .getFilterCapabilities();
#else
  return CHRE_BLE_FILTER_CAPABILITIES_NONE;
#endif  // CHRE_BLE_SUPPORT_ENABLED
}

bool ChreApiBleFunctions::StartScanAsync(
    chreBleScanMode mode, uint32_t reportDelayMs,
    const struct chreBleScanFilter* filter) {
#ifdef CHRE_BLE_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  if (filter == nullptr) {
    return EventLoopManagerSingleton::get()
        ->getBleRequestManager()
        .startScanAsync(nanoapp, mode, reportDelayMs, nullptr /* filter */,
                        nullptr /* cookie */);
  }
  struct chreBleScanFilterV1_9 filter_v1_9 = {
      .rssiThreshold = filter->rssiThreshold,
      .genericFilterCount = filter->scanFilterCount,
      .genericFilters = filter->scanFilters,
      .broadcasterAddressFilterCount = 0,
      .broadcasterAddressFilters = nullptr};
  return EventLoopManagerSingleton::get()
      ->getBleRequestManager()
      .startScanAsync(nanoapp, mode, reportDelayMs, &filter_v1_9,
                      nullptr /* cookie */);
#else
  UNUSED_VAR(mode);
  UNUSED_VAR(reportDelayMs);
  UNUSED_VAR(filter);
  return false;
#endif  // CHRE_BLE_SUPPORT_ENABLED
}

bool ChreApiBleFunctions::StartScanAsyncV1_9(
    chreBleScanMode mode, uint32_t reportDelayMs,
    const struct chreBleScanFilterV1_9* filter, const void* cookie) {
#ifdef CHRE_BLE_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()
      ->getBleRequestManager()
      .startScanAsync(nanoapp, mode, reportDelayMs, filter, cookie);
#else
  UNUSED_VAR(mode);
  UNUSED_VAR(reportDelayMs);
  UNUSED_VAR(filter);
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_BLE_SUPPORT_ENABLED
}

bool ChreApiBleFunctions::StopScanAsync() {
#ifdef CHRE_BLE_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()->getBleRequestManager().stopScanAsync(
      nanoapp, nullptr);
#else
  return false;
#endif  // CHRE_BLE_SUPPORT_ENABLED
}

bool ChreApiBleFunctions::StopScanAsyncV1_9(const void* cookie) {
#ifdef CHRE_BLE_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()->getBleRequestManager().stopScanAsync(
      nanoapp, cookie);
#else
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_BLE_SUPPORT_ENABLED
}

bool ChreApiBleFunctions::FlushAsync(const void* /* cookie */) {
  // TODO (b/254723363): When flush is implemented in CHRE, call
  // BleRequestManager.flushAsync() here.
  return false;
}

bool ChreApiBleFunctions::ReadRssiAsync(uint16_t connectionHandle,
                                        const void* cookie) {
#ifdef CHRE_BLE_SUPPORT_ENABLED
  chre::Nanoapp* nanoapp = EventLoopManager::validateChreApiCall(__func__);
  return EventLoopManagerSingleton::get()->getBleRequestManager().readRssiAsync(
      nanoapp, connectionHandle, cookie);
#else
  UNUSED_VAR(connectionHandle);
  UNUSED_VAR(cookie);
  return false;
#endif  // CHRE_BLE_SUPPORT_ENABLED
}

bool ChreApiBleFunctions::GetScanStatus(
    struct chreBleScanStatus* /* status */) {
  return false;
}

bool ChreApiBleFunctions::SocketAccept(uint64_t /* socketId */) { return true; }

int32_t ChreApiBleFunctions::SocketSend(
    uint64_t /* socketId */, const void* /* data */, uint16_t /* length */,
    chreBleSocketPacketFreeFunction* /* freeCallback */) {
#ifdef CHRE_BLE_SUPPORT_ENABLED
  return chreBleSocketSendStatus::CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS;
#else
  return chreBleSocketSendStatus::CHRE_ERROR_NOT_SUPPORTED;
#endif  // CHRE_BLE_SUPPORT_ENABLED
}

}  // namespace contexthub
}  // namespace lbs
