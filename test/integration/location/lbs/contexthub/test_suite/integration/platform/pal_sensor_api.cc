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
#include <cstring>
#include <memory>
#include <variant>

#include "absl/synchronization/mutex.h"
#include "chre/pal/sensor.h"
#include "chre/pal/system.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

namespace lbs::contexthub::testing {

bool openSensor(const struct chrePalSystemApi * /*systemApi*/,
                const struct chrePalSensorCallbacks *callbacks) {
  // we always use our own systemApi.
  Simulator::GetInstance()->sensor_callbacks_ = callbacks;
  return true;
}

void closeSensor() {
  Simulator::GetInstance()->sensor_callbacks_ = nullptr;
}

bool getSensors(const chreSensorInfo **sensors, uint32_t *arraySize) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeGetSensors);

  if (sim->data_source_->GetSensorCount() == 0) {
    *arraySize = 0;
  } else {
    auto resp = new SafeChreGetSensorsResponse(sim->data_source_->GetSensors());
    *arraySize = resp->size;
    chreSensorInfo **variable_sensors = const_cast<chreSensorInfo **>(sensors);
    *variable_sensors = resp->sensors;
    sim->get_sensors_response_container_.reset(resp);
  }
  return true;
}

bool configureSensor(uint32_t sensorInfoIndex,
                     enum chreSensorConfigureMode mode, uint64_t intervalNs,
                     uint64_t latencyNs) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeConfigureSensor);

  auto new_control = LatestControlParams{
      .enabled = true,
      .passive_enabled = false,
      .oneshot = false,
      .interval = static_cast<uint32_t>(intervalNs),
      .latency = latencyNs,
      .with_flush_id = 0,
  };

  switch (mode) {
    case CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS:
      new_control.passive_enabled = true;
      break;
    case CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT:
    case CHRE_SENSOR_CONFIGURE_MODE_PASSIVE_ONE_SHOT:
      // we have no support for passive oneshot, so treat it like a regular one
      new_control.oneshot = true;
      break;
    case CHRE_SENSOR_CONFIGURE_MODE_PASSIVE_CONTINUOUS:
      new_control.passive_enabled = true;
      new_control.enabled = false;
      break;
    case CHRE_SENSOR_CONFIGURE_MODE_DONE:
      new_control.enabled = false;
      new_control.passive_enabled = false;
      break;
  }

  sim->sensor_data_to_control_[sensorInfoIndex] = new_control;
  sim->sampling_status_container_.reset(
      sim->data_source_->GetSamplingStatusUpdate(
          sim->GetCurrentTime(), sensorInfoIndex, intervalNs, latencyNs));
  sim->sensor_callbacks_->samplingStatusUpdateCallback(
      sensorInfoIndex, sim->sampling_status_container_->GetUnsafe());
  sim->RequestNewDataLocked(
      kSensor, DataRequestParams{
                   .min_interval_ms = static_cast<uint32_t>(intervalNs),
                   .sensor_index = sensorInfoIndex,
                   .latency_ns = latencyNs,
               });

  return true;
}

bool flushSensor(uint32_t sensorInfoIndex, uint32_t *flushRequestId) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeFlushSensor);

  sim->current_flush_id_++;
  *flushRequestId = sim->current_flush_id_;
  auto to_return = sim->temp_sensor_container_[sensorInfoIndex].get();
  auto curr_time = sim->current_time_.load();

  // change next_expected_delivery so that the planned delivery is invalidated.
  sim->sensor_data_to_control_[sensorInfoIndex].next_expected_delivery =
      curr_time;

  if (to_return == nullptr || to_return->header.readingCount == 0) {
    sim->sensor_callbacks_->flushCompleteCallback(
        sensorInfoIndex, *flushRequestId, CHRE_ERROR_NONE);
    if (to_return != nullptr) {
      sim->queue_.emplace(ScheduledData{
          .delivery_time_ns = curr_time,
          .type = kSensor,
          .sensor_index = sensorInfoIndex,
      });
    }
  } else {
    auto so_far_time = to_return->header.baseTimestamp;
    int i = 0;
    for (; i < to_return->sample_data.size(); i++) {
      std::visit(
          [&so_far_time](auto const &e) { so_far_time += e.timestampDelta; },
          to_return->sample_data[i]);
      if (so_far_time > curr_time) break;
    }

    // data is valid up to i-1, remove data from [i, size)
    to_return->sample_data.erase(to_return->sample_data.begin() + i,
                                 to_return->sample_data.end());
    to_return->header.readingCount = to_return->sample_data.size();
    sim->queue_.emplace(ScheduledData{
        .delivery_time_ns = curr_time,
        .type = kSensor,
        .sensor_index = sensorInfoIndex,
    });
    sim->sensor_data_to_control_[sensorInfoIndex].with_flush_id =
        *flushRequestId;
  }
  return true;
}

