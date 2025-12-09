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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_DATA_FEED_SAFE_CHRE_STRUCTS_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_DATA_FEED_SAFE_CHRE_STRUCTS_H_

#include <chre.h>
#include <stdlib.h>

#include <cstdint>
#include <variant>
#include <vector>

#include "absl/types/span.h"

enum NanoappRequestType {
  kRequestTypeControlLocationSessionGnss,
  kRequestTypeControlMeasurementSessionGnss,
  kRequestTypeConfigurePassiveLocationListenerGnss,
  kRequestTypeGetSensors,
  kRequestTypeConfigureSensor,
  kRequestTypeFlushSensor,
  kRequestTypeConfigureBiasEventsSensor,
  kRequestTypeGetThreeAxisBiasSensor,
  kRequestTypeConfigureScanMonitorWifi,
  kRequestTypeRequestScanWifi,
  kRequestTypeRequestRangingWifi,
  kRequestTypeRequestCellInfoWwan,
  kRequestTypeReleaseCellInfoResultWwan,
  kRequestTypeStartScanBle,
  kRequestTypeStopScanBle,
  kRequestTypeReadRssiBle,
};

using chreSensorThreeAxisSampleData =
    chreSensorThreeAxisData::chreSensorThreeAxisSampleData;
using chreSensorOccurrenceSampleData =
    chreSensorOccurrenceData::chreSensorOccurrenceSampleData;
using chreSensorFloatSampleData =
    chreSensorFloatData::chreSensorFloatSampleData;
using chreSensorByteSampleData = chreSensorByteData::chreSensorByteSampleData;

struct SafeChreMessageFromHostData;
typedef SafeChreMessageFromHostData SafeChreMessageToHostData;

struct SafeChreMessageFromHostData : public chreMessageFromHostData {
 public:
  uint64_t appId;  // NOLINT
  bool should_fragment = false;
  int message_version = 1;
  using chreMessageFromHostData::chreMessageFromHostData;
  ~SafeChreMessageFromHostData();

  // add an explicit copy and assignment constructor so that we can copy the
  // memory allocated to this->message. This allows users to use this object
  // without having to worry about memory management, no matter what.
  SafeChreMessageFromHostData(const SafeChreMessageFromHostData &from);
  SafeChreMessageFromHostData &operator=(
      const SafeChreMessageFromHostData &from);
};

struct SafeChreBleAdvertisementEvent : public chreBleAdvertisementEvent {
  using chreBleAdvertisementEvent::chreBleAdvertisementEvent;

 public:
  ~SafeChreBleAdvertisementEvent() {
    if (reports != nullptr) {
      delete[] reports;
    }
  }

  chreBleAdvertisementEvent *GetUnsafe() {
    return static_cast<chreBleAdvertisementEvent *>(this);
  }
};

struct SafeChreBleScanFilter : public chreBleScanFilterV1_9 {
  using chreBleScanFilterV1_9::chreBleScanFilterV1_9;

 public:
  explicit SafeChreBleScanFilter(const chreBleScanFilterV1_9 *);
  ~SafeChreBleScanFilter() {
    if (genericFilters != nullptr) {
      delete[] genericFilters;
    }
    if (broadcasterAddressFilters != nullptr) {
      delete[] broadcasterAddressFilters;
    }
  }
};

struct SafeChreGnssLocationEvent : public chreGnssLocationEvent {
  using chreGnssLocationEvent::chreGnssLocationEvent;

 public:
  chreGnssLocationEvent *GetUnsafe() {
    return static_cast<chreGnssLocationEvent *>(this);
  }
};

struct SafeChreGnssDataEvent : public chreGnssDataEvent {
  using chreGnssDataEvent::chreGnssDataEvent;

 public:
  ~SafeChreGnssDataEvent() {
    if (measurements != nullptr) delete[] measurements;
  }

  chreGnssDataEvent *GetUnsafe() {
    return static_cast<chreGnssDataEvent *>(this);
  }
};

struct SafeChreWwanCellInfoResult : public chreWwanCellInfoResult {
  using chreWwanCellInfoResult::chreWwanCellInfoResult;

