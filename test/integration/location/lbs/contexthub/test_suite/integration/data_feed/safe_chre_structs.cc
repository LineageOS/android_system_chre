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

#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"

#include <chre.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "absl/types/span.h"

void CopyFromTo(const SafeChreMessageFromHostData &from,
                SafeChreMessageFromHostData *to) {
  to->messageType = from.messageType;
  to->reservedMessageType = from.reservedMessageType;
  to->messageSize = from.messageSize;
  to->hostEndpoint = from.hostEndpoint;
  to->appId = from.appId;
  to->should_fragment = from.should_fragment;
  to->message_version = from.message_version;
  if (to->messageSize != 0) {
    auto msg = malloc(to->messageSize);
    memcpy(msg, from.message, to->messageSize);
    to->message = msg;
  } else {
    to->message = nullptr;
  }
}

SafeChreMessageFromHostData::~SafeChreMessageFromHostData() {
  if (this->messageSize != 0) free(const_cast<void *>(this->message));
}

SafeChreMessageFromHostData::SafeChreMessageFromHostData(
    const SafeChreMessageFromHostData &from) {
  CopyFromTo(from, this);
}

SafeChreMessageFromHostData &SafeChreMessageFromHostData::operator=(
    const SafeChreMessageFromHostData &from) {
  if (this != &from) {
    if (this->messageSize != 0) free(const_cast<void *>(this->message));
    CopyFromTo(from, this);
  }
  return *this;
}

SafeChreBleScanFilter::SafeChreBleScanFilter(
    const chreBleScanFilterV1_9 *filter) {
  if (filter == nullptr) {
    rssiThreshold = 0;
    genericFilterCount = 0;
    genericFilters = nullptr;
    broadcasterAddressFilterCount = 0;
    broadcasterAddressFilters = nullptr;
    return;
  }
  rssiThreshold = filter->rssiThreshold;
  genericFilterCount = filter->genericFilterCount;
  broadcasterAddressFilterCount = filter->broadcasterAddressFilterCount;
  if (genericFilterCount == 0 || filter->genericFilters == nullptr) {
    genericFilters = nullptr;
  } else {
    auto scan_filters = new chreBleGenericFilter[genericFilterCount];
    memcpy(scan_filters, filter->genericFilters,
           genericFilterCount * sizeof(chreBleGenericFilter));
    genericFilters = static_cast<const chreBleGenericFilter *>(scan_filters);
  }
  if (broadcasterAddressFilterCount == 0 ||
      filter->broadcasterAddressFilters == nullptr) {
    broadcasterAddressFilters = nullptr;
  } else {
    auto scan_filters =
        new chreBleBroadcasterAddressFilter[broadcasterAddressFilterCount];
    memcpy(scan_filters, filter->broadcasterAddressFilters,
           broadcasterAddressFilterCount *
               sizeof(chreBleBroadcasterAddressFilter));
    broadcasterAddressFilters =
        static_cast<const chreBleBroadcasterAddressFilter *>(scan_filters);
  }
}

SafeChreWifiScanParams::SafeChreWifiScanParams(
    const chreWifiScanParams *params) {
  scanType = params->scanType;
  maxScanAgeMs = params->maxScanAgeMs;
  frequencyListLen = params->frequencyListLen;
  ssidListLen = params->ssidListLen;
  radioChainPref = params->radioChainPref;

  if (params->frequencyList == nullptr || frequencyListLen == 0) {
    frequencyList = nullptr;
  } else {
    auto freq_list = new uint32_t[frequencyListLen];
    memcpy(freq_list, params->frequencyList,
           frequencyListLen * sizeof(uint32_t));
    frequencyList = static_cast<const uint32_t *>(freq_list);
  }

  if (params->ssidList == nullptr || ssidListLen == 0) {
    ssidList = nullptr;
  } else {
    auto ssid_list = new chreWifiSsidListItem[ssidListLen];
    memcpy(ssid_list, params->ssidList,
           ssidListLen * sizeof(chreWifiSsidListItem));
    ssidList = static_cast<const chreWifiSsidListItem *>(ssid_list);
  }
}

SafeChreWifiRangingParams::SafeChreWifiRangingParams(
    const chreWifiRangingParams *params) {
  targetListLen = params->targetListLen;
  auto target_list = new chreWifiRangingTarget[targetListLen];
  memcpy(target_list, params->targetList,
         targetListLen * sizeof(chreWifiRangingTarget));
  targetList = target_list;
}

SafeChreGetSensorsResponse::SafeChreGetSensorsResponse(
    absl::Span<const chreSensorInfo> info) {
  size = info.size();
  sensors = new chreSensorInfo[size];
  for (int i = 0; i < size; i++) {
    sensors[i] = chreSensorInfo(info[i]);
  }
}

SafeChreGetSensorsResponse::~SafeChreGetSensorsResponse() {
  delete[] sensors;
}

