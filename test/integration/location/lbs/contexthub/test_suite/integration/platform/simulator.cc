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

#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

#include <chre.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>  // NOLINT(build/c++11)
#include <climits>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <queue>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/nanoapp.h"
#include "chre/platform/linux/platform_log.h"
#include "chre/platform/shared/init.h"
#include "chre_api/chre.h"
#include "core/include/chre/core/settings.h"
#ifdef SIMULATION_LOAD_STATIC
#include "core/include/chre/core/static_nanoapps.h"
#endif  // SIMULATION_LOAD_STATIC
#include "location/lbs/contexthub/test_suite/integration/data_feed/data_feed_base.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/fragment.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "platform/linux/include/chre/target_platform/log.h"
#include "util/include/chre/util/dynamic_vector.h"
#include "util/include/chre/util/unique_ptr.h"

namespace lbs::contexthub::testing {

namespace {

chre::Setting GetSetting(uint8_t setting_id) {
  if (setting_id == CHRE_USER_SETTING_WIFI_AVAILABLE) {
    return chre::Setting::WIFI_AVAILABLE;
  } else if (setting_id == CHRE_USER_SETTING_AIRPLANE_MODE) {
    return chre::Setting::AIRPLANE_MODE;
  } else if (setting_id == CHRE_USER_SETTING_MICROPHONE) {
    return chre::Setting::MICROPHONE;
  } else if (setting_id == CHRE_USER_SETTING_BLE_AVAILABLE) {
    return chre::Setting::BLE_AVAILABLE;
  } else if (setting_id == CHRE_USER_SETTING_LOCATION) {
    return chre::Setting::LOCATION;
  }
  chreAbort(0);
}

}  // namespace

// static
Simulator* Simulator::sim_instance_ = nullptr;

// static
Simulator* Simulator::GetInstance() {
  if (sim_instance_ == nullptr) {
    sim_instance_ = new Simulator();
  }
  return sim_instance_;
}

// static
void Simulator::ResetInstance() {
  if (sim_instance_ != nullptr) {
    delete sim_instance_;
  }
  sim_instance_ = nullptr;
}

Simulator::Simulator() : finished_(1) {
  absl::MutexLock lock(&guard_);
  // set current_time_ to now
  current_time_ = 0;
  time_since_epoch_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

  // always allow host messages.
  data_to_control_[kMessageFromHost] = LatestControlParams{
      .enabled = true,
      .passive_enabled = true,
      .oneshot = false,
      .interval = 0,
      .next_expected_delivery = 0,
  };

  // always allow timer triggers.
  data_to_control_[kTimer] = LatestControlParams{
      .enabled = true,
      .passive_enabled = true,
      .oneshot = false,
      .interval = 0,
      .next_expected_delivery = 0,
  };

  dying_ = false;
}

uint64_t Simulator::GetCurrentTime() { return current_time_; }

// Returns the next time after last_consumed where a data point in passive_data
// exists. If none is found, returns ULLONG_MAX instead.
template <class ReturnType>
uint64_t GetNextTimeForType(const std::map<uint64_t, ReturnType> passive_data,
                            int64_t last_consumed) {
  for (const auto& msg : passive_data) {
    // maps are ordered by default based on the increasing order of key
    if (msg.first > last_consumed) {
      return msg.first;
    }
  }
  return ULLONG_MAX;
}

void Simulator::GetNextPassiveScheduledData(ScheduledData& data) {
  auto supported_passive_data_types = {kMessageFromHost,
                                       kGnssLocation,
                                       kGnssMeasurement,
                                       kWifiScan,
                                       kBiasEvent,
                                       kRequestWifiScanConfiguration,
                                       kRequestWwanScanConfiguration,
                                       kHostEndpointDisconnect,
                                       kUserSettingEvent};
  uint64_t best_time = ULLONG_MAX;
  int sensor_idx = -1;

  for (auto dt : supported_passive_data_types) {
    uint64_t last_consumed_time = 0;
    auto last_consumed = time_last_consumed_.find(dt);
    if (last_consumed != time_last_consumed_.end()) {
      last_consumed_time = last_consumed->second;
    }

    uint64_t local_best = best_time;
    if (dt == kMessageFromHost) {
      local_best = std::min(
          best_time, GetNextTimeForType<SafeChreMessageFromHostData>(
                         data_source_->messages_to_chre_, last_consumed_time));

    } else if (dt == kGnssLocation) {
      local_best = std::min(
          best_time,
          GetNextTimeForType<SafeChreGnssLocationEvent*>(
              data_source_->gnss_location_events_, last_consumed_time));

    } else if (dt == kGnssMeasurement) {
      local_best = std::min(
          best_time, GetNextTimeForType<SafeChreGnssDataEvent*>(
                         data_source_->gnss_data_events_, last_consumed_time));

    } else if (dt == kWifiScan) {
      local_best = std::min(
          best_time, GetNextTimeForType<SafeChreWifiScanEvent*>(
                         data_source_->wifi_scan_events_, last_consumed_time));

    } else if (dt == kBiasEvent) {
      int sensor_count = this->data_source_->sensor_bias_events_.size();
      for (int sid = 0; sid < sensor_count; sid++) {
        last_consumed_time = bias_last_consumed_[sid];
        auto next_time = GetNextTimeForType<SafeChreBiasEvent*>(
            data_source_->sensor_bias_events_[sid], last_consumed_time);
        if (next_time < local_best) {
          local_best = next_time;
          sensor_idx = sid;
        }
      }

    } else if (dt == kRequestWifiScanConfiguration) {
      local_best = std::min(
          best_time,
          GetNextTimeForType<bool>(data_source_->wifi_scan_available_events_,
                                   last_consumed_time));

    } else if (dt == kRequestWwanScanConfiguration) {
      local_best = std::min(
          best_time,
          GetNextTimeForType<bool>(data_source_->wwan_scan_available_events_,
                                   last_consumed_time));
    } else if (dt == kHostEndpointDisconnect) {
      local_best = std::min(
          best_time,
          GetNextTimeForType<uint16_t>(data_source_->host_endpoint_disconnects_,
                                       last_consumed_time));
    } else if (dt == kUserSettingEvent) {
      local_best = std::min(
          best_time, GetNextTimeForType<std::pair<uint8_t, bool>>(
                         data_source_->setting_events_, last_consumed_time));
    }

    if (local_best < best_time) {
      data.type = dt;
      best_time = local_best;
    }
  }

  data.delivery_time_ns = best_time;
  if (best_time == ULLONG_MAX) {
    // means we found none.
    data.type = kNone;
  } else if (data.type == kBiasEvent) {
    data.sensor_index = sensor_idx;
  }
}

void Simulator::ConsumePassiveScheduledData(const ScheduledData& data) {
  if (data.type == kBiasEvent) {
    bias_last_consumed_[data.sensor_index] = data.delivery_time_ns;
  } else {
    time_last_consumed_[data.type] = data.delivery_time_ns;
  }
}

bool Simulator::UnconsumedPassiveScheduledDataExist() {
  ScheduledData d{.type = kNone};

  GetNextPassiveScheduledData(d);
  if (d.type != kNone) {
    return true;
  }

  for (const auto& ttd : all_timer_trigger_data_) {
    if (ttd.second.trigger_time >= current_time_.load()) {
      return true;
    }
  }
  return false;
}

void Simulator::AllEventsProcessed() { MoveToNextTime(); }

void Simulator::MoveToNextTime() {
  absl::MutexLock lock(&guard_);
  if (dying_) {
    return;
  }

  while (!queue_.empty() || UnconsumedPassiveScheduledDataExist()) {
    ScheduledData curr, passive{};
    passive.delivery_time_ns = ULLONG_MAX;
    curr.delivery_time_ns = ULLONG_MAX;

    if (!queue_.empty()) {
      curr = queue_.top();
    }
    GetNextPassiveScheduledData(passive);

    // check whether the next event we should send is a scheduled/generated
    // response, or a passive data response.
    bool is_passive = false;
    if (queue_.empty() || passive.delivery_time_ns <= curr.delivery_time_ns) {
      curr = passive;
      is_passive = true;
      // consume the data, regardless of source.
      ConsumePassiveScheduledData(curr);
    } else {
      queue_.pop();
    }

    // update the time information.
    current_time_ = curr.delivery_time_ns;

    // if we have no data_to_control_, then this is a passive data point without
    // a request. Ignore it. Otherwise fetch the control params.
    LatestControlParams control_params;
    if (curr.type == kSensor) {
      if (sensor_data_to_control_.find(curr.sensor_index) ==
          sensor_data_to_control_.end()) {
        continue;
      }
      control_params = sensor_data_to_control_[curr.sensor_index];
    } else if (curr.type == kBiasEvent) {
      if (bias_data_to_control_.find(curr.sensor_index) ==
          bias_data_to_control_.end()) {
        continue;
      }
      control_params = bias_data_to_control_[curr.sensor_index];
    } else if (curr.type == kRequestWifiScanConfiguration) {
      wifi_scan_available_ =
          data_source_->wifi_scan_available_events_[curr.delivery_time_ns];
      continue;
    } else if (curr.type == kRequestWwanScanConfiguration) {
      wwan_scan_available_ =
          data_source_->wwan_scan_available_events_[curr.delivery_time_ns];
      continue;
    } else if (curr.type == kHostEndpointDisconnect) {
      uint16_t disconnected_endpoint =
          data_source_->host_endpoint_disconnects_[curr.delivery_time_ns];
      disconnected_host_endpoints_.insert(disconnected_endpoint);
      chre::EventLoopManagerSingleton::get()
          ->getHostEndpointManager()
          .postHostEndpointDisconnected(disconnected_endpoint);
      // We're posting an event, so make sure that filters through before
      // posting the next one - similar to how we only call SendEventDataBack()
      // at most once per call to MoveToNextTime().
      return;
    } else if (curr.type == kUserSettingEvent) {
      std::pair<uint8_t, bool> setting_state =
          data_source_->setting_events_[curr.delivery_time_ns];
      chre::EventLoopManagerSingleton::get()
          ->getSettingManager()
          .postSettingChange(GetSetting(setting_state.first),
                             setting_state.second);
      return;
    } else {
      if (data_to_control_.find(curr.type) == data_to_control_.end()) {
        continue;
      }
      control_params = data_to_control_[curr.type];
    }

    // skip anything disabled.
    if (!(control_params.enabled ||
          (is_passive && control_params.passive_enabled))) {
      continue;
    }

    // generated data's timestamp has to match the next_expected_delivery,
    // otherwise it's ignored.
    if (curr.type != kTimer && !is_passive &&
        control_params.next_expected_delivery != curr.delivery_time_ns) {
      continue;
    }

    // if this is a timer, make sure it's still valid - could be invalid because
    // it was already triggered with another timer having the same time,
    // or because it was replaced
    if (curr.type == kTimer) {
      bool has_valid_timer = false;
      for (const auto& td : all_timer_trigger_data_) {
        if (td.second.trigger_time <= current_time_) {
          has_valid_timer = true;
        }
      }
      if (!has_valid_timer) {
        continue;
      }
    }

    // disable oneshots.
    if (control_params.oneshot) {
      data_to_control_[curr.type].enabled = false;
    }

    // finally send the nanoapp the data.
    SendEventDataBack(curr.type, current_time_, is_passive, curr.sensor_index);

    // schedule next ScheduledData.
    if (!control_params.oneshot && control_params.interval != 0 &&
        !is_passive) {  // if is_passive == false, enabled is true.
      RequestNewDataLocked(curr.type,
                           DataRequestParams{
                               .min_interval_ms = control_params.interval,
                               .min_time_to_next_fix_ms = 0,
                               .ble_scan_filter = ble_scan_filter_.get(),
                               .sensor_index = curr.sensor_index,
                               .latency_ns = control_params.latency,
                           });
    }

    return;
  }

  chre::EventLoopManagerSingleton::get()->getDebugDumpManager().trigger();

  // we only get here if queue_ is empty and there are no more passive data
  dying_ = true;
  finished_.DecrementCount();
}

void Simulator::SendEventDataBack(DataType type, uint64_t time_ns,
                                  bool is_passive, uint32_t sensor_index) {
  // The default data will instead be provided via helper functions.
  // guard_ is still held.
  if (type == kMessageFromHost) {
    auto ret = data_source_->messages_to_chre_[time_ns];
    MaybeConnectEndpoint(ret.hostEndpoint);
    auto& comms_manager =
        chre::EventLoopManagerSingleton::get()->getHostCommsManager();
    if (ret.should_fragment) {
      auto fragments = FragmentHostMessage(next_outgoing_message_id_, ret);
      next_outgoing_message_id_++;

      for (const auto& fragment : fragments) {
        comms_manager.sendMessageToNanoappFromHost(
            fragment.appId, fragment.messageType, ret.hostEndpoint,
            fragment.message, fragment.messageSize, false /*isReliable=*/,
            0 /*messageSequenceNumber*/);
      }
    } else {
      comms_manager.sendMessageToNanoappFromHost(
          ret.appId, ret.messageType, ret.hostEndpoint, ret.message,
          ret.messageSize, false /*isReliable=*/, 0 /*messageSequenceNumber*/);
    }

  } else if (type == kTimer) {
    std::vector<void*> captured_timers;
    for (const auto& td : all_timer_trigger_data_) {
      if (td.second.trigger_time <= time_ns) {
        td.second.callback();
        captured_timers.push_back(td.first);
      }
    }
    for (auto ct : captured_timers) all_timer_trigger_data_.erase(ct);

  } else if (type == kGnssLocation) {
    SafeChreGnssLocationEvent* ret;
    if (is_passive) {
      ret = data_source_->gnss_location_events_[time_ns];
    } else {
      gnss_location_container_ = std::move(temp_gnss_location_container_);
      ret = gnss_location_container_.get();
    }
    ret->timestamp = time_ns / kMillisToNano + time_since_epoch_;
    gnss_callbacks_->locationEventCallback(ret->GetUnsafe());

  } else if (type == kGnssMeasurement) {
    SafeChreGnssDataEvent* ret;
    if (is_passive) {
      ret = data_source_->gnss_data_events_[time_ns];
    } else {
      gnss_data_event_container_ = std::move(temp_gnss_data_event_container_);
      ret = gnss_data_event_container_.get();
    }
    gnss_callbacks_->measurementEventCallback(ret->GetUnsafe());

  } else if (type == kWwanCellInfo) {
    wwan_cell_info_container_ = std::move(temp_wwan_cell_info_container_);
    wwan_callbacks_->cellInfoResultCallback(
        wwan_cell_info_container_->GetUnsafe());

  } else if (type == kWifiScan) {
    SafeChreWifiScanEvent* ret;
    if (is_passive) {
      ret = data_source_->wifi_scan_events_[time_ns];
    } else {
      wifi_scan_event_container_ = std::move(temp_wifi_scan_event_container_);
      ret = wifi_scan_event_container_.get();
    }
    if (ret != nullptr) {
      wifi_callbacks_->scanEventCallback(ret->GetUnsafe());
    } else {
      wifi_callbacks_->scanResponseCallback(/*pending=*/false, CHRE_ERROR);
    }
  } else if (type == kWifiRanging) {
    wifi_ranging_event_container_ =
        std::move(temp_wifi_ranging_event_container_);
    wifi_callbacks_->rangingEventCallback(
        wifi_ranging_event_container_->error_code,
        wifi_ranging_event_container_->GetUnsafe());

  } else if (type == kSensor) {
    auto raw_sensor =
        new SafeChreSensorDataRaw(temp_sensor_container_[sensor_index].get());
    sensor_container_[sensor_index].reset(raw_sensor);
    sensor_callbacks_->dataEventCallback(sensor_index, raw_sensor->raw_data);
    if (sensor_data_to_control_[sensor_index].with_flush_id != 0) {
      sensor_callbacks_->flushCompleteCallback(
          sensor_index, sensor_data_to_control_[sensor_index].with_flush_id,
          CHRE_ERROR_NONE);
      sensor_data_to_control_[sensor_index].with_flush_id = 0;
    }

  } else if (type == kBiasEvent) {
    auto bias_event = data_source_->sensor_bias_events_[sensor_index][time_ns];
    bias_event->SetTime(time_ns);
    sensor_callbacks_->biasEventCallback(sensor_index,
                                         bias_event->GetRawData());
  } else if (type == kBle) {
    ble_advertisement_event_container_ =
        std::move(temp_ble_advertisement_event_container_);
    ble_callbacks_->advertisingEventCallback(
        ble_advertisement_event_container_->GetUnsafe());
  }
}

void Simulator::RequestNewDataLocked(DataType type,
                                     const DataRequestParams& params) {
  auto curr_time = GetCurrentTime();
  uint64_t scheduled_time = 0;

  if (type == kGnssLocation) {
    auto gnss_loc = data_source_->ReceivedGnssLocationEventRequestAtTime(
        curr_time, params.min_interval_ms, params.min_time_to_next_fix_ms);
    scheduled_time = gnss_loc->timestamp * kMillisToNano;
    temp_gnss_location_container_.reset(gnss_loc);

  } else if (type == kGnssMeasurement) {
    auto gnss_event = data_source_->ReceivedGnssDataEventRequestAtTime(
        curr_time, params.min_interval_ms);
    scheduled_time = gnss_event->clock.time_ns;
    temp_gnss_data_event_container_.reset(gnss_event);

  } else if (type == kWwanCellInfo) {
    auto cell_info =
        data_source_->ReceivedWwanCallInfoResultRequestAtTime(curr_time);
    if (cell_info->cellInfoCount <= 0) return;
    for (int i = 0; i < cell_info->cellInfoCount; i++)
      scheduled_time = fmax(scheduled_time, cell_info->cells[i].timeStamp);
    temp_wwan_cell_info_container_.reset(cell_info);

  } else if (type == kWifiScan) {
    auto wifi_scan = data_source_->ReceivedWifiScanEventRequestAtTime(
        curr_time, *params.wifi_scan_params);
    if (wifi_scan == nullptr) {
      scheduled_time = curr_time;
    } else {
      scheduled_time = wifi_scan->referenceTime;
    }
    temp_wifi_scan_event_container_.reset(wifi_scan);

  } else if (type == kWifiRanging) {
    auto wifi_ranging = data_source_->ReceivedWifiRangingEventRequestAtTime(
        curr_time, *params.wifi_ranging_params);
    if (wifi_ranging->resultCount <= 0) return;
    for (int i = 0; i < wifi_ranging->resultCount; i++)
      scheduled_time = fmax(scheduled_time, wifi_ranging->results[i].timestamp);
    temp_wifi_ranging_event_container_.reset(wifi_ranging);

  } else if (type == kSensor) {
    auto dtc = sensor_data_to_control_[params.sensor_index];
    auto sensor_data = data_source_->ConfigureSensor(
        curr_time, params.sensor_index, dtc.oneshot, params.min_interval_ms,
        params.latency_ns);
    scheduled_time = sensor_data->header.baseTimestamp;
    for (int i = 0; i < sensor_data->header.readingCount; i++)
      std::visit([&scheduled_time](
                     auto const& e) { scheduled_time += e.timestampDelta; },
                 sensor_data->sample_data[i]);

    temp_sensor_container_[params.sensor_index].reset(sensor_data);
  } else if (type == kBle) {
    SafeChreBleAdvertisementEvent* ble_advertisment =
        data_source_->ReceivedBleAdvertisementEventRequestAtTime(
            curr_time, params.latency_ns, *params.ble_scan_filter);
    if (ble_advertisment->numReports <= 0) return;
    for (int i = 0; i < ble_advertisment->numReports; i++)
      scheduled_time =
          fmax(scheduled_time, ble_advertisment->reports[i].timestamp);
    temp_ble_advertisement_event_container_.reset(ble_advertisment);
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
  // clang complains about queue_ not being held by mutex. Disable that just
  // for here since we assume it's captured already.
  queue_.emplace(ScheduledData{
      .delivery_time_ns = scheduled_time,
      .type = type,
      .sensor_index = params.sensor_index,
  });
#pragma clang diagnostic pop
  if (type == kSensor)
    sensor_data_to_control_[params.sensor_index].next_expected_delivery =
        scheduled_time;
  else
    data_to_control_[type].next_expected_delivery = scheduled_time;
}

std::vector<std::pair<uint64_t, chreMessageToHostData>>
Simulator::GetReceivedHostMessages() {
  auto ret = std::vector<std::pair<uint64_t, chreMessageFromHostData>>(
      received_messages_.size());
  for (int i = 0; i < ret.size(); i++) {
    ret[i].first = received_messages_[i].first;
    ret[i].second = *static_cast<const chreMessageFromHostData*>(
        received_messages_[i].second.get());
  }
  return ret;
}

void Simulator::AddHostMessage(SafeChreMessageToHostData* msg) {
  absl::MutexLock lock(&guard_);
  uint64_t time = current_time_;
  data_source_->ReceivedMessageFromNanoapp(time, *msg);
  received_messages_.push_back(
      std::make_pair(time, std::unique_ptr<SafeChreMessageToHostData>(msg)));
}

void signalHandler(int /* sig */) {
  LOGI("Stop request received");
  chre::EventLoopManagerSingleton::get()->getEventLoop().stop();
}

bool Simulator::InitializeDataFeed(DataFeedBase* data) {
  if (!Simulator::VerifyValidData(data)) {
    return false;
  }
  data_source_ = data;
  return true;
}

void Simulator::Run(std::string nanoapps_str) {
  // Initialize the system.
  chre::PlatformLogSingleton::init();
  chre::initCommon();

  // Register a signal handler.
  std::signal(SIGINT, signalHandler);

  std::thread chre_thread([&]() {
    chre::EventLoopManagerSingleton::get()->lateInit();

    // Load the nanoapps specified in the flag..
    nanoapps_loaded_ = true;
#ifdef SIMULATION_LOAD_STATIC
    (void)nanoapps_str;
    chre::loadStaticNanoapps();
#else
    chre::DynamicVector<chre::UniquePtr<chre::Nanoapp>> nanoapps;
    std::vector<std::string> v = absl::StrSplit(nanoapps_str, ',');
    for (const auto& nanoapp_file : v) {
      nanoapps.push_back(chre::MakeUnique<chre::Nanoapp>());
      nanoapps.back()->loadFromFile(nanoapp_file);
      chre::EventLoopManagerSingleton::get()->getEventLoop().startNanoapp(
          nanoapps.back());
    }
#endif  //  SIMULATION_LOAD_STATIC

    chre::EventLoopManagerSingleton::get()->getEventLoop().run();
  });

  std::thread stop_fiber([] {
    Simulator::GetInstance()->finished_.Wait();
    chre::EventLoopManagerSingleton::get()->getEventLoop().stop();
  });
  stop_fiber.join();
  chre_thread.join();

  chre::deinitCommon();
  chre::PlatformLogSingleton::deinit();
}

// static
bool Simulator::VerifyValidData(DataFeedBase* data) {
  auto valid = true;

  if (!data->skip_initial_message_from_host_ &&
      data->messages_to_chre_.empty()) {
    std::cerr << kVerifyDataMessageToSendError << '\n';
    valid = false;
  }

  uint32_t ble_cap = data->GetCapabilitiesBle();
  SafeChreBleScanFilter ble_scan_filter;
  ble_scan_filter.rssiThreshold = 0;
  ble_scan_filter.genericFilterCount = 0;
  ble_scan_filter.genericFilters = nullptr;
  ble_scan_filter.broadcasterAddressFilterCount = 0;
  ble_scan_filter.broadcasterAddressFilters = nullptr;
  SafeChreBleAdvertisementEvent* ble_res =
      data->ReceivedBleAdvertisementEventRequestAtTime(0, 0, ble_scan_filter);
  if ((ble_cap & CHRE_BLE_CAPABILITIES_SCAN) && ble_res == nullptr) {
    std::cerr << kVerifyDataReceivedBleAdvertisementEventRequestAtTimeError
              << '\n';
    valid = false;
  }
  delete ble_res;

  auto gnss_cap = data->GetCapabilitiesGnss();
  auto gnss_loc = data->ReceivedGnssLocationEventRequestAtTime(0, 0, 0);
  auto gnss_event = data->ReceivedGnssDataEventRequestAtTime(0, 0);
  if ((gnss_cap & CHRE_GNSS_CAPABILITIES_LOCATION) && gnss_loc == nullptr) {
    std::cerr << kVerifyDataReceivedGnssLocationEventRequestAtTimeError << '\n';
    valid = false;
  }
  if ((gnss_cap & CHRE_GNSS_CAPABILITIES_MEASUREMENTS) &&
      gnss_event == nullptr) {
    std::cerr << kVerifyDataReceivedGnssDataEventRequestAtTimeError << '\n';
    valid = false;
  }
  delete gnss_loc;
  delete gnss_event;

  auto wwan_res = data->ReceivedWwanCallInfoResultRequestAtTime(0);
  if ((data->GetCapabilitiesWwan() & CHRE_WWAN_GET_CELL_INFO) &&
      wwan_res == nullptr) {
    std::cerr << kVerifyDataReceivedWwanCallInfoResultRequestAtTimeError
              << '\n';
    valid = false;
  }
  delete wwan_res;

  SafeChreWifiRangingParams wifi_ranging_params;
  wifi_ranging_params.targetListLen = 1;
  wifi_ranging_params.targetList = new chreWifiRangingTarget[1];
  auto wifi_ranging =
      data->ReceivedWifiRangingEventRequestAtTime(0, wifi_ranging_params);
  if ((data->GetCapabilitiesWifi() & CHRE_WIFI_CAPABILITIES_RTT_RANGING) &&
      wifi_ranging == nullptr) {
    std::cerr << kVerifyDataReceivedWifiRangingEventRequestAtTime << '\n';
    valid = false;
  }
  delete wifi_ranging;

  if (data->GetSensorCount() != 0 &&
      data->GetSensorCount() != data->GetSensors().size()) {
    std::cerr << kVerifyDataReceivedSensorGetSensorsAtTime << '\n';
    valid = false;
  }

  auto sensor_sampling_status = data->GetSamplingStatusUpdate(0, 0, 500, 500);
  if (data->GetSensorCount() != 0 && sensor_sampling_status == nullptr) {
    std::cerr << kVerifyDataReceivedSensorGetSamplingStatusUpdateAtTime << '\n';
    valid = false;
  }
  delete sensor_sampling_status;

  auto sensors_configure_sensor = data->ConfigureSensor(0, 0, true, 1000, 0);
  if (data->GetSensorCount() != 0 && sensors_configure_sensor == nullptr) {
    std::cerr << kVerifyDataReceivedSensorConfigureSensorAtTime << '\n';
    valid = false;
  }
  delete sensors_configure_sensor;

  if (data->GetSensorCount() != 0 && !data->sensor_bias_events_.empty() &&
      data->sensor_bias_events_.size() != data->GetSensorCount()) {
    std::cerr << kVerifyBiasVectorInitializedCorrectly << '\n';
    valid = false;
  }

  absl::flat_hash_set<uint16_t> disconnected_host_endpoints;
  for (const auto& [time_ns, host_endpoint] :
       data->host_endpoint_disconnects_) {
    if (disconnected_host_endpoints.contains(host_endpoint)) {
      std::cerr << kVerifyHostEndpointDisconnectsUnique << '\n';
      valid = false;
    }
    disconnected_host_endpoints.insert(host_endpoint);
  }

  return valid;
}

bool Simulator::GetRequestWifiScanAvailable() { return wifi_scan_available_; }

bool Simulator::GetRequestWwanScanAvailable() { return wwan_scan_available_; }

void Simulator::MaybeConnectEndpoint(uint16_t host_endpoint) {
  CHECK(!disconnected_host_endpoints_.contains(host_endpoint))
      << "Cannot connect endpoint that has been disconnected." << std::endl
      << "Host endpoint: " << host_endpoint << std::endl
      << "Connected at time: "
      << GetNextTimeForType(data_source_->host_endpoint_disconnects_, 0)
      << std::endl
      << "Disconnected at time: " << current_time_;

  if (connected_host_endpoints_.contains(host_endpoint)) {
    return;
  }

  chreHostEndpointInfo endpoint_info{
      .hostEndpointId = host_endpoint,
      .hostEndpointType = CHRE_HOST_ENDPOINT_TYPE_APP,
      .isNameValid = 0,
      .isTagValid = 0,
  };
  if (nanoapps_loaded_) {
    chre::EventLoopManagerSingleton::get()
        ->getHostEndpointManager()
        .postHostEndpointConnected(endpoint_info);
  }
}

// Adds a PAL request to the simulator's verification data.
void Simulator::AddNanoappPlatformRequest(uint64_t timestamp,
                                          NanoappRequestType request_type) {
  if (!nanoapps_loaded_) {
    return;
  }
  nanoapp_requests_received_.push_back(std::make_pair(timestamp, request_type));
}

// Returns a copy of the PAL requests received by the simulator.
std::vector<std::pair<uint64_t, NanoappRequestType>>
Simulator::GetNanoappPlatformRequests() {
  return nanoapp_requests_received_;
}

void Simulator::SetNanoappLoadedForTest(bool loaded) {
  nanoapps_loaded_ = loaded;
}

}  // namespace lbs::contexthub::testing