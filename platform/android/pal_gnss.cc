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

#include "chre/platform/android/pal_gnss.h"
#include "chre/pal/gnss.h"
#include "chre/platform/log.h"

#include "chre/util/memory.h"
#include "chre/util/unique_ptr.h"

#include <cinttypes>

/**
 * A simulated implementation of the GNSS PAL for the CHRE AP platform.
 */
namespace {

uint32_t chrePalGnssGetCapabilities() {
  // TODO(b/445584823): implement this
  return CHRE_GNSS_CAPABILITIES_LOCATION | CHRE_GNSS_CAPABILITIES_MEASUREMENTS |
         CHRE_GNSS_CAPABILITIES_GNSS_ENGINE_BASED_PASSIVE_LISTENER;
}

bool chrePalControlLocationSession(bool /*enable*/, uint32_t /*minIntervalMs*/,
                                   uint32_t /* minTimeToNextFixMs */) {
  // TODO(b/445584823): implement this
  return true;
}

void chrePalGnssReleaseLocationEvent(struct chreGnssLocationEvent * /*event*/) {
  // TODO(b/445584823): implement this
}

bool chrePalControlMeasurementSession(bool /*enable*/,
                                      uint32_t /*minIntervalMs*/) {
  // TODO(b/445584823): implement this
  return true;
}

void chrePalGnssReleaseMeasurementDataEvent(
    struct chreGnssDataEvent * /*event*/) {
  // TODO(b/445584823): implement this
}

void chrePalGnssApiClose() {
  // TODO(b/445584823): implement this
}

bool chrePalGnssApiOpen(const struct chrePalSystemApi * /*systemApi*/,
                        const struct chrePalGnssCallbacks * /*callbacks*/) {
  // TODO(b/445584823): implement this
  return true;
}

bool chrePalGnssconfigurePassiveLocationListener(bool /*enable*/) {
  // TODO(b/445584823): implement this
  return true;
}

}  // anonymous namespace

bool chrePalGnssIsLocationEnabled() {
  // TODO(b/445584823): implement this
  return false;
}

bool chrePalGnssIsMeasurementEnabled() {
  // TODO(b/445584823): implement this
  return false;
}

bool chrePalGnssIsPassiveLocationListenerEnabled() {
  // TODO(b/445584823): implement this
  return false;
}

void chrePalGnssDelaySendingLocationEvents(bool /*enabled*/) {
  // TODO(b/445584823): implement this
}

void chrePalGnssStartSendingLocationEvents() {
  // TODO(b/445584823): implement this
}

const struct chrePalGnssApi *chrePalGnssGetApi(uint32_t requestedApiVersion) {
  static const struct chrePalGnssApi kApi = {
      .moduleVersion = CHRE_PAL_GNSS_API_CURRENT_VERSION,
      .open = chrePalGnssApiOpen,
      .close = chrePalGnssApiClose,
      .getCapabilities = chrePalGnssGetCapabilities,
      .controlLocationSession = chrePalControlLocationSession,
      .releaseLocationEvent = chrePalGnssReleaseLocationEvent,
      .controlMeasurementSession = chrePalControlMeasurementSession,
      .releaseMeasurementDataEvent = chrePalGnssReleaseMeasurementDataEvent,
      .configurePassiveLocationListener =
          chrePalGnssconfigurePassiveLocationListener,
  };

  if (!CHRE_PAL_VERSIONS_ARE_COMPATIBLE(kApi.moduleVersion,
                                        requestedApiVersion)) {
    return nullptr;
  } else {
    return &kApi;
  }
}
