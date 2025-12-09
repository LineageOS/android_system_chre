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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_PLATFORM_SIMULATOR_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_PLATFORM_SIMULATOR_H_

#include <chre.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "chre/pal/ble.h"
#include "chre/pal/gnss.h"
#include "chre/pal/sensor.h"
#include "chre/pal/wifi.h"
#include "chre/pal/wwan.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/data_feed_base.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"

typedef chreMessageFromHostData chreMessageToHostData;

namespace lbs::contexthub::testing {

// these error messages are used by the VerifyValidData function and exposed
// to allow better testing.
const char kVerifyDataInvalidData[] = "[***INVALID DATA***]";
const char kVerifyDataMessageToSendError[] =
    "[***INVALID DATA***]: messages_to_chre_ is empty, and thus no message "
    "will be sent to the CHRE core to start the nanoapp. If you don't want to "
    "send an initial message, please set skip_initial_message_to_host_.";
const char kVerifyDataReceivedBleAdvertisementEventRequestAtTimeError[] =
    "[***INVALID DATA***]: CHRE_BLE_CAPABILITIES_SCAN is set, but "
    "ReceivedBleAdvertisementEventRequestAtTime is not defined.";
const char kVerifyDataReceivedGnssLocationEventRequestAtTimeError[] =
    "[***INVALID DATA***]: CHRE_GNSS_CAPABILITIES_LOCATION is set, but "
    "ReceivedGnssLocationEventRequestAtTime is not defined.";
const char kVerifyDataReceivedGnssDataEventRequestAtTimeError[] =
    "[***INVALID DATA***]: CHRE_GNSS_CAPABILITIES_MEASUREMENTS is set, "
    "but ReceivedGnssDataEventRequestAtTime is not defined.";
const char kVerifyDataReceivedWwanCallInfoResultRequestAtTimeError[] =
    "[***INVALID DATA***]: CHRE_WWAN_GET_CELL_INFO is set, but "
    "ReceivedWwanCallInfoResultRequestAtTime is not defined.";
const char kVerifyDataReceivedWifiRangingEventRequestAtTime[] =
    "[***INVALID DATA***]: CHRE_WIFI_CAPABILITIES_RTT_RANGING is set, but "
    "ReceivedWifiRangingEventRequestAtTime is not defined.";
const char kVerifyDataReceivedSensorGetSensorsAtTime[] =
    "[***INVALID DATA***]: GetSensorCount returns a non-zero number, but "
    "GetSensors returns a vector with a different number of elements.";
const char kVerifyDataReceivedSensorGetSamplingStatusUpdateAtTime[] =
    "[***INVALID DATA***]: GetSensorCount returns a non-zero number, but "
    "GetSamplingStatusUpdate is not defined.";
const char kVerifyDataReceivedSensorConfigureSensorAtTime[] =
    "[***INVALID DATA***]: GetSensorCount returns a non-zero number, but "
    "ConfigureSensor is not defined.";
const char kVerifyBiasVectorInitializedCorrectly[] =
    "[***INVALID DATA***]: The bias vector 'sensor_bias_events_' is "
    "initialized in correctly, please make sure that the size of the bias list "
    "is either 0 or equal to the number of sensors, even if you don't intend "
    "to add biases to all sensors.";
const char kVerifyHostEndpointDisconnectsUnique[] =
    "[***INVALID DATA***]: All host endpoint disconnects must be unique.";

// the different types of data the simulator may send to the CHRE framework.
enum DataType {
  kNone,
  kTimer,
  kMessageFromHost,
  kGnssLocation,
  kGnssMeasurement,
  kWwanCellInfo,
  kWifiScan,
  kWifiRanging,
  kSensor,
  kBiasEvent,
  kBle,
  kBleRssi,
  kRequestWifiScanConfiguration,
  kRequestWwanScanConfiguration,
  kHostEndpointDisconnect,
  kUserSettingEvent,
};

// DataRequestParams are populated by the PAL and forwarded to the simulator's
// RequestNewDataLocked function. The PALs are only expected to fill in the
// relevant fields. The relevant fields will then be inferred based on DataType.
struct DataRequestParams {
  uint64_t min_interval_ms = 0;
  uint64_t min_time_to_next_fix_ms = 0;
  SafeChreWifiScanParams *wifi_scan_params = nullptr;
  SafeChreWifiRangingParams *wifi_ranging_params = nullptr;
  SafeChreBleScanFilter *ble_scan_filter = nullptr;
  uint32_t sensor_index = 0;
  uint64_t latency_ns = 0;
};

// ScheduledData contains information about when the simulator should send data,
// and its type. By crossreferencing with the relevant LatestControlParams, we
// can ascertain everything about rhe relevant request.
struct ScheduledData {
  uint64_t delivery_time_ns;
  DataType type;
  uint32_t sensor_index;  // used only with sensors.
};

struct TimerTriggerData {
  uint64_t trigger_time;
  std::function<void()> callback;
};

// contains the parameters received with the latests call to a 'control' api.
// enabled: specifies whether data of this particular type should be returned
//     to the nanoapp. This applies for both passive and generated data.
//     if set to false, the data could still return passive data if
//     passive_enabled is set, but it can't return generated data.
// passive_enabled: is set by any function that enabled/disabled passive data.
//     If set to true, this passive data of this type will be forwarded to the
//     nanoapp. If set to false, passive data could still be enabled if enabled
//     is set to true.
// oneshot: specifies whether this data type should be disabled immediately
//     following a data response. If set to false, a new ScheduledData will be
//     scheduled based on interval.
// interval: specifies how long we should wait, after sending a response, before
//     sending another one. Ignored if oneshot is true. If oneshot is false but
//     interval is 0, then no further scheduling will be done, but the data type
//     will remain enabled (useful for always-on type of requests).
// next_expected_delivery: specifies when we expect the next ScheduledData to be
//     returned. Passive data is always returned regardless of timestamp.
//     Generated data's timestamp has to match the next_expected_delivery,
//     otherwise it's ignored (under the assumption that the data was already
//     returned hence the modification of this field).
// latency: Used by sensor and BLE requests. In a sensor request, the interval
//     specifies how often to query for data. The latency specifies how long
//     we can wait before returning the results. Results obtained every interval
//     are batched together and sent every latency. In BLE requests corresponds
//     to reportDelayMs.
// with_flush_id: unique to sensor requests. Is ignored if set to 0. If not set
//     to 0, then this sensor data is due to a flush, and we we should call
//     flushCompleteCallback after delivering the data.
struct LatestControlParams {
  bool enabled;
  bool passive_enabled;
  bool oneshot;
  uint32_t interval;
  uint64_t next_expected_delivery;
  uint64_t latency;
  uint32_t with_flush_id;
  SafeChreBleScanFilter *ble_scan_filter = nullptr;
};

// ScheduledDataComp is a helper class to allow sorting ScheduledData
struct ScheduledDataComp {
  bool operator()(const ScheduledData &tr1, const ScheduledData &tr2) {
    return tr1.delivery_time_ns > tr2.delivery_time_ns;
  }
};

class Simulator {
 public:
  // singleton functions.
  static Simulator *GetInstance();
  static void ResetInstance();

