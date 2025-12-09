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
#include "chre/pal/wwan.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

namespace lbs::contexthub::testing {

bool openWwan(const struct chrePalSystemApi * /* systemApi */,
              const struct chrePalWwanCallbacks *callbacks) {
  // we always use our own systemApi.
  Simulator::GetInstance()->wwan_callbacks_ = callbacks;
  return true;
}

void closeWwan() {
  Simulator::GetInstance()->wwan_callbacks_ = nullptr;
}

uint32_t getCapabilitiesWwan() {
  return Simulator::GetInstance()->data_source_->GetCapabilitiesWwan();
}

bool requestCellInfoWwan() {
  // instead of immediately returning the data, comply with queue structure and
  // add the request into the queue at the current time.
  // This allows all of the master logic to be contained in one location.
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeRequestCellInfoWwan);

  if (!sim->GetRequestWwanScanAvailable()) {
    return false;
  }

  auto next_time = static_cast<uint32_t>(sim->current_time_);

  sim->data_to_control_[kWwanCellInfo] = LatestControlParams{
      .enabled = true,
      .oneshot = true,
      .interval = 0,
      .next_expected_delivery = next_time,
  };

  sim->RequestNewDataLocked(kWwanCellInfo, DataRequestParams{});
  return true;
}

void releaseCellInfoResultWwan(struct chreWwanCellInfoResult *result) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (result == sim->wwan_cell_info_container_.get())
    sim->wwan_cell_info_container_.reset();
}

}  // namespace lbs::contexthub::testing

const struct chrePalWwanApi *chrePalWwanGetApi(uint32_t requestedApiVersion) {
  auto sim = lbs::contexthub::testing::Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (sim->chre_pal_wwan_api_) return sim->chre_pal_wwan_api_.get();

  sim->chre_pal_wwan_api_ = std::make_unique<chrePalWwanApi>(
      chrePalWwanApi{requestedApiVersion, lbs::contexthub::testing::openWwan,
                     lbs::contexthub::testing::closeWwan,
                     lbs::contexthub::testing::getCapabilitiesWwan,
                     lbs::contexthub::testing::requestCellInfoWwan,
                     lbs::contexthub::testing::releaseCellInfoResultWwan});
  return sim->chre_pal_wwan_api_.get();
}