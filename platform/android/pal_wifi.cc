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

#include "chre/platform/android/pal_wifi.h"

#include <chrono>

#include "chre/pal/wifi.h"
#include "chre/util/enum.h"
#include "chre/util/memory.h"
#include "chre/util/time.h"
#include "chre/util/unique_ptr.h"
#include "chre_api/include/chre_api/chre/wifi.h"

#include <cinttypes>

/**
 * A simulated implementation of the WiFi PAL for the CHRE AP platform.
 */
namespace {

uint32_t chrePalWifiGetCapabilities() {
  // TODO(b/445584823): implement this
  return CHRE_WIFI_CAPABILITIES_SCAN_MONITORING |
         CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN | CHRE_WIFI_CAPABILITIES_NAN_SUB;
}

bool chrePalWifiConfigureScanMonitor(bool /*enable*/) {
  // TODO(b/445584823): implement this
  return true;
}

bool chrePalWifiApiRequestScan(const struct chreWifiScanParams * /*params*/) {
  // TODO(b/445584823): implement this
  return true;
}

bool chrePalWifiApiRequestRanging(
    const struct chreWifiRangingParams * /*params*/) {
  // TODO(b/445584823): implement this
  return true;
}

void chrePalWifiApiReleaseScanEvent(struct chreWifiScanEvent * /*event*/) {
  // TODO(b/445584823): implement this
}

void chrePalWifiApiReleaseRangingEvent(
    struct chreWifiRangingEvent * /*event*/) {
  // TODO(b/445584823): implement this
}

bool chrePalWifiApiNanSubscribe(
    const struct chreWifiNanSubscribeConfig * /*config*/) {
  // TODO(b/445584823): implement this
  return true;
}

bool chrePalWifiApiNanSubscribeCancel(const uint32_t /*subscriptionId*/) {
  // TODO(b/445584823): implement this
  return true;
}

void chrePalWifiApiNanReleaseDiscoveryEvent(
    struct chreWifiNanDiscoveryEvent * /*event*/) {
  // TODO(b/445584823): implement this
}

bool chrePalWifiApiRequestNanRanging(
    const struct chreWifiNanRangingParams * /*params*/) {
  // TODO(b/445584823): implement this
  return true;
}

void chrePalWifiApiClose() {
  // TODO(b/445584823): implement this
}

bool chrePalWifiApiOpen(const struct chrePalSystemApi * /*systemApi*/,
                        const struct chrePalWifiCallbacks * /*callbacks*/) {
  // TODO(b/445584823): implement this
  return true;
}

bool chrePalWifiNanGetCapabilities(
    struct chreWifiNanCapabilities * /* capabilities */) {
  // TODO(b/445584823): implement this
  return false;
}

}  // anonymous namespace

void chrePalWifiEnableResponse(PalWifiAsyncRequestTypes /*requestType*/,
                               bool /*enableResponse*/) {
  // TODO(b/445584823): implement this
}

bool chrePalWifiIsScanMonitoringActive() {
  // TODO(b/445584823): implement this
  return false;
}

bool chrePalWifiTriggerScanMonitorEvent() {
  // TODO(b/445584823): implement this
  return false;
}

void chrePalWifiDelayResponse(PalWifiAsyncRequestTypes /*requestType*/,
                              std::chrono::milliseconds /*milliseconds*/) {
  // TODO(b/445584823): implement this
}

const struct chrePalWifiApi *chrePalWifiGetApi(uint32_t requestedApiVersion) {
  static const struct chrePalWifiApi kApi = {
      .moduleVersion = CHRE_PAL_WIFI_API_CURRENT_VERSION,
      .open = chrePalWifiApiOpen,
      .close = chrePalWifiApiClose,
      .getCapabilities = chrePalWifiGetCapabilities,
      .configureScanMonitor = chrePalWifiConfigureScanMonitor,
      .requestScan = chrePalWifiApiRequestScan,
      .releaseScanEvent = chrePalWifiApiReleaseScanEvent,
      .requestRanging = chrePalWifiApiRequestRanging,
      .releaseRangingEvent = chrePalWifiApiReleaseRangingEvent,
      .nanSubscribe = chrePalWifiApiNanSubscribe,
      .nanSubscribeCancel = chrePalWifiApiNanSubscribeCancel,
      .releaseNanDiscoveryEvent = chrePalWifiApiNanReleaseDiscoveryEvent,
      .requestNanRanging = chrePalWifiApiRequestNanRanging,
      .getNanCapabilities = chrePalWifiNanGetCapabilities,
  };

  if (!CHRE_PAL_VERSIONS_ARE_COMPATIBLE(kApi.moduleVersion,
                                        requestedApiVersion)) {
    return nullptr;
  } else {
#ifdef CHRE_WIFI_NAN_SUPPORT_ENABLED
    chre::PalNanEngineSingleton::init();
#endif
    return &kApi;
  }
}