  // the simulator version to differentiate it from other platforms.
  static constexpr int kSimulatorVersion = 42;

  // verifies that the data object is valid, ie that any functions that we
  // expect to be overridden (based on capabilities) are overridden and that
  // messages to host are non-empty unless explicitly specified as empty.
  static bool VerifyValidData(DataFeedBase *data);

  // sets the data feed object after verifying if it's valid. Returns true if
  // the object is valid, false otherwise.
  bool InitializeDataFeed(DataFeedBase *data);

  // starts the simulator by initializing the chre and platform. This function
  // holds until the scenario/simulation finishes.
  void Run(std::string nanoapps);

  // Adds a PAL request to the simulator's verification data.
  void AddNanoappPlatformRequest(uint64_t timestamp,
                                 NanoappRequestType request_type);

  // Returns a copy of the PAL requests received by the simulator.
  std::vector<std::pair<uint64_t, NanoappRequestType>>
  GetNanoappPlatformRequests();

  // public access to the callbacks so they may be set by the platform
  // implementations. They are raw pointers as the core is responsible for
  // memory management.
  // These do not have to be mutex locked as they're set only once with the
  // platform initialization and all subsequent memory access are read-access.
  const struct chrePalBleCallbacks *ble_callbacks_;
  const struct chrePalGnssCallbacks *gnss_callbacks_;
  const struct chrePalWwanCallbacks *wwan_callbacks_;
  const struct chrePalWifiCallbacks *wifi_callbacks_;
  const struct chrePalSensorCallbacks *sensor_callbacks_;

