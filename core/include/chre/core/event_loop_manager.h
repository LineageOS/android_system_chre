/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef CHRE_CORE_EVENT_LOOP_MANAGER_H_
#define CHRE_CORE_EVENT_LOOP_MANAGER_H_

#include "chre/core/audio_request_manager.h"
#include "chre/core/ble_request_manager.h"
#include "chre/core/ble_socket_manager.h"
#include "chre/core/chre_message_hub_manager.h"
#include "chre/core/debug_dump_manager.h"
#include "chre/core/event_loop.h"
#include "chre/core/gnss_manager.h"
#include "chre/core/host_comms_manager.h"
#include "chre/core/host_endpoint_manager.h"
#include "chre/core/host_message_hub_manager.h"
#include "chre/core/sensor_request_manager.h"
#include "chre/core/settings.h"
#include "chre/core/system_health_monitor.h"
#include "chre/core/telemetry_manager.h"
#include "chre/core/wifi_request_manager.h"
#include "chre/core/wwan_request_manager.h"
#include "chre/platform/atomic.h"
#include "chre/platform/memory_manager.h"
#include "chre/platform/mutex.h"
#include "chre/util/always_false.h"
#include "chre/util/fixed_size_vector.h"
#include "chre/util/non_copyable.h"
#include "chre/util/singleton.h"
#include "chre/util/system/system_callback_type.h"
#include "chre/util/unique_ptr.h"
#include "chre_api/chre/event.h"

#include <cstddef>

namespace chre {

template <typename T>
using TypedSystemEventCallbackFunction = void(SystemCallbackType type,
                                              UniquePtr<T> &&data);

// These Manager classes are forward declared because not every platform will
// support their implementation. If a platform does support their
// implementation, it must enable the corresponding build flag and pass in a
// non-null value to the EventLoopManager constructor.
class BleSocketManager;
class GnssManager;
class WifiRequestManager;
class WwanRequestManager;
class ChreMessageHubManager;
class HostMessageHubManager;

/**
 * A class that keeps track of all event loops in the system. This class
 * represents the top-level object in CHRE, providing a centralized access point
 * to the components that implement CHRE.
 *
 * NOTE: The platform implementation must perform initialization of this object
 * and its dependencies in this order:
 *
 *  1. SystemTime::init()
 *  2. Construct the *Manager objects accepted in the EventLoopManager
 *     constructor.
 *  3. EventLoopManagerSingleton::init()
 *  4. Start the thread that will run the EventLoop
 *
 * After this point, it is safe for other threads to access CHRE, e.g. incoming
 * requests from the host can be posted to the EventLoop. Then within the CHRE
 * thread:
 *
 *  5. EventLoopManager::lateInit() (this typically involves blocking on
 *     readiness of other subsystems as part of PAL initialization)
 *  6. loadStaticNanoapps()
 *  7. EventLoopManagerSingleton::get()->getEventLoop().run()
 *
 * Platforms may also perform additional platform-specific initialization steps
 * at any point along the way as needed.
 */
class EventLoopManager : public NonCopyable {
 public:
  EventLoopManager(BleSocketManager *bleSocketManager, GnssManager *gnssManager,
                   WifiRequestManager *wifiRequestManager,
                   WwanRequestManager *wwanRequestManager,
                   ChreMessageHubManager *chreMessageHubManager,
                   HostMessageHubManager *hostMessageHubManager)
      : mBleSocketManager(bleSocketManager),
        mGnssManager(gnssManager),
        mWifiRequestManager(wifiRequestManager),
        mWwanRequestManager(wwanRequestManager),
        mChreMessageHubManager(chreMessageHubManager),
        mHostMessageHubManager(hostMessageHubManager) {
#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED
    CHRE_ASSERT(mBleSocketManager != nullptr);
#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED
#ifdef CHRE_GNSS_SUPPORT_ENABLED
    CHRE_ASSERT(mGnssManager != nullptr);
#endif  // CHRE_GNSS_SUPPORT_ENABLED
#ifdef CHRE_WIFI_SUPPORT_ENABLED
    CHRE_ASSERT(mWifiRequestManager != nullptr);
#endif  // CHRE_WIFI_SUPPORT_ENABLED
#ifdef CHRE_WWAN_SUPPORT_ENABLED
    CHRE_ASSERT(mWwanRequestManager != nullptr);
#endif  // CHRE_WWAN_SUPPORT_ENABLED
#ifdef CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
    CHRE_ASSERT(mChreMessageHubManager != nullptr);
    CHRE_ASSERT(mHostMessageHubManager != nullptr);
#endif  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
  }

