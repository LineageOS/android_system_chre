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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_FAKE_DETECTOR_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_FAKE_DETECTOR_H_

#include <chre.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>

#include <gmock/gmock.h>
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_audio_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_ble_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_event_fake.h"
#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_sensor_fake.h"

namespace lbs {
namespace contexthub {

// Skeleton providing virtual declarations of all possible CHRE Api functions.
// Only exists so that ChreApiDetector can inherit from this class to provide
// mocks.
class ApiDetectorSkeleton {
 public:
  ApiDetectorSkeleton() = default;
  virtual ~ApiDetectorSkeleton() = default;

  // re+version functions.
  virtual void chreAbort(uint32_t abortCode) = 0;
  virtual uint32_t chreGetCapabilities() = 0;
  virtual uint32_t chreGetMessageToHostMaxSize() = 0;
  virtual uint64_t chreGetTime() = 0;
  virtual uint64_t chreGetAppId() = 0;
  virtual int64_t chreGetEstimatedHostTimeOffset() = 0;
  virtual uint32_t chreGetInstanceId() = 0;
  virtual uint32_t chreTimerSet(uint64_t duration, const void *cookie,
                                bool oneShot) = 0;
  virtual bool chreTimerCancel(uint32_t timerId) = 0;
  virtual void *chreHeapAlloc(uint32_t bytes) = 0;
  virtual void chreHeapFree(void *ptr) = 0;
  virtual void chreDebugDumpLog(const char *format_str, va_list arg_list) = 0;

  virtual uint32_t chreGetApiVersion() = 0;
  virtual uint32_t chreGetVersion() = 0;
  virtual uint64_t chreGetPlatformId() = 0;

  // gnss functions.
  virtual uint32_t chreGnssGetCapabilities() = 0;
  virtual bool chreGnssLocationSessionStartAsync(uint32_t minIntervalMs,
                                                 uint32_t minTimeToNextFixMs,
                                                 const void *cookie) = 0;
  virtual bool chreGnssLocationSessionStopAsync(const void *cookie) = 0;
  virtual bool chreGnssMeasurementSessionStartAsync(uint32_t minIntervalMs,
                                                    const void *cookie) = 0;
  virtual bool chreGnssMeasurementSessionStopAsync(const void *cookie) = 0;
  virtual bool chreGnssConfigurePassiveLocationListener(bool enable) = 0;

  // wifi functions.
  virtual uint32_t chreWifiGetCapabilities() = 0;
  virtual bool chreWifiConfigureScanMonitorAsync(bool enable,
                                                 const void *cookie) = 0;
  virtual bool chreWifiRequestScanAsync(const chreWifiScanParams *params,
                                        const void *cookie) = 0;
  virtual bool chreWifiRequestRangingAsync(const chreWifiRangingParams *params,
                                           const void *cookie) = 0;

  // audio functions.
  virtual bool chreAudioGetSource(uint32_t handle,
                                  AudioSource *audioSource) = 0;
  virtual bool chreAudioConfigureSource(uint32_t handle, bool enable,
                                        uint64_t bufferDuration,
                                        uint64_t deliveryInterval) = 0;
  virtual bool chreAudioGetStatus(uint32_t handle,
                                  AudioSourceStatus *status) = 0;

  // wwan functions.
  virtual uint32_t chreWwanGetCapabilities() = 0;
  virtual bool chreWwanGetCellInfoAsync(const void *cookie) = 0;

  // sensor functions.
  virtual bool chreSensorFindDefault(uint8_t sensorType, uint32_t *handle) = 0;
  virtual bool chreSensorFind(uint8_t sensorType, uint8_t sensorIndex,
                              uint32_t *handle) = 0;
  virtual bool chreGetSensorInfo(uint32_t sensorHandle, SensorInfo *info) = 0;
  virtual bool chreGetSensorSamplingStatus(uint32_t sensorHandle,
                                           SamplingStatus *status) = 0;
  virtual bool chreSensorConfigure(uint32_t sensorHandle,
                                   SensorConfigureMode mode, uint64_t interval,
                                   uint64_t latency) = 0;
  virtual bool chreSensorConfigureBiasEvents(uint32_t sensorHandle,
                                             bool enable) = 0;
  virtual bool chreSensorGetThreeAxisBias(uint32_t sensorHandle,
                                          ThreeAxisData *bias) = 0;
  virtual bool chreSensorFlushAsync(uint32_t sensorHandle,
                                    const void *cookie) = 0;

