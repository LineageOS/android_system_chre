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

#include "chre/platform/platform_sensor_manager.h"
#include "chre/platform/android/looper.h"

#include "chre/core/event.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/log.h"
#include "chre/platform/shared/pal_system_api.h"
#include "chre_api/chre/sensor_types.h"

#include <android/sensor.h>
#include <dlfcn.h>
#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

namespace chre {
namespace {

// The function pointer to ASensorEventQueue_registerSensor.
// This function is not available in lower NDK API versions, but we need this
// function to be able to set the batching parameters for the sensor. So we need
// to use the dynamic linking to be able to load the function pointer at
// runtime. If the function is not available, we will just disable CHRE sensor
// processing by returning empty sensor list.
// Using dynamic finding for this function is mainly used to bypass some build
// systems that are strict to a lower NDK API version.
typedef int (*PFN_ASensorEventQueue_registerSensor)(
    ASensorEventQueue *queue, ASensor const *sensor, int32_t samplingPeriodUs,
    int64_t maxBatchReportLatencyUs);
static PFN_ASensorEventQueue_registerSensor gRegisterSensorFunc = nullptr;

const size_t MAX_EVENT_BUFFER_SIZE = 64;

// Maps Android NDK sensor accuracy status to CHRE accuracy enum.
static uint8_t mapAndroidAccuracyToChre(int32_t androidStatus) {
  switch (androidStatus) {
    case ASENSOR_STATUS_ACCURACY_HIGH:
      return CHRE_SENSOR_ACCURACY_HIGH;
    case ASENSOR_STATUS_ACCURACY_MEDIUM:
      return CHRE_SENSOR_ACCURACY_MEDIUM;
    case ASENSOR_STATUS_ACCURACY_LOW:
      return CHRE_SENSOR_ACCURACY_LOW;
    case ASENSOR_STATUS_UNRELIABLE:
    default:
      return CHRE_SENSOR_ACCURACY_UNRELIABLE;
  }
}

// Maps Android sensor type to CHRE sensor type.
// Returns std::nullopt if the sensor type is not supported.
static std::optional<uint8_t> mapAndroidToChreSensorType(
    int32_t androidSensorType) {
  switch (androidSensorType) {
    case ASENSOR_TYPE_ACCELEROMETER:
      return CHRE_SENSOR_TYPE_ACCELEROMETER;
    case ASENSOR_TYPE_ACCELEROMETER_UNCALIBRATED:
      return CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER;
    case ASENSOR_TYPE_PRESSURE:
      return CHRE_SENSOR_TYPE_PRESSURE;
    default:
      return std::nullopt;
  }
}

std::string sensorEventToString(const ASensorEvent &event) {
  std::ostringstream sstream;
  sstream << "ASensorEvent: version " << event.version << ", sensor "
          << event.sensor << ", type " << event.type << ", timestamp "
          << event.timestamp << ", flags " << event.flags
          << ", data[0]: " << event.data[0];
  return sstream.str();
}
}  // namespace

PlatformSensorManager::~PlatformSensorManager() {
  // Destroy event queue.
  if (mSensorManager != nullptr && mSharedEventQueue != nullptr) {
    ASensorManager_destroyEventQueue(mSensorManager, mSharedEventQueue);
    mSharedEventQueue = nullptr;
  }

  Looper::deinit();

  mSensorTypeToHandleMap.clear();
}

void PlatformSensorManager::init() {
  if (gRegisterSensorFunc == nullptr) {
    void *handle = dlopen("libandroid.so", RTLD_NOW);
    if (handle == nullptr) {
      LOGE("Failed to open libandroid.so: %s", dlerror());
      return;
    }
    gRegisterSensorFunc = (PFN_ASensorEventQueue_registerSensor)dlsym(
        handle, "ASensorEventQueue_registerSensor");
    if (gRegisterSensorFunc == nullptr) {
      LOGE("Failed to find ASensorEventQueue_registerSensor in libandroid.so");
      return;
    }
  }

  if (__builtin_available(android 26, *)) {
    mSensorManager = ASensorManager_getInstanceForPackage("");
  } else {
    mSensorManager = ASensorManager_getInstance();
  }

  mLooper = Looper::init();
  if (mLooper == nullptr) {
    LOGE("Failed to get main thread looper.");
    return;
  }

  // Create one shared event queue and pass `this` as data.
  mSharedEventQueue = ASensorManager_createEventQueue(
      mSensorManager, mLooper, 1 /* ident */, looperCallback, this);
  if (mSharedEventQueue == nullptr) {
    LOGE("Failed to create shared event queue");
    return;
  }

  // Retrieves sensor list.
  ASensorList sensorList = nullptr;
  int sensorCount = ASensorManager_getSensorList(mSensorManager, &sensorList);
  if (sensorCount <= 0) {
    LOGW("No sensors found on the device.");
    return;
  }
  // Inits sensors.
  uint8_t chreSensorHandle = 0;
  std::set<int32_t> initializedAndroidSensorTypes;
  for (int i = 0; i < sensorCount; ++i) {
    const ASensor *androidSensor = sensorList[i];
    const char *chreName = ASensor_getName(androidSensor);
    int32_t type = ASensor_getType(androidSensor);
    std::optional<uint8_t> chreType = mapAndroidToChreSensorType(type);

    // Skip unsupported sensors.
    if (chreType == std::nullopt) {
      continue;
    }
    // Skip sensors that are already initialized.
    // Each type of sensor is only initialized once.
    // TODO(b/445584823): Add support for multiple sensors of the same type.
    if (initializedAndroidSensorTypes.find(type) !=
        initializedAndroidSensorTypes.end()) {
      continue;
    }
    initializedAndroidSensorTypes.insert(type);

    uint64_t minIntervalNs =
        static_cast<uint64_t>(ASensor_getMinDelay(androidSensor) * 1000);
    int reportingMode = ASensor_getReportingMode(androidSensor);

    // Prepares sensor info.
    mSensorContextArray.push_back({
        .sensorInfo =
            {
                .sensorName = chreName,
                .sensorType = *chreType,
                .isOnChange = (reportingMode == AREPORTING_MODE_ON_CHANGE),
                .isOneShot = (reportingMode == AREPORTING_MODE_ONE_SHOT),
                .reportsBiasEvents = 0,
                .supportsPassiveMode = 0,
                .unusedFlags = 0,
                .minInterval = (minIntervalNs > 0)
                                   ? minIntervalNs
                                   : CHRE_SENSOR_INTERVAL_DEFAULT,
                .sensorIndex = 0,
            },
        .androidSensor = androidSensor,
        .androidSensorType = type,
        .chreSensorHandle = chreSensorHandle,
    });

    // Populate the map
    mSensorTypeToHandleMap[type] = chreSensorHandle;

    chreSensorHandle++;
  }
}

DynamicVector<Sensor> PlatformSensorManager::getSensors() {
  DynamicVector<Sensor> sensors;
  sensors.reserve(mSensorContextArray.size());
  for (int i = 0; i < mSensorContextArray.size(); ++i) {
    const struct chreSensorInfo *sensor = &mSensorContextArray[i].sensorInfo;
    sensors.push_back(Sensor());
    sensors[i].initBase(sensor, i /* sensorHandle */);
    if (sensor->sensorName != nullptr) {
      LOGD("Found sensor: %s", sensor->sensorName);
    } else {
      LOGD("Sensor at index %" PRIu32 " has type %" PRIu8, i,
           sensor->sensorType);
    }
  }
  return sensors;
}

bool PlatformSensorManager::configureSensor(Sensor &sensor,
                                            const SensorRequest &request) {
  if (sensor.getSensorHandle() >= mSensorContextArray.size()) {
    return false;
  }
  struct SensorContext *context =
      &mSensorContextArray[sensor.getSensorHandle()];
  if (context->androidSensor == nullptr) {
    return false;
  }
  chreSensorConfigureMode mode =
      getConfigureModeFromSensorMode(request.getMode());
  uint64_t intervalNs = request.getInterval().toRawNanoseconds();
  uint64_t latencyNs = request.getLatency().toRawNanoseconds();

  bool success = false;
  if (mode == CHRE_SENSOR_CONFIGURE_MODE_DONE) {
    if (context->enabled) {
      if (ASensorEventQueue_disableSensor(mSharedEventQueue,
                                          context->androidSensor) == 0) {
        context->enabled = false;
        success = true;
      }
    } else {
      success = true;
    }
  } else if (mode == CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS) {
    if (!context->enabled) {
      if (ASensorEventQueue_enableSensor(mSharedEventQueue,
                                         context->androidSensor) != 0) {
        return false;
      }
      context->enabled = true;
    }
    int32_t samplingRateUs =
        (intervalNs > 0) ? static_cast<int32_t>(intervalNs / 1000) : 0;
    int64_t latencyUs =
        (latencyNs > 0) ? static_cast<int64_t>(latencyNs / 1000) : 0;
    int result = gRegisterSensorFunc(mSharedEventQueue, context->androidSensor,
                                     samplingRateUs, latencyUs);
    if (result == 0) {
      context->intervalNs = intervalNs;
      context->latencyNs = latencyNs;
      success = true;
    } else {
      ASensorEventQueue_disableSensor(mSharedEventQueue,
                                      context->androidSensor);
      context->enabled = false;
    }
  } else {
    LOGE("Unsupported CHRE sensor mode: %d", mode);
    return false;
  }

  // Send status update on success.
  if (success) {
    struct chreSensorSamplingStatus status = {
        .interval = context->intervalNs,
        .latency = context->latencyNs,
        .enabled = context->enabled,
    };
    EventLoopManagerSingleton::get()
        ->getSensorRequestManager()
        .handleSamplingStatusUpdate(sensor.getSensorHandle(), &status);
  }
  return success;
}

bool PlatformSensorManager::configureBiasEvents(const Sensor & /*sensor*/,
                                                bool /*enable*/,
                                                uint64_t /*latencyNs*/) {
  // TODO(b/445584823): implement this.
  return false;
}

bool PlatformSensorManager::getThreeAxisBias(
    const Sensor & /*sensor*/,
    struct chreSensorThreeAxisData * /*bias*/) const {
  // TODO(b/445584823): implement this.
  return false;
}

bool PlatformSensorManager::flush(const Sensor & /*sensor*/,
                                  uint32_t * /*flushRequestId*/) {
  // TODO(b/445584823): implement this.
  return false;
}

uint16_t PlatformSensorManager::getTargetGroupId(
    const Nanoapp & /*nanoapp*/) const {
  // Target group IDs are not supported for this implementation of platform
  // sensors, so return kDefaultTargetGroupMask, indicating that there are no
  // required bits for any given nanoapp.
  return kDefaultTargetGroupMask;
}

void PlatformSensorManager::releaseSamplingStatusUpdate(
    struct chreSensorSamplingStatus * /*status*/) {
  // TODO(b/445584823): implement this.
}

void PlatformSensorManager::releaseSensorDataEvent(void *data) {
  chre::memoryFree(data);
}

void PlatformSensorManager::releaseBiasEvent(void *data) {
  chre::memoryFree(data);
}

void PlatformSensorManagerBase::fillAccelerometerEvent(
    const ASensorEvent &event, Event &chreEvent) {
  if (chreEvent.threeAxisData == nullptr) {
    // Alloc memory.
    size_t total_size =
        sizeof(struct chreSensorThreeAxisData) +
        sizeof(struct chreSensorThreeAxisData::chreSensorThreeAxisSampleData) *
            (chreEvent.dataSize - 1);
    chreEvent.threeAxisData =
        (struct chreSensorThreeAxisData *)malloc(total_size);
    // Fill in the header data based on the first sensor event.
    chreEvent.threeAxisData->header.sensorHandle =
        chreEvent.context->chreSensorHandle;
    chreEvent.threeAxisData->header.readingCount = chreEvent.dataSize;
    // Assuming that the first event has the earliest timestamp of all
    // events.
    chreEvent.threeAxisData->header.baseTimestamp = event.timestamp;
    chreEvent.threeAxisData->header.accuracy =
        event.type == ASENSOR_TYPE_ACCELEROMETER
            ? mapAndroidAccuracyToChre(event.acceleration.status)
            : CHRE_SENSOR_ACCURACY_UNRELIABLE;
    chreEvent.threeAxisData->header.reserved = 0;
    chreEvent.lastEventTimestamp = event.timestamp;
  }
  // Fill in data for accelerometer
  size_t index = chreEvent.currentIndex;
  ++chreEvent.currentIndex;
  chreEvent.threeAxisData->readings[index].timestampDelta =
      event.timestamp - chreEvent.lastEventTimestamp;
  chreEvent.lastEventTimestamp = event.timestamp;
  if (event.type == ASENSOR_TYPE_ACCELEROMETER) {
    chreEvent.threeAxisData->readings[index].v[0] = event.acceleration.x;
    chreEvent.threeAxisData->readings[index].v[1] = event.acceleration.y;
    chreEvent.threeAxisData->readings[index].v[2] = event.acceleration.z;
  } else if (event.type == ASENSOR_TYPE_ACCELEROMETER_UNCALIBRATED) {
    chreEvent.threeAxisData->readings[index].v[0] =
        event.uncalibrated_acceleration.x_uncalib;
    chreEvent.threeAxisData->readings[index].v[1] =
        event.uncalibrated_acceleration.y_uncalib;
    chreEvent.threeAxisData->readings[index].v[2] =
        event.uncalibrated_acceleration.z_uncalib;
  }
}

void PlatformSensorManagerBase::fillBarometerEvent(const ASensorEvent &event,
                                                   Event &chreEvent) {
  if (chreEvent.floatData == nullptr) {
    // Alloc memory.
    size_t total_size =
        sizeof(struct chreSensorFloatData) +
        sizeof(struct chreSensorFloatData::chreSensorFloatSampleData) *
            (chreEvent.dataSize - 1);
    chreEvent.floatData = (struct chreSensorFloatData *)malloc(total_size);
    // Fill in the header data based on the first sensor event.
    chreEvent.floatData->header.sensorHandle =
        chreEvent.context->chreSensorHandle;
    chreEvent.floatData->header.readingCount = chreEvent.dataSize;
    // Assuming that the first event has the earliest timestamp of all
    // events.
    chreEvent.floatData->header.baseTimestamp = event.timestamp;
    chreEvent.floatData->header.accuracy = CHRE_SENSOR_ACCURACY_UNRELIABLE;
    chreEvent.floatData->header.reserved = 0;
    chreEvent.lastEventTimestamp = event.timestamp;
  }
  // Fill in data for barometer
  size_t index = chreEvent.currentIndex;
  ++chreEvent.currentIndex;
  chreEvent.floatData->readings[index].timestampDelta =
      event.timestamp - chreEvent.lastEventTimestamp;
  chreEvent.lastEventTimestamp = event.timestamp;
  chreEvent.floatData->readings[index].pressure = event.pressure;
}

// NDK Looper callback function. Now a static member of the class.
int PlatformSensorManagerBase::looperCallback(int /*fd*/, int /*events*/,
                                              void *data) {
  // `data` is now a pointer to the PlatformSensorManager instance.
  PlatformSensorManager *manager = static_cast<PlatformSensorManager *>(data);
  std::array<ASensorEvent, MAX_EVENT_BUFFER_SIZE> eventBuffer;
  ssize_t numEvents = ASensorEventQueue_getEvents(
      manager->mSharedEventQueue, eventBuffer.data(), MAX_EVENT_BUFFER_SIZE);

  // Create chreEvent by type+sensor and count the sample data size of each
  // event.
  std::unordered_map<int32_t, Event> chreEventBySensor;
  for (ssize_t i = 0; i < numEvents; ++i) {
    const ASensorEvent &event = eventBuffer[i];
    // Find the corresponding CHRE sensor handle using the map.
    auto it = manager->mSensorTypeToHandleMap.find(event.type);
    if (it == manager->mSensorTypeToHandleMap.end()) {
      LOGW("Received event for unknown Android sensor type: %d, %s", event.type,
           sensorEventToString(event).c_str());
      continue;
    }
    // Increase the sample data size of this event group.
    Event &chreEvent = chreEventBySensor[event.sensor];
    if (chreEvent.context == nullptr) {
      uint32_t chreSensorHandle = it->second;
      chreEvent.context = &manager->mSensorContextArray[chreSensorHandle];
    }
    ++chreEvent.dataSize;
  }

  // Loop event buffer again to fill in the real chreEvent data.
  for (ssize_t i = 0; i < numEvents; ++i) {
    const ASensorEvent &event = eventBuffer[i];
    if (chreEventBySensor.find(event.sensor) == chreEventBySensor.end()) {
      LOGW("Received events from non-registered sensor: %ld, %s", event.sensor,
           sensorEventToString(event).c_str());
      continue;
    }
    Event &chreEvent = chreEventBySensor[event.sensor];
    switch (event.type) {
      case ASENSOR_TYPE_ACCELEROMETER:
      case ASENSOR_TYPE_ACCELEROMETER_UNCALIBRATED:
        fillAccelerometerEvent(event, chreEvent);
        break;
      case ASENSOR_TYPE_PRESSURE:
        fillBarometerEvent(event, chreEvent);
        break;
      default:
        LOGW("Received event for unsupported sensor type: %d", event.type);
        break;
    }
  }

  // Sends CHRE events
  for (const auto &elem : chreEventBySensor) {
    const Event &chreEvent = elem.second;
    EventLoopManagerSingleton::get()
        ->getSensorRequestManager()
        .handleSensorDataEvent(chreEvent.context->chreSensorHandle,
                               chreEvent.data);
  }

  // Return 1 to continue receiving callbacks.
  return 1;
}
}  // namespace chre