  /**
   * Validates that a CHRE API is invoked from a valid nanoapp context and
   * returns a pointer to the currently executing nanoapp. This should be
   * called by most CHRE API methods that require accessing details about the
   * event loop or the nanoapp itself. If the current event loop or nanoapp are
   * null, this is an assertion error.
   *
   * @param functionName The name of the CHRE API. This should be __func__.
   * @param eventLoop Optional output parameter, which will be populated with
   *        the EventLoop that is currently executing if this function is
   *        successful
   * @return A pointer to the currently executing nanoapp or null if outside
   *         the context of a nanoapp.
   */
  static Nanoapp *validateChreApiCall(const char *functionName);

  /**
   * Leverages the event queue mechanism to schedule a CHRE system callback to
   * be invoked at some point in the future from within the context of the
   * "main" EventLoop. Which EventLoop is considered to be the "main" one is
   * currently not specified, but it is required to be exactly one EventLoop
   * that does not change at runtime.
   *
   * This function is safe to call from any thread.
   *
   * @param type An identifier for the callback, which is passed through to the
   *        callback as a uint16_t, and can also be useful for debugging
   * @param data Arbitrary data to provide to the callback
   * @param callback Function to invoke from within the main CHRE thread
   * @param extraData Additional arbitrary data to provide to the callback
   * @return If true, the callback was deferred successfully; false otherwise.
   */
  bool deferCallback(SystemCallbackType type, void *data,
                     SystemEventCallbackFunction *callback,
                     void *extraData = nullptr) {
    return mEventLoop.postSystemEvent(static_cast<uint16_t>(type), data,
                                      callback, extraData);
  }

  /**
   * Alternative version of deferCallback which accepts a UniquePtr for the data
   * passed to the callback. This overload helps ensure that type continuity is
   * maintained with the callback, and also helps to ensure that the memory is
   * not leaked, including when CHRE is shutting down.
   *
   * Safe to call from any thread.
   *
   * @param type An identifier for the callback, which is passed through as
   *        uint16_t, and can also be useful for debugging
   * @param data Pointer to arbitrary data to provide to the callback
   * @param callback Function to invoke from within the main CHRE thread
   * @return If true, the callback was deferred successfully; false otherwise.
   */
  template <typename T>
  bool deferCallback(SystemCallbackType type, UniquePtr<T> &&data,
                     TypedSystemEventCallbackFunction<T> *callback) {
    auto outerCallback = [](uint16_t callbackType, void *eventData,
                            void *extraData) {
      // Re-wrap eventData in UniquePtr so its destructor will get called and
      // the memory will be freed once we leave this scope
      UniquePtr<T> dataWrapped = UniquePtr<T>(static_cast<T *>(eventData));
      auto *innerCallback =
          reinterpret_cast<TypedSystemEventCallbackFunction<T> *>(extraData);
      innerCallback(static_cast<SystemCallbackType>(callbackType),
                    std::move(dataWrapped));
    };
    // Pass the "inner" callback (the caller's callback) through to the "outer"
    // callback using the extraData parameter. Note that we're leveraging the
    // C++11 ability to cast a function pointer to void*
    bool status = mEventLoop.postSystemEvent(
        static_cast<uint16_t>(type), data.get(), outerCallback,
        reinterpret_cast<void *>(callback));
    if (status) {
      data.release();
    }
    return status;
  }

  //! Override that allows passing a lambda for the callback
  template <typename T, typename LambdaT>
  bool deferCallback(SystemCallbackType type, UniquePtr<T> &&data,
                     LambdaT callback) {
    return deferCallback(
        type, std::move(data),
        static_cast<TypedSystemEventCallbackFunction<T> *>(callback));
  }

  //! Disallows passing a null callback, as we don't include a null check in the
  //! outer callback to reduce code size. Note that this doesn't prevent the
  //! caller from passing a variable which is set to nullptr at runtime, but
  //! generally the callback is always known at compile time.
  template <typename T>
  void deferCallback(SystemCallbackType /*type*/, UniquePtr<T> && /*data*/,
                     std::nullptr_t /*callback*/) {
    static_assert(AlwaysFalse<T>::value,
                  "deferCallback(SystemCallbackType, UniquePtr<T>, nullptr) is "
                  "not allowed");
  }