SafeChreSensorDataRaw::SafeChreSensorDataRaw(const SafeChreSensorData *data) {
  header = data->header;
  sensor_data_type = data->sensor_data_type;

  if (sensor_data_type == kSensorThreeAxisData) {
    auto temp_data = static_cast<chreSensorThreeAxisData *>(
        malloc(sizeof(chreSensorDataHeader) +
               header.readingCount * sizeof(chreSensorThreeAxisSampleData)));
    temp_data->header = header;
    for (int i = 0; i < header.readingCount; i++)
      temp_data->readings[i] =
          std::get<chreSensorThreeAxisSampleData>(data->sample_data[i]);
    raw_data = temp_data;

  } else if (sensor_data_type == kSensorOccurrenceData) {
    auto temp_data = static_cast<chreSensorOccurrenceData *>(
        malloc(sizeof(chreSensorDataHeader) +
               header.readingCount * sizeof(chreSensorOccurrenceSampleData)));
    temp_data->header = header;
    for (int i = 0; i < header.readingCount; i++)
      temp_data->readings[i] =
          std::get<chreSensorOccurrenceSampleData>(data->sample_data[i]);
    raw_data = temp_data;

  } else if (sensor_data_type == kSensorFloatData) {
    auto temp_data = static_cast<chreSensorFloatData *>(
        malloc(sizeof(chreSensorDataHeader) +
               header.readingCount * sizeof(chreSensorFloatSampleData)));
    temp_data->header = header;
    for (int i = 0; i < header.readingCount; i++)
      temp_data->readings[i] =
          std::get<chreSensorFloatSampleData>(data->sample_data[i]);
    raw_data = temp_data;

  } else if (sensor_data_type == kSensorByteData) {
    auto temp_data = static_cast<chreSensorByteData *>(
        malloc(sizeof(chreSensorDataHeader) +
               header.readingCount * sizeof(chreSensorByteSampleData)));
    temp_data->header = header;
    for (int i = 0; i < header.readingCount; i++)
      temp_data->readings[i] =
          std::get<chreSensorByteSampleData>(data->sample_data[i]);
    raw_data = temp_data;

  } else {
    raw_data = nullptr;
  }
}

SafeChreSensorDataRaw::~SafeChreSensorDataRaw() {
  free(raw_data);
}

SafeChreBiasEvent::~SafeChreBiasEvent() {
  if (raw_data != nullptr) free(raw_data);
}

SafeChreBiasEvent::SafeChreBiasEvent(SensorDataType data_type,
                                     uint8_t sensor_accuracy)
    : sensor_data_type(data_type), raw_data(nullptr) {
  header.readingCount = 1;
  header.accuracy = sensor_accuracy;
  header.reserved = 0;
}

SafeChreBiasEvent::SafeChreBiasEvent(const SafeChreBiasEvent &event) {
  header = event.header;
  sensor_data_type = event.sensor_data_type;
  bias_data = event.bias_data;
  raw_data = nullptr;
}

void SafeChreBiasEvent::CreateRawData() {
  if (sensor_data_type == kSensorNone) return;
  if (raw_data != nullptr) return;

  if (sensor_data_type == kSensorThreeAxisData) {
    auto sz = sizeof(chreSensorThreeAxisData);
    auto tmp_data = static_cast<chreSensorThreeAxisData *>(malloc(sz));
    tmp_data->header = header;
    tmp_data->readings[0] = std::get<chreSensorThreeAxisSampleData>(bias_data);
    raw_data = tmp_data;

  } else if (sensor_data_type == kSensorOccurrenceData) {
    auto sz = sizeof(chreSensorOccurrenceData);
    auto tmp_data = static_cast<chreSensorOccurrenceData *>(malloc(sz));
    tmp_data->header = header;
    tmp_data->readings[0] = std::get<chreSensorOccurrenceSampleData>(bias_data);
    raw_data = tmp_data;

  } else if (sensor_data_type == kSensorFloatData) {
    auto sz = sizeof(chreSensorFloatData);
    auto tmp_data = static_cast<chreSensorFloatData *>(malloc(sz));
    tmp_data->header = header;
    tmp_data->readings[0] = std::get<chreSensorFloatSampleData>(bias_data);
    raw_data = tmp_data;

  } else if (sensor_data_type == kSensorByteData) {
    auto sz = sizeof(chreSensorByteData);
    auto tmp_data = static_cast<chreSensorByteData *>(malloc(sz));
    tmp_data->header = header;
    tmp_data->readings[0] = std::get<chreSensorByteSampleData>(bias_data);
    raw_data = tmp_data;
  }
}

void *SafeChreBiasEvent::GetRawData() {
  if (raw_data == nullptr) CreateRawData();
  return raw_data;
}

void SafeChreBiasEvent::SetTime(uint64_t t) {
  header.baseTimestamp = t;
}