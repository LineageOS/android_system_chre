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
#include <memory>

#include "absl/synchronization/mutex.h"
#include "chre/pal/system.h"
#include "chre/pal/wifi.h"
#include "chre_api/chre.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

namespace lbs::contexthub::testing {

bool openWifi(const struct chrePalSystemApi * /* systemApi */,
              const struct chrePalWifiCallbacks *callbacks) {
  // we always use our own systemApi.
  Simulator::GetInstance()->wifi_callbacks_ = callbacks;
  return true;
}

void closeWifi() {
  Simulator::GetInstance()->wifi_callbacks_ = nullptr;
}

uint32_t getCapabilitiesWifi() {
  return Simulator::GetInstance()->data_source_->GetCapabilitiesWifi();
}

bool configureScanMonitor(bool enable) {
  auto sim = Simulator::GetInstance();
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeConfigureScanMonitorWifi);

  if (sim->data_to_control_.find(kWifiScan) == sim->data_to_control_.end()) {
    // new entry for kWifiScan. Only set if enabled.
    if (enable) {
      sim->data_to_control_[kWifiScan] = LatestControlParams{
          .enabled = false,
          .passive_enabled = true,
          .oneshot = false,
          .interval = 0,
          .next_expected_delivery = 0,
      };
    }
  } else if (!sim->data_to_control_[kWifiScan].enabled && !enable) {
    // if nothing is enabled, erase it.
    sim->data_to_control_.erase(kWifiScan);
  } else {
    sim->data_to_control_[kWifiScan].passive_enabled = enable;
  }
  sim->wifi_callbacks_->scanMonitorStatusChangeCallback(enable,
                                                        CHRE_ERROR_NONE);
  return true;
}

bool requestScan(const struct chreWifiScanParams *params) {
  // instead of immediately returning the data, comply with queue structure and
  // add the request into the queue at the current time.
  // This allows all of the master logic to be contained in one location.
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeRequestScanWifi);

  if (!sim->GetRequestWifiScanAvailable()) {
    return false;
  }

  auto next_time = static_cast<uint32_t>(sim->current_time_);

  bool passive_enabled = false;
  if (sim->data_to_control_.find(kWifiScan) != sim->data_to_control_.end())
    passive_enabled = sim->data_to_control_[kWifiScan].passive_enabled;

  sim->data_to_control_[kWifiScan] = LatestControlParams{
      .enabled = true,
      .passive_enabled = passive_enabled,
      .oneshot = true,
      .interval = 0,
      .next_expected_delivery = next_time,
  };

  auto copied_params = new SafeChreWifiScanParams(params);
  sim->wifi_scan_params_.reset(copied_params);
  sim->wifi_callbacks_->scanResponseCallback(true, 0);

  sim->RequestNewDataLocked(
      kWifiScan, DataRequestParams{.wifi_scan_params = copied_params});

  return true;
}

void releaseScanEvent(struct chreWifiScanEvent *event) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (event == sim->wifi_scan_event_container_.get())
    sim->wifi_scan_event_container_.reset();
}

bool requestRanging(const struct chreWifiRangingParams *params) {
  // instead of immediately returning the data, comply with queue structure and
  // add the request into the queue at the current time.
  // This allows all of the master logic to be contained in one location.
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeRequestRangingWifi);

  auto next_time = static_cast<uint32_t>(sim->current_time_);

  sim->data_to_control_[kWifiRanging] = LatestControlParams{
      .enabled = true,
      .passive_enabled = false,
      .oneshot = true,
      .interval = 0,
      .next_expected_delivery = next_time,
  };

  auto copied_params = new SafeChreWifiRangingParams(params);
  sim->wifi_ranging_params_.reset(copied_params);

  sim->RequestNewDataLocked(kWifiRanging,
                            DataRequestParams{
                                .wifi_ranging_params = copied_params,
                            });

  return true;
}

void releaseRangingEvent(struct chreWifiRangingEvent *event) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (event == sim->wifi_ranging_event_container_.get())
    sim->wifi_ranging_event_container_.reset();
}

}  // namespace lbs::contexthub::testing

const struct chrePalWifiApi *chrePalWifiGetApi(uint32_t requestedApiVersion) {
  auto sim = lbs::contexthub::testing::Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (sim->chre_pal_wifi_api_) return sim->chre_pal_wifi_api_.get();

  sim->chre_pal_wifi_api_ = std::make_unique<chrePalWifiApi>(chrePalWifiApi{
      .moduleVersion = requestedApiVersion,
      .open = lbs::contexthub::testing::openWifi,
      .close = lbs::contexthub::testing::closeWifi,
      .getCapabilities = lbs::contexthub::testing::getCapabilitiesWifi,
      .configureScanMonitor = lbs::contexthub::testing::configureScanMonitor,
      .requestScan = lbs::contexthub::testing::requestScan,
      .releaseScanEvent = lbs::contexthub::testing::releaseScanEvent,
      .requestRanging = lbs::contexthub::testing::requestRanging,
      .releaseRangingEvent = lbs::contexthub::testing::releaseRangingEvent,
  });
  return sim->chre_pal_wifi_api_.get();
}