  /**
   * Schedules a CHRE system callback to be invoked at some point in the future
   * after a specified amount of time, in the context of the "main" CHRE
   * EventLoop.
   *
   * This function is safe to call from any thread.
   *
   * @param type An identifier for the callback, which is passed through to the
   *        callback as a uint16_t, and can also be useful for debugging
   * @param data Arbitrary data to provide to the callback
   * @param callback Function to invoke from within the main CHRE event loop -
   *        note that extraData is always passed back as nullptr
   * @param delay The delay to postpone posting the event
   * @return TimerHandle of the requested timer.
   *
   * @see deferCallback
   */
  TimerHandle setDelayedCallback(SystemCallbackType type, void *data,
                                 SystemEventCallbackFunction *callback,
                                 Nanoseconds delay) {
    return mEventLoop.getTimerPool().setSystemTimer(delay, callback, type,
                                                    data);
  }

  /**
   * Cancels a delayed callback previously scheduled by setDelayedCallback.
   *
   * This function is safe to call from any thread.
   *
   * @param timerHandle The TimerHandle returned by setDelayedCallback
   *
   * @return true if the callback was successfully cancelled
   */
  bool cancelDelayedCallback(TimerHandle timerHandle) {
    return mEventLoop.getTimerPool().cancelSystemTimer(timerHandle);
  }

  /**
   * Returns a guaranteed unique instance identifier to associate with a newly
   * constructed nanoapp.
   *
   * @return a unique instance ID
   */
  uint16_t getNextInstanceId();

#ifdef CHRE_AUDIO_SUPPORT_ENABLED
  /**
   * @return A reference to the audio request manager. This allows interacting
   *         with the audio subsystem and manages requests from various
   *         nanoapps.
   */
  AudioRequestManager &getAudioRequestManager() {
    return mAudioRequestManager;
  }
#endif  // CHRE_AUDIO_SUPPORT_ENABLED

#ifdef CHRE_BLE_SUPPORT_ENABLED
  /**
   * @return A reference to the ble request manager. This allows interacting
   *         with the ble subsystem and manages requests from various
   *         nanoapps.
   */
  BleRequestManager &getBleRequestManager() {
    return mBleRequestManager;
  }
#endif  // CHRE_BLE_SUPPORT_ENABLED

  /**
   * @return A reference to the BLE socket manager. This allows interacting
   *         with the BLE socket subsystem and manages requests from various
   *         nanoapps.
   */
  BleSocketManager &getBleSocketManager() {
    return *mBleSocketManager;
  }

  /**
   * @return The event loop managed by this event loop manager.
   */
  EventLoop &getEventLoop() {
    return mEventLoop;
  }

  /**
   * @return A reference to the GNSS request manager. This allows interacting
   *         with the platform GNSS subsystem and manages requests from various
   *         nanoapps.
   */
  GnssManager &getGnssManager() {
    return *mGnssManager;
  }

  /**
   * @return A reference to the host communications manager that enables
   *         transferring arbitrary data between the host processor and CHRE.
   */
  HostCommsManager &getHostCommsManager() {
    return mHostCommsManager;
  }

  HostEndpointManager &getHostEndpointManager() {
    return mHostEndpointManager;
  }

#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  /**
   * @return Returns a reference to the sensor request manager. This allows
   *         interacting with the platform sensors and managing requests from
   *         various nanoapps.
   */
  SensorRequestManager &getSensorRequestManager() {
    return mSensorRequestManager;
  }
#endif  // CHRE_SENSORS_SUPPORT_ENABLED

  /**
   * @return Returns a reference to the wifi request manager. This allows
   *         interacting with the platform wifi subsystem and manages the
   *         requests from various nanoapps.
   */
  WifiRequestManager &getWifiRequestManager() {
    return *mWifiRequestManager;
  }

  /**
   * @return A reference to the WWAN request manager. This allows interacting
   *         with the platform WWAN subsystem and manages requests from various
   *         nanoapps.
   */
  WwanRequestManager &getWwanRequestManager() {
    return *mWwanRequestManager;
  }

  /**
   * @return A reference to the memory manager. This allows central control of
   *         the heap space allocated by nanoapps.
   */
  MemoryManager &getMemoryManager() {
    return mMemoryManager;
  }

  /**
   * @return A reference to the debug dump manager. This allows central control
   *         of the debug dump process.
   */
  DebugDumpManager &getDebugDumpManager() {
    return mDebugDumpManager;
  }

#ifdef CHRE_TELEMETRY_SUPPORT_ENABLED
  /**
   * @return A reference to the telemetry manager.
   */
  TelemetryManager &getTelemetryManager() {
    return mTelemetryManager;
  }
#endif  // CHRE_TELEMETRY_SUPPORT_ENABLED

