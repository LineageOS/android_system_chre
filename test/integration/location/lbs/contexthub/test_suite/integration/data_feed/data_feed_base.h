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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_DATA_FEED_DATA_FEED_BASE_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_DATA_FEED_DATA_FEED_BASE_H_

#include <chre.h>

#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "chre/util/time.h"
#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"

namespace lbs::contexthub::testing {

const uint64_t kMillisToNano = chre::kOneMillisecondInNanoseconds;
const uint64_t kSecsToNano = chre::kOneSecondInNanoseconds;

/*
 * DataFeedBase is an abstract class that is intended to be derived. The class
 * serves as the source of all data and host messages in the CHRE integration
 * testing framework. It consists of 4 major components:
 * - Contains structures to allow sending messages to the CHRE core (via
 *     messages_to_send_).
 * - Allows setting the capabilities of the different APIs (ex:
 *     GetCapabilitiesGnss).
 * - Allows for the implementation of the API calls the CHRE core will make (ex:
 *     ReceivedGnssLocationEventRequestAtTime), to which the input is the time t
 * at which this request will have been made.
 * - Helper functions that generate empty structures for the data that will be
 *     returned (ex: EmptyChreGnssLocationEvent).
 *
 * More details about these components can be found in the field/method
 * comments.
 */
class DataFeedBase {
 public:
  DataFeedBase() = default;
  virtual ~DataFeedBase();

  // pairs of <timestamp, message_to_send_to_chre_core> that are read by the
  // simulator. Changing the contents during the simulation will correctly
  // influence the simulator behaviour, and any additions/changes to messages
  // scheduled after the "current" time will be correctly reflected.
  // Due to the nature of how the CHRE consumes these messages, the contents of
  // the SafeChreMessageFromHostData could be externally modified.
  std::map<uint64_t, SafeChreMessageFromHostData> messages_to_chre_;

  // the simulator will verify that at least one message exists in
  // messages_to_chre_, and throws an error if there is none. setting
  // skip_initial_message_from_host_ will bypass that requirement. This is done
  // so that the test author explicitly messages_to_chre_ or sets
  // skip_initial_message_from_host_ to avoid "hidden" side-effects.
  bool skip_initial_message_from_host_ = false;

  // called by the simulator whenever a message is received from the CHRE core.
  virtual void ReceivedMessageFromNanoapp(
      uint64_t t_ns, const SafeChreMessageToHostData &message);

  // these functions should provide the correct sensor data for the
  // tests. They are virtual functions intended to be overridden. The relevant
  // API functions (based on GetCapabilities return values), must be overridden
  // to provide correct functionality.

  // additionally, there are <timestamp, safe_return_structure> maps that allow
  // scheduling passive data to be sent for many of data types. The pointers in
  // these maps must be initialized via new or GetEmpty<>() functions. After
  // being added to the map, the DataFeed class will handle all memory
  // management, so the objects should not be deleted manually.

  // ble capabilities found here:
  // chre_api/include/chre_api/chre/ble.h
  virtual uint32_t GetCapabilitiesBle() = 0;
  virtual uint32_t GetFilterCapabilitiesBle() = 0;
  virtual SafeChreBleAdvertisementEvent *absl_nullable
  ReceivedBleAdvertisementEventRequestAtTime(
      uint64_t t_ns, uint64_t latency, const SafeChreBleScanFilter &filter);
  virtual std::optional<chreBleReadRssiEvent> ReceivedBleRssiRequestAtTime(
      uint64_t t_ns, uint16_t connectionHandle);

  // gnss capabilities found here:
  // chre_api/include/chre_api/chre/gnss.h
  virtual uint32_t GetCapabilitiesGnss() = 0;
  virtual SafeChreGnssLocationEvent *absl_nullable
  ReceivedGnssLocationEventRequestAtTime(uint64_t t_ns,
                                         uint32_t min_interval_ms,
                                         uint32_t min_time_to_next_fix_ms);
  virtual SafeChreGnssDataEvent *absl_nullable
  ReceivedGnssDataEventRequestAtTime(uint64_t t_ns, uint32_t min_interval_ms);

