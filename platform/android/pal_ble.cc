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

#include "chre/pal/ble.h"

#include "chre.h"
#include "chre/platform/android/pal_ble.h"
#include "chre/util/memory.h"
#include "chre/util/unique_ptr.h"
#include "chre_api/chre/ble.h"
#include "include/chre/platform/android/pal_ble.h"

#include <cinttypes>

/**
 * A simulated implementation of the BLE PAL for the CHRE AP platform.
 */
namespace {

uint32_t chrePalBleGetCapabilities() {
  // TODO(b/445584823): implement this
  return CHRE_BLE_CAPABILITIES_SCAN |
         CHRE_BLE_CAPABILITIES_SCAN_RESULT_BATCHING |
         CHRE_BLE_CAPABILITIES_SCAN_FILTER_BEST_EFFORT;
}

uint32_t chrePalBleGetFilterCapabilities() {
  // TODO(b/445584823): implement this
  return CHRE_BLE_FILTER_CAPABILITIES_RSSI |
         CHRE_BLE_FILTER_CAPABILITIES_SERVICE_DATA;
}

bool chrePalBleStartScan(chreBleScanMode /*mode*/, uint32_t /*reportDelayMs*/,
                         const struct chreBleScanFilterV1_9 * /* filter */) {
  // TODO(b/445584823): implement this
  return true;
}

bool chrePalBleStopScan() {
  // TODO(b/445584823): implement this
  return true;
}

void chrePalBleReleaseAdvertisingEvent(
    struct chreBleAdvertisementEvent * /*event*/) {
  // TODO(b/445584823): implement this
}

bool chrePalBleReadRssi(uint16_t /*connectionHandle*/) {
  // TODO(b/445584823): implement this
  return true;
}

bool chrePalBleFlush() {
  // TODO(b/445584823): implement this
  return true;
}

void chrePalBleApiClose() {
  // TODO(b/445584823): implement this
}

bool chrePalBleApiOpen(const struct chrePalSystemApi * /*systemApi*/,
                       const struct chrePalBleCallbacks * /*callbacks*/) {
  // TODO(b/445584823): implement this
  return true;
}

}  // anonymous namespace

const struct chrePalBleApi *chrePalBleGetApi(uint32_t requestedApiVersion) {
  static const struct chrePalBleApi kApi = {
      .moduleVersion = CHRE_PAL_BLE_API_CURRENT_VERSION,
      .open = chrePalBleApiOpen,
      .close = chrePalBleApiClose,
      .getCapabilities = chrePalBleGetCapabilities,
      .getFilterCapabilities = chrePalBleGetFilterCapabilities,
      .startScan = chrePalBleStartScan,
      .stopScan = chrePalBleStopScan,
      .releaseAdvertisingEvent = chrePalBleReleaseAdvertisingEvent,
      .readRssi = chrePalBleReadRssi,
      .flush = chrePalBleFlush,
  };

  if (!CHRE_PAL_VERSIONS_ARE_COMPATIBLE(kApi.moduleVersion,
                                        requestedApiVersion)) {
    return nullptr;
  } else {
    return &kApi;
  }
}

namespace chre {

bool chrePalIsBleEnabled() {
  // TODO(b/445584823): implement this
  return false;
}

void delayBleScanStart(bool /*delay*/) {
  // TODO(b/445584823): implement this
}

bool startBleScan() {
  // TODO(b/445584823): implement this
  return false;
}

void resetSocketVariables() {
  // TODO(b/445584823): implement this
}

void incrementSocketClosureCount() {
  // TODO(b/445584823): implement this
}

uint32_t getSocketClosureCount() {
  // TODO(b/445584823): implement this
  return 0;
}

void setSocketOpenSuccess(bool /*success*/) {
  // TODO(b/445584823): implement this
}

bool getSocketOpenSuccess() {
  return false;
}

void setSocketOpenFailureReason(const char * /*reason*/) {
  // TODO(b/445584823): implement this
}

const char *getSocketOpenFailureReason() {
  // TODO(b/445584823): implement this
  return nullptr;
}

void setSocketCapabilities(BtSocketCapabilities /*capabilities*/) {
  // TODO(b/445584823): implement this
}

BtSocketCapabilities getSocketCapabilities() {
  // TODO(b/445584823): implement this
  return {0, 0, 0, 0};
}

}  // namespace chre
