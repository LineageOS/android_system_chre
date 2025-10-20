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
#include "chre/pal/gnss.h"
#include "chre/pal/system.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

namespace lbs::contexthub::testing {

bool openGnss(const struct chrePalSystemApi * /* systemApi */,
              const struct chrePalGnssCallbacks *callbacks) {
  // we always use our own systemApi.
  Simulator::GetInstance()->gnss_callbacks_ = callbacks;
  return true;
}

void closeGnss() {
  Simulator::GetInstance()->gnss_callbacks_ = nullptr;
}

uint32_t getCapabilitiesGnss() {
  return Simulator::GetInstance()->data_source_->GetCapabilitiesGnss();
}

bool controlLocationSessionGnss(bool enable, uint32_t minIntervalMs,
                                uint32_t minTimeToNextFixMs) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeControlLocationSessionGnss);

  uint32_t next_time =
      static_cast<uint32_t>(sim->current_time_) + minTimeToNextFixMs;

  bool is_passive_enabled = false;
  if (sim->data_to_control_.find(kGnssLocation) !=
      sim->data_to_control_.end()) {
    is_passive_enabled = sim->data_to_control_[kGnssLocation].passive_enabled;
  }
  sim->data_to_control_[kGnssLocation] = LatestControlParams{
      .enabled = enable,
      .passive_enabled = is_passive_enabled,
      .oneshot = false,
      .interval = minIntervalMs,
      .next_expected_delivery = next_time,
  };

  if ((enable || is_passive_enabled) == false)
    // active and passive data are disabled, so reset the kGnssLocation entry.
    sim->data_to_control_.erase(kGnssLocation);

  if (enable) {
    sim->RequestNewDataLocked(kGnssLocation,
                              DataRequestParams{
                                  .min_interval_ms = minIntervalMs,
                                  .min_time_to_next_fix_ms = minTimeToNextFixMs,
                              });
  }

  // respond back to CHRE so it knows the request was accepted.
  sim->gnss_callbacks_->locationStatusChangeCallback(enable, CHRE_ERROR_NONE);
  return true;
}

void releaseLocationEventGnss(struct chreGnssLocationEvent *event) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (event == sim->gnss_location_container_.get())
    sim->gnss_location_container_.reset();
}

bool controlMeasurementSessionGnss(bool enable, uint32_t minIntervalMs) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeControlMeasurementSessionGnss);

  uint32_t next_time = static_cast<uint32_t>(sim->current_time_);

  sim->data_to_control_[kGnssMeasurement] = LatestControlParams{
      .enabled = enable,
      .oneshot = false,
      .interval = minIntervalMs,
      .next_expected_delivery = next_time,
  };

  if (enable) {
    sim->RequestNewDataLocked(kGnssMeasurement,
                              DataRequestParams{
                                  .min_interval_ms = minIntervalMs,
                              });
  }

  // respond back to CHRE so it knows the request was accepted.
  sim->gnss_callbacks_->measurementStatusChangeCallback(enable,
                                                        CHRE_ERROR_NONE);
  return true;
}

void releaseMeasurementDataEventGnss(struct chreGnssDataEvent *event) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (event == sim->gnss_data_event_container_.get())
    sim->gnss_data_event_container_.reset();
}

bool configurePassiveLocationListenerGnss(bool enable) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(
      sim->current_time_.load(),
      kRequestTypeConfigurePassiveLocationListenerGnss);

  if (sim->data_to_control_.find(kGnssLocation) !=
      sim->data_to_control_.end()) {
    auto dtc = sim->data_to_control_[kGnssLocation];

    if (!dtc.enabled && !enable)  // no active data, and now no passive ones.
      sim->data_to_control_.erase(kGnssLocation);
    else
      sim->data_to_control_[kGnssLocation].passive_enabled = enable;
  } else if (enable) {
    sim->data_to_control_[kGnssLocation] = LatestControlParams{
        .enabled = false,
        .passive_enabled = enable,
        .oneshot = false,
        .interval = 0,
        .next_expected_delivery = 0,
    };
  }
  return true;
}

}  // namespace lbs::contexthub::testing

const struct chrePalGnssApi *chrePalGnssGetApi(uint32_t requestedApiVersion) {
  auto sim = lbs::contexthub::testing::Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (sim->chre_pal_gnss_api_) return sim->chre_pal_gnss_api_.get();

  sim->chre_pal_gnss_api_ = std::make_unique<chrePalGnssApi>(chrePalGnssApi{
      requestedApiVersion, lbs::contexthub::testing::openGnss,
      lbs::contexthub::testing::closeGnss,
      lbs::contexthub::testing::getCapabilitiesGnss,
      lbs::contexthub::testing::controlLocationSessionGnss,
      lbs::contexthub::testing::releaseLocationEventGnss,
      lbs::contexthub::testing::controlMeasurementSessionGnss,
      lbs::contexthub::testing::releaseMeasurementDataEventGnss,
      lbs::contexthub::testing::configurePassiveLocationListenerGnss});
  return sim->chre_pal_gnss_api_.get();
}