 public:
  ~SafeChreWwanCellInfoResult() {
    if (cells != nullptr) delete[] cells;
  }

  chreWwanCellInfoResult *GetUnsafe() {
    return static_cast<chreWwanCellInfoResult *>(this);
  }
};

struct SafeChreWifiScanParams : public chreWifiScanParams {
  using chreWifiScanParams::chreWifiScanParams;

 public:
  explicit SafeChreWifiScanParams(const chreWifiScanParams *);
  ~SafeChreWifiScanParams() {
    if (frequencyList != nullptr) delete[] frequencyList;
    if (ssidList != nullptr) delete[] ssidList;
  }

  chreWifiScanParams *GetUnsafe() {
    return static_cast<chreWifiScanParams *>(this);
  }
};

struct SafeChreWifiRangingParams : public chreWifiRangingParams {
  using chreWifiRangingParams::chreWifiRangingParams;

 public:
  explicit SafeChreWifiRangingParams(const chreWifiRangingParams *);
  ~SafeChreWifiRangingParams() {
    if (targetList != nullptr) delete[] targetList;
  }

  chreWifiRangingParams *GetUnsafe() {
    return static_cast<chreWifiRangingParams *>(this);
  }
};

struct SafeChreWifiScanEvent : public chreWifiScanEvent {
  using chreWifiScanEvent::chreWifiScanEvent;

 public:
  ~SafeChreWifiScanEvent() {
    if (results != nullptr) delete[] results;
    if (scannedFreqList != nullptr) delete[] scannedFreqList;
  }

  chreWifiScanEvent *GetUnsafe() {
    return static_cast<chreWifiScanEvent *>(this);
  }
};

struct SafeChreWifiRangingEvent : public chreWifiRangingEvent {
  using chreWifiRangingEvent::chreWifiRangingEvent;

 public:
  uint8_t error_code = 0;
  ~SafeChreWifiRangingEvent() {
    if (results != nullptr) delete[] results;
  }

  chreWifiRangingEvent *GetUnsafe() {
    return static_cast<chreWifiRangingEvent *>(this);
  }
};

enum SensorDataType {
  kSensorThreeAxisData = 0,
  kSensorOccurrenceData = 1,
  kSensorFloatData = 2,
  kSensorByteData = 3,
  kSensorNone = 4,
};

typedef std::variant<chreSensorThreeAxisSampleData,
                     chreSensorOccurrenceSampleData, chreSensorFloatSampleData,
                     chreSensorByteSampleData>
    ChreSensorSampleData;

struct SafeChreGetSensorsResponse {
 public:
  explicit SafeChreGetSensorsResponse(absl::Span<const chreSensorInfo> info);
  ~SafeChreGetSensorsResponse();

  int size;
  chreSensorInfo *sensors;
};

struct SafeChreSensorData {
 public:
  explicit SafeChreSensorData(SensorDataType data_type)
      : sensor_data_type(data_type) {}

  chreSensorDataHeader header;
  SensorDataType sensor_data_type;
  std::vector<ChreSensorSampleData> sample_data;
};

struct SafeChreSensorDataRaw {
 public:
  explicit SafeChreSensorDataRaw(const SafeChreSensorData *data);
  ~SafeChreSensorDataRaw();
  chreSensorDataHeader header;
  SensorDataType sensor_data_type;
  void *raw_data;
};

struct SafeChreBiasEvent {
 public:
  explicit SafeChreBiasEvent(SensorDataType data_type, uint8_t sensor_accuracy);
  SafeChreBiasEvent(const SafeChreBiasEvent &event);
  ~SafeChreBiasEvent();

  SensorDataType sensor_data_type;
  ChreSensorSampleData bias_data;

  void CreateRawData();
  void *GetRawData();
  void SetTime(uint64_t t);

 private:
  chreSensorDataHeader header;
  void *raw_data;
};

struct SafeChreSensorSamplingStatus : public chreSensorSamplingStatus {
  using chreSensorSamplingStatus::chreSensorSamplingStatus;
  chreSensorSamplingStatus *GetUnsafe() {
    return static_cast<chreSensorSamplingStatus *>(this);
  }
};

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_DATA_FEED_SAFE_CHRE_STRUCTS_H_