  // event and user_settings functions.
  virtual bool chreSendEvent(uint16_t eventType, void *eventData,
                             chreEventCompleteFunction *freeCallback,
                             uint32_t targetInstanceId) = 0;
  virtual bool chreSendMessageToHost(void *message, uint32_t messageSize,
                                     uint32_t messageType,
                                     chreMessageFreeFunction *freeCallback) = 0;
  virtual bool chreSendMessageToHostEndpoint(
      void *message, size_t messageSize, uint32_t messageType,
      uint16_t hostEndpoint, chreMessageFreeFunction *freeCallback) = 0;
  virtual bool chreSendMessageWithPermissions(
      void *message, size_t messageSize, uint32_t messageType,
      uint16_t hostEndpoint, uint32_t messagePermissions,
      chreMessageFreeFunction *freeCallback) = 0;
  virtual bool chreGetNanoappInfoByAppId(uint64_t appId,
                                         struct chreNanoappInfo *info) = 0;
  virtual bool chreGetNanoappInfoByInstanceId(uint32_t instanceId,
                                              struct chreNanoappInfo *info) = 0;
  virtual void chreConfigureNanoappInfoEvents(bool enable) = 0;
  virtual void chreConfigureHostSleepStateEvents(bool enable) = 0;
  virtual int8_t chreUserSettingGetState(uint8_t setting) = 0;
  virtual void chreUserSettingConfigureEvents(uint8_t setting, bool enable) = 0;
  virtual bool chreIsHostAwake() = 0;
  virtual void chreConfigureDebugDumpEvent(bool enable) = 0;
  virtual bool chreConfigureHostEndpointNotifications(uint16_t hostEndpointId,
                                                      bool enable) = 0;
  virtual bool chreGetHostEndpointInfo(uint16_t hostEndpointId,
                                       struct chreHostEndpointInfo *info) = 0;

  // ble functions.
  virtual uint32_t chreBleGetCapabilities() = 0;
  virtual uint32_t chreBleGetFilterCapabilities() = 0;
  virtual bool chreBleStartScanAsync(
      chreBleScanMode mode, uint32_t reportDelayMs,
      const struct chreBleScanFilter *filter) = 0;
  virtual bool chreBleStartScanAsyncV1_9(
      chreBleScanMode mode, uint32_t reportDelayMs,
      const struct chreBleScanFilterV1_9 *filter, const void *cookie) = 0;
  virtual bool chreBleStopScanAsync() = 0;
  virtual bool chreBleStopScanAsyncV1_9(const void *cookie) = 0;
  virtual bool chreBleFlushAsync(const void *cookie) = 0;
  virtual bool chreBleReadRssiAsync(uint16_t connectionHandle,
                                    const void *cookie) = 0;
  virtual bool chreBleGetScanStatus(struct chreBleScanStatus *status) = 0;
  virtual bool chreBleSocketAccept(uint64_t socketId) = 0;
  virtual int32_t chreBleSocketSend(
      uint64_t socketId, const void *data, uint16_t length,
      chreBleSocketPacketFreeFunction *freeCallback) = 0;
};

// ChreApiDetector is a class that mocks all CHRE API functions.
// The functions of this class are called whenever the fake api stubs are called
// ensuring that any EXPECT_CALL() check for chre API functions will succeed.
class ChreApiDetector : public ApiDetectorSkeleton {
 public:
  ChreApiDetector() = default;

  // re+version functions.
  MOCK_METHOD(void, chreAbort, (uint32_t), (override));
  MOCK_METHOD(uint32_t, chreGetCapabilities, (), (override));
  MOCK_METHOD(uint32_t, chreGetMessageToHostMaxSize, (), (override));
  MOCK_METHOD(uint64_t, chreGetTime, (), (override));
  MOCK_METHOD(int64_t, chreGetEstimatedHostTimeOffset, (), (override));
  MOCK_METHOD(uint64_t, chreGetAppId, (), (override));
  MOCK_METHOD(uint32_t, chreGetInstanceId, (), (override));
  MOCK_METHOD(uint32_t, chreTimerSet,
              (uint64_t duration, const void *cookie, bool one_shot),
              (override));
  MOCK_METHOD(bool, chreTimerCancel, (uint32_t timer_id), (override));
  MOCK_METHOD(void *, chreHeapAlloc, (uint32_t), (override));
  MOCK_METHOD(void, chreHeapFree, (void *ptr), (override));
  MOCK_METHOD(void, chreDebugDumpLog,
              (const char *format_str, va_list arg_list), (override));
  MOCK_METHOD(uint32_t, chreGetApiVersion, (), (override));
  MOCK_METHOD(uint32_t, chreGetVersion, (), (override));
  MOCK_METHOD(uint64_t, chreGetPlatformId, (), (override));

  // gnss functions.
  MOCK_METHOD(uint32_t, chreGnssGetCapabilities, (), (override));
  MOCK_METHOD(bool, chreGnssLocationSessionStartAsync,
              (uint32_t minIntervalMs, uint32_t minTimeToNextFixMs,
               const void *cookie),
              (override));
  MOCK_METHOD(bool, chreGnssLocationSessionStopAsync, (const void *cookie),
              (override));
  MOCK_METHOD(bool, chreGnssMeasurementSessionStartAsync,
              (uint32_t minIntervalMs, const void *cookie), (override));
  MOCK_METHOD(bool, chreGnssMeasurementSessionStopAsync, (const void *cookie),
              (override));
  MOCK_METHOD(bool, chreGnssConfigurePassiveLocationListener, (bool enable),
              (override));

