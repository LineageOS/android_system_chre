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

#include "chre/platform/system_timer.h"

#include <cstdint>
#include <functional>

#include "absl/synchronization/mutex.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"
#include "third_party/contexthub/chre/util/include/chre/util/time.h"

namespace chre {

SystemTimer::SystemTimer() {}

SystemTimer::~SystemTimer() {}

bool SystemTimer::init() {
  mTimerId = this;
  return true;
}

void CallCallback(SystemTimerCallback *callback, void *data) {
  callback(data);
}

bool SystemTimer::set(SystemTimerCallback *callback, void *data,
                      Nanoseconds delay) {
  auto sim = lbs::contexthub::testing::Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);

  sim->all_timer_trigger_data_[mTimerId] =
      lbs::contexthub::testing::TimerTriggerData{
          .trigger_time = sim->current_time_ + delay.toRawNanoseconds(),
          .callback = std::bind(CallCallback, callback, data),
      };

  sim->queue_.emplace(lbs::contexthub::testing::ScheduledData{
      .delivery_time_ns =
          static_cast<uint64_t>(sim->current_time_ + delay.toRawNanoseconds()),
      .type = lbs::contexthub::testing::kTimer,
  });

  return true;
}

bool SystemTimer::cancel() {
  auto sim = lbs::contexthub::testing::Simulator::GetInstance();
  sim->all_timer_trigger_data_.erase(mTimerId);

  return true;
}

bool SystemTimer::isActive() {
  auto sim = lbs::contexthub::testing::Simulator::GetInstance();
  return sim->all_timer_trigger_data_.find(this) !=
         sim->all_timer_trigger_data_.end();
}

}  // namespace chre