  /**
   * @return A reference to the setting manager.
   */
  SettingManager &getSettingManager() {
    return mSettingManager;
  }

  SystemHealthMonitor &getSystemHealthMonitor() {
    return mSystemHealthMonitor;
  }

  ChreMessageHubManager &getChreMessageHubManager() {
    return *mChreMessageHubManager;
  }

  HostMessageHubManager &getHostMessageHubManager() {
    return *mHostMessageHubManager;
  }

  /**
   * Performs second-stage initialization of things that are not necessarily
   * required at construction time but need to be completed prior to executing
   * any nanoapps.
   */
  void lateInit();

 private:
  //! The instance ID generated by getNextInstanceId().
  AtomicUint32 mNextInstanceId{kSystemInstanceId + 1};

#ifdef CHRE_AUDIO_SUPPORT_ENABLED
  //! The audio request manager handles requests for all nanoapps and manages
  //! the state of the audio subsystem that the runtime subscribes to.
  AudioRequestManager mAudioRequestManager;
#endif

#ifdef CHRE_BLE_SUPPORT_ENABLED
  //! The BLE request manager handles requests for all nanoapps and manages
  //! the state of the BLE subsystem that the runtime subscribes to.
  BleRequestManager mBleRequestManager;
#endif  // CHRE_BLE_SUPPORT_ENABLED

  //! The BLE socket manager tracks offloaded sockets and handles sending
  //! packets between nanoapps and offloaded sockets.
  BleSocketManager *mBleSocketManager = nullptr;

  //! The event loop managed by this event loop manager.
  EventLoop mEventLoop;

  //! The GnssManager that handles requests for all nanoapps. This manages the
  //! state of the GNSS subsystem that the runtime subscribes to.
  GnssManager *mGnssManager = nullptr;

  //! Handles communications with the host processor.
  HostCommsManager mHostCommsManager;

  HostEndpointManager mHostEndpointManager;

  SystemHealthMonitor mSystemHealthMonitor;

#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  //! The SensorRequestManager that handles requests for all nanoapps. This
  //! manages the state of all sensors that runtime subscribes to.
  SensorRequestManager mSensorRequestManager;
#endif  // CHRE_SENSORS_SUPPORT_ENABLED

  //! The WifiRequestManager that handles requests for nanoapps. This manages
  //! the state of the wifi subsystem that the runtime subscribes to.
  WifiRequestManager *mWifiRequestManager = nullptr;

  //! The WwanRequestManager that handles requests for nanoapps. This manages
  //! the state of the WWAN subsystem that the runtime subscribes to.
  WwanRequestManager *mWwanRequestManager = nullptr;

  //! The MemoryManager that handles malloc/free call from nanoapps and also
  //! controls upper limits on the heap allocation amount.
  MemoryManager mMemoryManager;

  //! The DebugDumpManager that handles the debug dump process.
  DebugDumpManager mDebugDumpManager;

#ifdef CHRE_TELEMETRY_SUPPORT_ENABLED
  //! The TelemetryManager that handles metric collection/reporting.
  TelemetryManager mTelemetryManager;
#endif  // CHRE_TELEMETRY_SUPPORT_ENABLED

  //! The SettingManager that manages setting states.
  SettingManager mSettingManager;

  //! The ChreMessageHubManager that manages the CHRE Message Hub.
  ChreMessageHubManager *mChreMessageHubManager = nullptr;

  //! The HostMessageHubManager handling communication with host message hubs.
  HostMessageHubManager *mHostMessageHubManager = nullptr;
};

//! Provide an alias to the EventLoopManager singleton.
typedef Singleton<EventLoopManager> EventLoopManagerSingleton;

//! Extern the explicit EventLoopManagerSingleton to force non-inline method
//! calls. This reduces codesize considerably.
extern template class Singleton<EventLoopManager>;

#ifdef CHRE_SENSORS_SUPPORT_ENABLED
inline SensorRequestManager &getSensorRequestManager() {
  return EventLoopManagerSingleton::get()->getSensorRequestManager();
}
#endif  // CHRE_SENSORS_SUPPORT_ENABLED

}  // namespace chre

#endif  // CHRE_CORE_EVENT_LOOP_MANAGER_H_