  // wifi functions
  MOCK_METHOD(uint32_t, chreWifiGetCapabilities, (), (override));
  MOCK_METHOD(bool, chreWifiConfigureScanMonitorAsync,
              (bool enable, const void *cookie), (override));
  MOCK_METHOD(bool, chreWifiRequestScanAsync,
              (const chreWifiScanParams *params, const void *cookie),
              (override));
  MOCK_METHOD(bool, chreWifiRequestRangingAsync,
              (const chreWifiRangingParams *params, const void *cookie),
              (override));

  // audio functions.
  MOCK_METHOD(bool, chreAudioGetSource,
              (uint32_t handle, AudioSource *audioSource), (override));
  MOCK_METHOD(bool, chreAudioConfigureSource,
              (uint32_t handle, bool enable, uint64_t bufferDuration,
               uint64_t deliveryInterval),
              (override));
  MOCK_METHOD(bool, chreAudioGetStatus,
              (uint32_t handle, AudioSourceStatus *status), (override));

  // wwan functions.
  MOCK_METHOD(uint32_t, chreWwanGetCapabilities, (), (override));
  MOCK_METHOD(bool, chreWwanGetCellInfoAsync, (const void *cookie), (override));

  // sensor functions.
  MOCK_METHOD(bool, chreSensorFindDefault, (uint8_t, uint32_t *), (override));
  MOCK_METHOD(bool, chreSensorFind, (uint8_t, uint8_t, uint32_t *), (override));
  MOCK_METHOD(bool, chreGetSensorInfo, (uint32_t, SensorInfo *), (override));
  MOCK_METHOD(bool, chreGetSensorSamplingStatus, (uint32_t, SamplingStatus *),
              (override));
  MOCK_METHOD(bool, chreSensorConfigure,
              (uint32_t, SensorConfigureMode, uint64_t, uint64_t), (override));
  MOCK_METHOD(bool, chreSensorConfigureBiasEvents, (uint32_t, bool),
              (override));
  MOCK_METHOD(bool, chreSensorGetThreeAxisBias, (uint32_t, ThreeAxisData *),
              (override));
  MOCK_METHOD(bool, chreSensorFlushAsync, (uint32_t, const void *), (override));

  // event and user_settings functions.
  MOCK_METHOD(bool, chreSendEvent,
              (uint16_t, void *, chreEventCompleteFunction *, uint32_t),
              (override));
  MOCK_METHOD(bool, chreSendMessageToHost,
              (void *, uint32_t, uint32_t, MessageFreeFunction *), (override));
  MOCK_METHOD(bool, chreSendMessageToHostEndpoint,
              (void *, size_t, uint32_t, uint16_t, MessageFreeFunction *),
              (override));
  MOCK_METHOD(bool, chreSendMessageWithPermissions,
              (void *, size_t, uint32_t, uint16_t, uint32_t,
               MessageFreeFunction *),
              (override));
  MOCK_METHOD(bool, chreGetNanoappInfoByAppId, (uint64_t, NanoappInfo *),
              (override));
  MOCK_METHOD(bool, chreGetNanoappInfoByInstanceId, (uint32_t, NanoappInfo *),
              (override));
  MOCK_METHOD(void, chreConfigureNanoappInfoEvents, (bool), (override));
  MOCK_METHOD(void, chreConfigureHostSleepStateEvents, (bool), (override));
  MOCK_METHOD(int8_t, chreUserSettingGetState, (uint8_t), (override));
  MOCK_METHOD(void, chreUserSettingConfigureEvents, (uint8_t, bool),
              (override));
  MOCK_METHOD(bool, chreIsHostAwake, (), (override));
  MOCK_METHOD(void, chreConfigureDebugDumpEvent, (bool), (override));
  MOCK_METHOD(bool, chreConfigureHostEndpointNotifications, (uint16_t, bool),
              (override));
  MOCK_METHOD(bool, chreGetHostEndpointInfo, (uint16_t, HostEndpointInfo *),
              (override));

  // BLE functions
  MOCK_METHOD(uint32_t, chreBleGetCapabilities, (), (override));
  MOCK_METHOD(uint32_t, chreBleGetFilterCapabilities, (), (override));
  MOCK_METHOD(bool, chreBleStartScanAsync,
              (BleScanMode, uint32_t, const BleScanFilter *), (override));
  MOCK_METHOD(bool, chreBleStartScanAsyncV1_9,
              (chreBleScanMode, uint32_t, const chreBleScanFilterV1_9 *,
               const void *),
              (override));
  MOCK_METHOD(bool, chreBleStopScanAsync, (), (override));
  MOCK_METHOD(bool, chreBleStopScanAsyncV1_9, (const void *), (override));
  MOCK_METHOD(bool, chreBleFlushAsync, (const void *), (override));
  MOCK_METHOD(bool, chreBleReadRssiAsync, (uint16_t, const void *), (override));
  MOCK_METHOD(bool, chreBleGetScanStatus, (chreBleScanStatus *), (override));
  MOCK_METHOD(bool, chreBleSocketAccept, (uint64_t), (override));
  MOCK_METHOD(int32_t, chreBleSocketSend,
              (uint64_t, const void *, uint16_t,
               chreBleSocketPacketFreeFunction *),
              (override));
};

}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_FAKE_DETECTOR_H_