  std::map<uint64_t, SafeChreGnssLocationEvent *> gnss_location_events_;
  std::map<uint64_t, SafeChreGnssDataEvent *> gnss_data_events_;

  // wwan capabilities found here:
  // chre_api/include/chre_api/chre/wwan.h
  virtual uint32_t GetCapabilitiesWwan() = 0;
  virtual SafeChreWwanCellInfoResult *absl_nullable
  ReceivedWwanCallInfoResultRequestAtTime(uint64_t t_ns);

  // wifi capabilities found here:
  // chre_api/include/chre_api/chre/wifi.h
  virtual uint32_t GetCapabilitiesWifi() = 0;
  // When returning nullptr, the simulator will send a failure event to the
  // nanoapp.
  virtual SafeChreWifiScanEvent *absl_nullable
  ReceivedWifiScanEventRequestAtTime(uint64_t t_ns,
                                     const SafeChreWifiScanParams &params);
  virtual SafeChreWifiRangingEvent *absl_nullable
  ReceivedWifiRangingEventRequestAtTime(
      uint64_t t_ns, const SafeChreWifiRangingParams &params);

  std::map<uint64_t, SafeChreWifiScanEvent *> wifi_scan_events_;

  // This map is used to toggle the availability of chreWifiRequestScanAsync. By
  // default, availability is set to true. The availability is set for the rest
  // of the simulation or until the next toggle by setting the value at a
  // certain time.
  std::map<uint64_t, bool> wifi_scan_available_events_;

  // This map is used to toggle the availability of
  // chreWwanRequestCellInfoAsync. By default, availability is set to true. The
  // availability is set for the rest of the simulation or until the next toggle
  // by setting the value at a certain time.
  std::map<uint64_t, bool> wwan_scan_available_events_;

  // Broadcast user setting events to the nanoapp. The key is the time and
  // the value is a pair of <setting, enabled>.
  std::map<uint64_t, std::pair<uint8_t, bool>> setting_events_;

  // return 0 for no sensor support, n > 0 for sensor support.
  virtual uint32_t GetSensorCount() = 0;
  virtual std::vector<chreSensorInfo> GetSensors();
  virtual SafeChreSensorSamplingStatus *absl_nullable GetSamplingStatusUpdate(
      uint64_t t_ns, uint32_t sensor_info_index, uint64_t requested_interval_ns,
      uint64_t requested_latency_ns);
  virtual SafeChreSensorData *absl_nullable
  ConfigureSensor(uint64_t t_ns, uint32_t sensor_info_index, bool is_oneshot,
                  uint64_t interval_ns, uint64_t latency_ns);
  std::vector<std::map<uint64_t, SafeChreBiasEvent *>> sensor_bias_events_;

  // return 0 for no audio support, n > 0 for audio support.
  virtual uint32_t GetAudioSourceCount() = 0;

  // the below functions exist for convenience: calling any of them will return
  // an empty data structure. In some cases, t is required as an argument and
  // the function will populate the timestamp field with t.
  SafeChreBleAdvertisementEvent *absl_nullable EmptyChreBleAdvertisementEvent();
  SafeChreGnssLocationEvent *absl_nullable
  EmptyChreGnssLocationEvent(uint64_t t_ns);
  SafeChreGnssDataEvent *absl_nullable EmptyChreGnssDataEvent(uint64_t t_ns);

  SafeChreWwanCellInfoResult *absl_nullable EmptyChreWwanCellInfoResult();

  SafeChreWifiScanEvent *absl_nullable EmptyChreWifiScanEvent(uint64_t t_ns);
  SafeChreWifiRangingEvent *absl_nullable EmptyChreWifiRangingEvent();

  // Represents a list of times that the host endpoint is disconnected along
  // with the id of the endpoint.
  std::map<uint64_t, uint16_t> host_endpoint_disconnects_;
};

}  // namespace lbs::contexthub::testing

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_INTEGRATION_DATA_FEED_DATA_FEED_BASE_H_