void *GetLastBiasEvent(const Simulator &sim, uint32_t idx, uint64_t t_ns) {
  auto curr_time = sim.current_time_.load();
  SafeChreBiasEvent *last_best = nullptr;
  for (auto it = sim.data_source_->sensor_bias_events_[idx].begin();
       it != sim.data_source_->sensor_bias_events_[idx].end(); it++) {
    if (it->first <= curr_time)
      last_best = it->second;
    else
      break;
  }

  if (last_best == nullptr) return nullptr;

  last_best->SetTime(t_ns);
  return last_best->GetRawData();
}

bool configureBiasEventsSensor(uint32_t sensorInfoIndex, bool enable,
                               uint64_t /*latencyNs*/) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeConfigureBiasEventsSensor);

  if (!enable) {
    sim->bias_data_to_control_.erase(sensorInfoIndex);
    return true;
  }

  // verify that the sensor is active first.
  if (sim->sensor_data_to_control_.find(sensorInfoIndex) ==
      sim->sensor_data_to_control_.end())
    return false;
  if (!(sim->sensor_data_to_control_[sensorInfoIndex].enabled &&
        !sim->sensor_data_to_control_[sensorInfoIndex].oneshot))
    return false;

  auto new_control = LatestControlParams{
      .enabled = true,
      .passive_enabled = true,
      .oneshot = false,
      .interval = static_cast<uint32_t>(0),
      .latency = 0,
      .with_flush_id = 0,
  };

  sim->bias_data_to_control_[sensorInfoIndex] = new_control;

  // need to return latest bias data immediately.
  auto last_bias =
      GetLastBiasEvent(*sim, sensorInfoIndex, sim->current_time_.load());
  if (last_bias != nullptr)
    sim->sensor_callbacks_->biasEventCallback(sensorInfoIndex, last_bias);

  return true;
}

bool getThreeAxisBiasSensor(uint32_t sensorInfoIndex,
                            struct chreSensorThreeAxisData *bias) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  sim->AddNanoappPlatformRequest(sim->current_time_.load(),
                                 kRequestTypeGetThreeAxisBiasSensor);

  auto last_bias =
      GetLastBiasEvent(*sim, sensorInfoIndex, sim->current_time_.load());

  if (last_bias == nullptr) return false;

  memcpy(bias, last_bias, sizeof(chreSensorThreeAxisData));
  return true;
}

void releaseSensorDataEventSensor(void * /* data */) {
  // we'll release it on our own so we can ignore this call.
}

void releaseSamplingStatusEventSensor(chreSensorSamplingStatus *status) {
  auto sim = Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);

  if (sim->sampling_status_container_->GetUnsafe() == status)
    sim->sampling_status_container_.reset();
}

void releaseBiasEventSensor(void * /* bias */) {}

}  // namespace lbs::contexthub::testing

const struct chrePalSensorApi *chrePalSensorGetApi(
    uint32_t requestedApiVersion) {
  auto sim = lbs::contexthub::testing::Simulator::GetInstance();
  absl::MutexLock lock(&sim->guard_);
  if (sim->chre_pal_gnss_api_) return sim->chre_pal_sensor_api_.get();

  sim->chre_pal_sensor_api_ =
      std::make_unique<chrePalSensorApi>(chrePalSensorApi{
          .moduleVersion = requestedApiVersion,
          .open = lbs::contexthub::testing::openSensor,
          .close = lbs::contexthub::testing::closeSensor,
          .getSensors = lbs::contexthub::testing::getSensors,
          .configureSensor = lbs::contexthub::testing::configureSensor,
          .flush = lbs::contexthub::testing::flushSensor,
          .configureBiasEvents =
              lbs::contexthub::testing::configureBiasEventsSensor,
          .getThreeAxisBias = lbs::contexthub::testing::getThreeAxisBiasSensor,
          .releaseSensorDataEvent =
              lbs::contexthub::testing::releaseSensorDataEventSensor,
          .releaseSamplingStatusEvent =
              lbs::contexthub::testing::releaseSamplingStatusEventSensor,
          .releaseBiasEvent = lbs::contexthub::testing::releaseBiasEventSensor,
      });
  return sim->chre_pal_sensor_api_.get();
}