  // when the power monitor is notified of a change, it subsequently notifies us
  // so that we can move to the next point in time.
  // Exported for testing.
  void AllEventsProcessed();

  uint64_t GetCurrentTime();

  // This mutex guards all variables that could be accessed/set at the same time
  absl::Mutex guard_;

  // tracks the current "point in time". This is in ns.
  std::atomic<uint64_t> current_time_;
  uint64_t time_since_epoch_;

  // the queue of all scheduled read events.
  std::priority_queue<ScheduledData, std::vector<ScheduledData>,
                      ScheduledDataComp>
      queue_ ABSL_GUARDED_BY(guard_);

  // maps each type of data with the most parameters from the most recent fired
  // control function for that particular data.
  std::map<DataType, LatestControlParams> data_to_control_;

  // does the same as above, but this is exclusive to sensors since we treat
  // each individual sensor as its own "DataType".
  std::map<uint32_t, LatestControlParams> sensor_data_to_control_;

  // same as above but for bias information.
  std::map<uint32_t, LatestControlParams> bias_data_to_control_;

  // contains the parameters for the more complex requests.
  std::unique_ptr<SafeChreBleScanFilter> ble_scan_filter_;
  std::unique_ptr<SafeChreWifiScanParams> wifi_scan_params_;
  std::unique_ptr<SafeChreWifiRangingParams> wifi_ranging_params_;

  // contains the pointers that have been sent to the nanoapps but not deleted
  // yet. On instance reset, or release event, all pointers in here are freed.
  // Some of these pointers are sub-structs within the main 'returned' structs.
  std::unique_ptr<SafeChreBleAdvertisementEvent>
      ble_advertisement_event_container_;
  std::unique_ptr<SafeChreGnssLocationEvent> gnss_location_container_;
  std::unique_ptr<SafeChreGnssDataEvent> gnss_data_event_container_;
  std::unique_ptr<SafeChreWwanCellInfoResult> wwan_cell_info_container_;
  std::unique_ptr<SafeChreWifiScanEvent> wifi_scan_event_container_;
  std::unique_ptr<SafeChreWifiRangingEvent> wifi_ranging_event_container_;
  std::unique_ptr<SafeChreSensorSamplingStatus> sampling_status_container_;
  std::unique_ptr<SafeChreGetSensorsResponse> get_sensors_response_container_;
  std::map<uint32_t, std::unique_ptr<SafeChreSensorDataRaw>> sensor_container_;
  std::map<uint32_t, std::unique_ptr<SafeChreBiasEvent>> bias_container_;

  // these "temp" containers are where responses are stored before being sent to
  // the CHRE. Pointers in here can be changed as needed, whereas the ones in
  // the above containers are only reset once a "release" function call is
  // received from CHRE.
  std::unique_ptr<SafeChreGnssLocationEvent> temp_gnss_location_container_;
  std::unique_ptr<SafeChreGnssDataEvent> temp_gnss_data_event_container_;
  std::unique_ptr<SafeChreWwanCellInfoResult> temp_wwan_cell_info_container_;
  std::unique_ptr<SafeChreWifiScanEvent> temp_wifi_scan_event_container_;
  std::unique_ptr<SafeChreWifiRangingEvent> temp_wifi_ranging_event_container_;
  std::map<uint32_t, std::unique_ptr<SafeChreSensorData>>
      temp_sensor_container_;
  std::unique_ptr<SafeChreBleAdvertisementEvent>
      temp_ble_advertisement_event_container_;
  uint32_t current_flush_id_ = 0;

  // the different public apis that will be returned to the core chre.
  std::unique_ptr<chrePalBleApi> chre_pal_ble_api_ ABSL_GUARDED_BY(guard_);
  std::unique_ptr<chrePalGnssApi> chre_pal_gnss_api_ ABSL_GUARDED_BY(guard_);
  std::unique_ptr<chrePalWwanApi> chre_pal_wwan_api_ ABSL_GUARDED_BY(guard_);
  std::unique_ptr<chrePalWifiApi> chre_pal_wifi_api_ ABSL_GUARDED_BY(guard_);
  std::unique_ptr<chrePalSensorApi> chre_pal_sensor_api_
      ABSL_GUARDED_BY(guard_);

