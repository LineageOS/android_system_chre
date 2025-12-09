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

#include "location/lbs/contexthub/test_suite/integration/data_feed/data_feed_base.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "absl/base/nullability.h"
#include "chre_api/chre.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"

namespace lbs::contexthub::testing {

DataFeedBase::~DataFeedBase() {
  for (auto d : gnss_location_events_)
    if (d.second != nullptr) delete d.second;
  for (auto d : gnss_data_events_)
    if (d.second != nullptr) delete d.second;
  for (auto d : wifi_scan_events_)
    if (d.second != nullptr) delete d.second;
  for (const auto &m : sensor_bias_events_) {
    for (auto d : m) {
      if (d.second != nullptr) delete d.second;
    }
  }
}

void DataFeedBase::ReceivedMessageFromNanoapp(
    uint64_t /*t_ns*/, const SafeChreMessageToHostData & /*message*/) {}

SafeChreBleAdvertisementEvent *absl_nullable
DataFeedBase::ReceivedBleAdvertisementEventRequestAtTime(
    uint64_t /*t_ns*/, uint64_t /*latency*/,
    const SafeChreBleScanFilter & /*filter*/) {
  return nullptr;
}

std::optional<chreBleReadRssiEvent> DataFeedBase::ReceivedBleRssiRequestAtTime(
    uint64_t /*t_ns*/, uint16_t /*connectionHandle*/) {
  return std::nullopt;
}

SafeChreGnssLocationEvent *absl_nullable
DataFeedBase::ReceivedGnssLocationEventRequestAtTime(
    uint64_t /*t_ns*/, uint32_t /*min_interval_ms*/,
    uint32_t /*min_time_to_next_fix_ms*/) {
  return nullptr;
}

SafeChreGnssDataEvent *absl_nullable
DataFeedBase::ReceivedGnssDataEventRequestAtTime(uint64_t /*t_ns*/,
                                                 uint32_t /*min_interval_ms*/) {
  return nullptr;
}

SafeChreWwanCellInfoResult *absl_nullable
DataFeedBase::ReceivedWwanCallInfoResultRequestAtTime(uint64_t /*t*/) {
  return nullptr;
}

SafeChreWifiScanEvent *absl_nullable
DataFeedBase::ReceivedWifiScanEventRequestAtTime(
    uint64_t /*t_ns*/, const SafeChreWifiScanParams & /*params*/) {
  return nullptr;
}

SafeChreWifiRangingEvent *absl_nullable
DataFeedBase::ReceivedWifiRangingEventRequestAtTime(
    uint64_t /*t*/, const SafeChreWifiRangingParams & /*params*/) {
  return nullptr;
}

std::vector<chreSensorInfo> DataFeedBase::GetSensors() {
  return std::vector<chreSensorInfo>();
}

SafeChreSensorSamplingStatus *absl_nullable
DataFeedBase::GetSamplingStatusUpdate(uint64_t /*t_ns*/,
                                      uint32_t /*sensor_info_index*/,
                                      uint64_t /*requested_interval_ns*/,
                                      uint64_t /*requested_latency_ns*/) {
  return nullptr;
}

SafeChreSensorData *absl_nullable DataFeedBase::ConfigureSensor(
    uint64_t /*t_ns*/, uint32_t /*sensor_info_index*/, bool /*is_oneshot*/,
    uint64_t /*interval_ns*/, uint64_t /*latency_ns*/) {
  return nullptr;
}

SafeChreBleAdvertisementEvent *DataFeedBase::EmptyChreBleAdvertisementEvent() {
  auto ble_event = new chreBleAdvertisementEvent;
  std::memset(ble_event, 0, sizeof(*ble_event));
  ble_event->reports = nullptr;

  return static_cast<SafeChreBleAdvertisementEvent *>(ble_event);
}

SafeChreGnssLocationEvent *DataFeedBase::EmptyChreGnssLocationEvent(
    uint64_t t_ns) {
  auto loc_event = new chreGnssLocationEvent;
  std::memset(loc_event, 0, sizeof(*loc_event));
  loc_event->timestamp = t_ns / kMillisToNano;

  return static_cast<SafeChreGnssLocationEvent *>(loc_event);
}

SafeChreGnssDataEvent *DataFeedBase::EmptyChreGnssDataEvent(uint64_t t_ns) {
  auto data_event = new chreGnssDataEvent;
  std::memset(data_event, 0, sizeof(*data_event));
  data_event->measurement_count = 0;
  data_event->measurements = nullptr;
  data_event->clock.time_ns = t_ns;

  return static_cast<SafeChreGnssDataEvent *>(data_event);
}

SafeChreWwanCellInfoResult *DataFeedBase::EmptyChreWwanCellInfoResult() {
  auto cell_info = new chreWwanCellInfoResult;
  std::memset(cell_info, 0, sizeof(*cell_info));
  cell_info->cellInfoCount = 0;
  cell_info->cells = nullptr;

  return static_cast<SafeChreWwanCellInfoResult *>(cell_info);
}

SafeChreWifiScanEvent *DataFeedBase::EmptyChreWifiScanEvent(uint64_t t_ns) {
  auto scan_event = new chreWifiScanEvent;
  std::memset(scan_event, 0, sizeof(chreWifiScanEvent));
  scan_event->scannedFreqList = nullptr;
  scan_event->results = nullptr;
  scan_event->referenceTime = t_ns;

  return static_cast<SafeChreWifiScanEvent *>(scan_event);
}

SafeChreWifiRangingEvent *DataFeedBase::EmptyChreWifiRangingEvent() {
  auto ranging_event = new SafeChreWifiRangingEvent;
  std::memset(static_cast<chreWifiRangingEvent *>(ranging_event), 0,
              sizeof(chreWifiRangingEvent));
  ranging_event->results = nullptr;

  return static_cast<SafeChreWifiRangingEvent *>(ranging_event);
}

}  // namespace lbs::contexthub::testing
