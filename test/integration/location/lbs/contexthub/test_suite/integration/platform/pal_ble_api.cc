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

#include <chre.h>

#include <cstdint>
#include <optional>

#include "absl/base/nullability.h"
#include "absl/synchronization/mutex.h"
#include "chre/pal/ble.h"
#include "chre/pal/system.h"
#include "chre/pal/version.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/data_feed_base.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

namespace lbs::contexthub::testing {

namespace {

// TODO(b/219992369) Update interval when duty cycle details are known
uint32_t scanModeToInterval(chreBleScanMode mode) {
  switch (mode) {
    case CHRE_BLE_SCAN_MODE_BACKGROUND:
      return 1000 * kMillisToNano;
    case CHRE_BLE_SCAN_MODE_FOREGROUND:
      return 500 * kMillisToNano;
    case CHRE_BLE_SCAN_MODE_AGGRESSIVE:
      return 100 * kMillisToNano;
    default:
      return 1000 * kMillisToNano;
  }
}

}  // namespace

bool openBle(const struct chrePalSystemApi * /* systemApi */,
             const struct chrePalBleCallbacks *callbacks) {
  Simulator::GetInstance()->ble_callbacks_ = callbacks;
  return true;
}

void closeBle() {
  Simulator::GetInstance()->ble_callbacks_ = nullptr;
}

uint32_t getCapabilitiesBle() {
  return Simulator::GetInstance()->data_source_->GetCapabilitiesBle();
}

uint32_t getFilterCapabilitiesBle() {
  return Simulator::GetInstance()->data_source_->GetFilterCapabilitiesBle();
}

bool startScanBle(chreBleScanMode mode, uint32_t reportDelayMs,
                  const struct chreBleScanFilterV1_9 *filter) {
  Simulator *sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeStartScanBle);

  uint64_t next_time =
      static_cast<uint64_t>(sim->current_time_) + scanModeToInterval(mode);

  sim->data_to_control_[kBle] = LatestControlParams{
      .enabled = true,
      .oneshot = false,
      .interval = scanModeToInterval(mode),
      .next_expected_delivery = next_time,
      .latency = reportDelayMs,
  };

  sim->ble_callbacks_->scanStatusChangeCallback(true, CHRE_ERROR_NONE);
  auto copied_params = new SafeChreBleScanFilter(filter);
  sim->ble_scan_filter_.reset(copied_params);
  sim->RequestNewDataLocked(kBle,
                            DataRequestParams{.ble_scan_filter = copied_params,
                                              .latency_ns = reportDelayMs});
  return true;
}

bool stopScanBle() {
  Simulator *sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeStopScanBle);

  uint32_t next_time = static_cast<uint32_t>(sim->current_time_);

  sim->data_to_control_[kBle] = LatestControlParams{
      .enabled = false,
      .oneshot = false,
      .interval = 0,
      .next_expected_delivery = next_time,
  };

  sim->ble_callbacks_->scanStatusChangeCallback(false, CHRE_ERROR_NONE);
  return true;
}

void releaseAdvertisingEventBle(struct chreBleAdvertisementEvent *event) {
  Simulator *sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (event == sim->ble_advertisement_event_container_.get())
    sim->ble_advertisement_event_container_.reset();
}

bool readRssi(uint16_t connectionHandle) {
  Simulator *sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);

  uint32_t curr_time = static_cast<uint32_t>(sim->current_time_);

  sim->AddNanoappPlatformRequest(curr_time, kRequestTypeReadRssiBle);

  std::optional<chreBleReadRssiEvent> result =
      sim->data_source_->ReceivedBleRssiRequestAtTime(curr_time,
                                                      connectionHandle);

  if (result.has_value()) {
    sim->ble_callbacks_->readRssiCallback(result->result.errorCode,
                                          connectionHandle, result->rssi);
    return true;
  }
  return false;
}

}  // namespace lbs::contexthub::testing

const struct chrePalBleApi *absl_nullable
chrePalBleGetApi(uint32_t requestedApiVersion) {
  static const struct chrePalBleApi kApi = {
      .moduleVersion = requestedApiVersion,
      .open = lbs::contexthub::testing::openBle,
      .close = lbs::contexthub::testing::closeBle,
      .getCapabilities = lbs::contexthub::testing::getCapabilitiesBle,
      .getFilterCapabilities =
          lbs::contexthub::testing::getFilterCapabilitiesBle,
      .startScan = lbs::contexthub::testing::startScanBle,
      .stopScan = lbs::contexthub::testing::stopScanBle,
      .releaseAdvertisingEvent =
          lbs::contexthub::testing::releaseAdvertisingEventBle,
      .readRssi = lbs::contexthub::testing::readRssi,
  };

  if (!CHRE_PAL_VERSIONS_ARE_COMPATIBLE(kApi.moduleVersion,
                                        requestedApiVersion)) {
    return nullptr;
  }
  return &kApi;
}