  // only set to true right before finished_ is incremented. When dying_ is true
  // we will refuse all requests and not send any event data back.
  bool dying_;

  // exports the received host messages in a format more known to the CHRE
  // developers. Check the documentation of received_messages_ below for more
  // information.
  std::vector<std::pair<uint64_t, chreMessageToHostData>>
  GetReceivedHostMessages();

  // public access to allow adding to received_messages_. This method will allow
  // the simulator class to take control of the msg, and will thus be
  // responsible for freeing the memory later.
  void AddHostMessage(SafeChreMessageToHostData *msg);

  std::map<int8_t, std::vector<SafeChreMessageFromHostData>>
      received_host_message_fragments_;

  // provides a source for all data that we will read.
  DataFeedBase *data_source_;

  // all_timer_trigger_data_ maps the different initialized timers by "id" to
  // the time they're intended to trigger and the callback they're intended to
  // call. Whenever a timer trigger event is found on top of the queue by the
  // simulator, it scans this map for all timers that should trigger at that
  // time (as there could be multiple at the same time).
  absl::node_hash_map<void *, TimerTriggerData> all_timer_trigger_data_;

  // called by the PAL and the simulator run loop to call one of the
  // ReceivedRequest<...>AtTime form the DataFeed, as well as manage the result
  // and placing it into the queue.
  void RequestNewDataLocked(DataType type, const DataRequestParams &params);

  // returns whether the WiFi PAL is available.
  bool GetRequestWifiScanAvailable();

  // returns whether the Wwan PAL is available.
  bool GetRequestWwanScanAvailable();

  // TODO(b/356932419): Get rid of this. This is used because simulator testing
  // does not actually run the intended control flow. It tracks nanoapp requests
  // by calling the PAL directly instead of going through the control flow.
  void SetNanoappLoadedForTest(bool loaded);

 private:
  // Simulator is a singleton. It can only be constructed via GetInstance.
  Simulator();
  static Simulator *sim_instance_;

  // moves to the next "point in time"
  void MoveToNextTime();

  // the Run function blocks on the finished_ counter. When it's incremented
  // for the first time (after no more data is available), the Run will be able
  // to conclude.
  absl::BlockingCounter finished_;

  // returns the event data back to the nanoapp at the specified time for the
  // specified DataType.
  void SendEventDataBack(DataType type, uint64_t time_ns, bool is_passive,
                         uint32_t sensor_index);

  // takes in a pointer to a ScheduledData and populates it with the next
  // instance of ScheduledData across all passive data maps.
  // if no such data is available, the function does not modify data.
  // Only returns non-consumed data.
  void GetNextPassiveScheduledData(ScheduledData &data);

  // marks the ScheduledData as consumed so that it will not be returned by the
  // above Get function.
  void ConsumePassiveScheduledData(const ScheduledData &data);

  // returns whether there are any remaning unconsumed passive scheduled data.
  bool UnconsumedPassiveScheduledDataExist();

  void MaybeConnectEndpoint(uint16_t host_endpoint);

  // Whether the nanoapps are loaded.
  bool nanoapps_loaded_ = false;

  // contains all messages received from the nanoapps, wrapped in a safe class.
  // each element of received_messages_ is a pair, containing the time when the
  // message was received, along with the message. The message is wrapped in a
  // safe class to guarantee safe pointer deletion.
  std::vector<std::pair<uint64_t, std::unique_ptr<SafeChreMessageToHostData>>>
      received_messages_;

  // contains data about the PAL requests received from the nanoapps.
  std::vector<std::pair<uint64_t, NanoappRequestType>>
      nanoapp_requests_received_;

  // a map from each different DataType to the time at which the last consumed
  // passive message is. Since we consume passive data as a function of
  // increasing time, the latest is necessarily also the max.
  std::map<DataType, uint64_t> time_last_consumed_;
  std::map<int, uint64_t> bias_last_consumed_;

  int8_t next_outgoing_message_id_ = 0;

  bool wifi_scan_available_ = true;
  bool wwan_scan_available_ = true;

  // All currently connected host endpoints.
  absl::flat_hash_set<uint16_t> connected_host_endpoints_;
  // All host endpoints that have been disconnected.
  absl::flat_hash_set<uint16_t> disconnected_host_endpoints_;
};

}  // namespace lbs::contexthub::testing

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_PLATFORM_SIMULATOR_H_