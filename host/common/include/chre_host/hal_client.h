/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef CHRE_HOST_HAL_CLIENT_H_
#define CHRE_HOST_HAL_CLIENT_H_

#include <future>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <aidl/android/hardware/contexthub/BnContextHubCallback.h>
#include <aidl/android/hardware/contexthub/ContextHubMessage.h>
#include <aidl/android/hardware/contexthub/HostEndpointInfo.h>
#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <aidl/android/hardware/contexthub/IContextHubCallback.h>
#include <aidl/android/hardware/contexthub/NanoappBinary.h>
#include <android-base/thread_annotations.h>
#include <android-base/threads.h>
#include <android/binder_process.h>
#include <utils/SystemClock.h>

#include "hal_error.h"

namespace android::chre {

using aidl::android::hardware::contexthub::AsyncEventType;
using aidl::android::hardware::contexthub::BnContextHubCallback;
using aidl::android::hardware::contexthub::ContextHubInfo;
using aidl::android::hardware::contexthub::ContextHubMessage;
using aidl::android::hardware::contexthub::EndpointId;
using aidl::android::hardware::contexthub::EndpointInfo;
using aidl::android::hardware::contexthub::HostEndpointInfo;
using aidl::android::hardware::contexthub::HubInfo;
using aidl::android::hardware::contexthub::IContextHub;
using aidl::android::hardware::contexthub::IContextHubCallback;
using aidl::android::hardware::contexthub::IContextHubDefault;
using aidl::android::hardware::contexthub::IEndpointCallback;
using aidl::android::hardware::contexthub::IEndpointCommunication;
using aidl::android::hardware::contexthub::MessageDeliveryStatus;
using aidl::android::hardware::contexthub::NanoappBinary;
using aidl::android::hardware::contexthub::NanoappInfo;
using aidl::android::hardware::contexthub::NanSessionRequest;
using aidl::android::hardware::contexthub::Service;
using aidl::android::hardware::contexthub::Setting;
using ndk::ScopedAStatus;

/**
 * A class connecting to CHRE Multiclient HAL via binder and taking care of
 * binder (re)connection.
 *
 * <p>HalClient will replace the SocketClient that does the similar
 * communication with CHRE but through a socket connection.
 *
 * <p>HalClient also maintains a set of connected host endpoints, using which
 * it will enforce in the future that a message can only be sent to/from an
 * endpoint id that is already connected to HAL.
 *
 * <p>When the binder connection to HAL is disconnected HalClient will have a
 * death recipient re-establish the connection and reconnect the previously
 * connected endpoints. In a rare case that CHRE also restarts at the same time,
 * a client should rely on IContextHubCallback.handleContextHubAsyncEvent() to
 * handle the RESTARTED event which is a signal that CHRE is up running.
 */
class HalClient {
 public:
  static constexpr int32_t kDefaultContextHubId = 0;

  /** Callback interface for a background connection. */
  class BackgroundConnectionCallback {
   public:
    /**
     * This function is called when the connection to CHRE HAL is finished.
     *
     * @param isConnected indicates whether CHRE HAL is successfully connected.
     */
    virtual void onInitialization(bool isConnected) = 0;
    virtual ~BackgroundConnectionCallback() = default;
  };

  /**
   * A builder class to facilitate the creation of EndpointInfo objects.
   *
   * This class provides a fluent interface for constructing an EndpointInfo
   * object step-by-step. It simplifies the process by setting default values
   * for optional fields and allowing method chaining.
   *
   * Usage:
   * 1. Construct an EndpointInfoBuilder with the mandatory EndpointId and
   * name. Please refer to EndpointId.aidl for details about endpoint ids.
   *    - The `hubId` within the EndpointId is expected to be statically
   *      defined and globally unique, identifying a specific session-based
   *      messaging hub.
   *    - The `endpointId` within the EndpointId is expected to be statically
   *      defined and unique *within the scope of its hub*, identifying a
   *      specific endpoint (e.g., a nanoapp, a specific host client, etc.).
   * 2. Optionally call setter methods like `setVersion()`, `setTag()`, etc., to
   * configure the optional details. These methods return a reference to the
   * builder, allowing chaining.
   * 3. Call `build()` to obtain the final, configured EndpointInfo object.
   */
  class EndpointInfoBuilder {
   public:
    EndpointInfoBuilder(const EndpointId &id, const std::string &name) {
      mEndpointInfo.id = id;
      mEndpointInfo.name = name;
      mEndpointInfo.type = EndpointInfo::EndpointType::NATIVE;
      mEndpointInfo.version = 0;
      mEndpointInfo.tag = std::nullopt;
    }

    EndpointInfoBuilder &setVersion(const int32_t &version) {
      mEndpointInfo.version = version;
      return *this;
    }

    EndpointInfoBuilder &setTag(const std::string &tag) {
      mEndpointInfo.tag = tag;
      return *this;
    }

    EndpointInfoBuilder &addRequiredPermission(const std::string &permission) {
      mEndpointInfo.requiredPermissions.push_back(permission);
      return *this;
    }

    EndpointInfoBuilder &addService(const Service &service) {
      mEndpointInfo.services.push_back(service);
      return *this;
    }

    [[nodiscard]] EndpointInfo build() const {
      return mEndpointInfo;
    }

   private:
    EndpointInfo mEndpointInfo;
  };

  ~HalClient();

  /**
   * Create a HalClient unique pointer used to communicate with CHRE HAL.
   *
   * @param callback a non-null callback.
   * @param contextHubId context hub id that only 0 is supported at this moment.
   *
   * @return null pointer if the creation fails.
   */
  static std::unique_ptr<HalClient> create(
      const std::shared_ptr<IContextHubCallback> &callback,
      int32_t contextHubId = kDefaultContextHubId);

  /** Returns true if this HalClient instance is connected to the HAL. */
  bool isConnected() {
    return mIsHalConnected;
  }

  /** Connects to CHRE HAL synchronously and returns true if successful. */
  bool connect();

  /** Connects to CHRE HAL in background. */
  void connectInBackground(BackgroundConnectionCallback &callback) {
    std::lock_guard lock(mBackgroundConnectionFuturesLock);
    // Policy std::launch::async is required to avoid lazy evaluation which can
    // postpone the execution until get() of the future returned by std::async
    // is called.
    mBackgroundConnectionFutures.emplace_back(std::async(
        std::launch::async, [&] { callback.onInitialization(connect()); }));
  }

  ScopedAStatus queryNanoapps() {
    return callIfConnected([&](const std::shared_ptr<IContextHub> &hub) {
      return hub->queryNanoapps(mContextHubId);
    });
  }

  /** Sends a message to a Nanoapp. */
  ScopedAStatus sendMessage(const ContextHubMessage &message);

  /** Connects a host endpoint to CHRE. */
  ScopedAStatus connectEndpoint(const HostEndpointInfo &hostEndpointInfo);

  /** Disconnects a host endpoint from CHRE. */
  ScopedAStatus disconnectEndpoint(char16_t hostEndpointId);

  /** Registers a new hub for endpoint communication. */
  ScopedAStatus registerEndpointHub(
      const std::shared_ptr<IEndpointCallback> &callback,
      const HubInfo &hubInfo,
      std::shared_ptr<IEndpointCommunication> *communication) {
    return callIfConnected(
        [&](const std::shared_ptr<IContextHub> &contextHubHal) {
          return contextHubHal->registerEndpointHub(callback, hubInfo,
                                                    communication);
        });
  }

  /** Lists all the hubs, including the Context Hub and generic hubs. */
  ScopedAStatus getHubs(std::vector<HubInfo> *hubs) {
    return callIfConnected(
        [&](const std::shared_ptr<IContextHub> &contextHubHal) {
          return contextHubHal->getHubs(hubs);
        });
  }

  /** Lists all the endpoints, including the Context Hub nanoapps and generic
   * endpoints. */
  ScopedAStatus getEndpoints(std::vector<EndpointInfo> *endpoints) {
    return callIfConnected(
        [&](const std::shared_ptr<IContextHub> &contextHubHal) {
          return contextHubHal->getEndpoints(endpoints);
        });
  }

 protected:
  /**
   * @brief Callback interface for asynchronous communication with the CHRE HAL.
   *
   * Actual implementations of interface IContextHubCallback are provided by the
   * host clients using @code HalClient. Because IContextHubCallback is NOT
   * oneway, a client must make sure these callbacks return quickly, otherwise
   * they may block other clients from running their callbacks. A watchdog is
   * launched to enforce this requirement once ContextHub HAL is connected.
   */
  class HalClientCallback : public BnContextHubCallback {
   public:
    explicit HalClientCallback(
        const std::shared_ptr<IContextHubCallback> &callback,
        HalClient *halClient)
        : mCallback(callback), mHalClient(halClient) {}

    ScopedAStatus handleNanoappInfo(
        const std::vector<NanoappInfo> &appInfo) override {
      TimeRefresher refresher(mHalClient, __func__);
      return mCallback->handleNanoappInfo(appInfo);
    }

    ScopedAStatus handleContextHubMessage(
        const ContextHubMessage &msg,
        const std::vector<std::string> &msgContentPerms) override {
      TimeRefresher refresher(mHalClient, __func__);
      return mCallback->handleContextHubMessage(msg, msgContentPerms);
    }

    ScopedAStatus handleContextHubAsyncEvent(AsyncEventType event) override {
      TimeRefresher refresher(mHalClient, __func__);
      if (event == AsyncEventType::RESTARTED) {
        tryReconnectEndpoints(mHalClient);
      }
      return mCallback->handleContextHubAsyncEvent(event);
    }

    ScopedAStatus handleTransactionResult(int32_t transactionId,
                                          bool success) override {
      TimeRefresher refresher(mHalClient, __func__);
      return mCallback->handleTransactionResult(transactionId, success);
    }

    ScopedAStatus handleNanSessionRequest(
        const NanSessionRequest &request) override {
      TimeRefresher refresher(mHalClient, __func__);
      return mCallback->handleNanSessionRequest(request);
    }

    ScopedAStatus handleMessageDeliveryStatus(
        char16_t hostEndPointId,
        const MessageDeliveryStatus &messageDeliveryStatus) override {
      TimeRefresher refresher(mHalClient, __func__);
      return mCallback->handleMessageDeliveryStatus(hostEndPointId,
                                                    messageDeliveryStatus);
    }

    ScopedAStatus getUuid(std::array<uint8_t, 16> *outUuid) override {
      TimeRefresher refresher(mHalClient, __func__);
      return mCallback->getUuid(outUuid);
    }

    ScopedAStatus getName(std::string *outName) override {
      TimeRefresher refresher(mHalClient, __func__);
      return mCallback->getName(outName);
    }

   private:
    class TimeRefresher {
     public:
      explicit TimeRefresher(HalClient *halClient, const char *funcName)
          : mHalClient(halClient) {
        halClient->updateWatchdogSnapshot(funcName, elapsedRealtime());
        halClient->mWatchdogCv.notify_one();
      }
      ~TimeRefresher() {
        mHalClient->updateWatchdogSnapshot(/*funcName=*/nullptr,
                                           /* timeMs= */ 0);
      }

     private:
      HalClient *mHalClient;
    };
    std::shared_ptr<IContextHubCallback> mCallback;
    HalClient *mHalClient;
  };

  explicit HalClient(const std::shared_ptr<IContextHubCallback> &callback,
                     int32_t contextHubId = kDefaultContextHubId);

  /**
   * Initializes the connection to CHRE HAL.
   */
  HalError initConnection();

  using HostEndpointId = char16_t;

  const std::string kAidlServiceName =
      std::string() + IContextHub::descriptor + "/default";

  /** The callback for a disconnected HAL binder connection. */
  static void onHalDisconnected(void *cookie);

  /** Reconnect previously connected endpoints after CHRE or HAL restarts. */
  static void tryReconnectEndpoints(HalClient *halClient);

  ScopedAStatus callIfConnected(
      const std::function<
          ScopedAStatus(const std::shared_ptr<IContextHub> &hub)> &func) {
    std::shared_ptr<IContextHub> hub;
    {
      // Make a copy of mContextHub so that even if HAL is disconnected and
      // mContextHub is set to null the copy is kept as non-null to avoid crash.
      // Still guard the copy by a shared lock to avoid torn writes.
      std::shared_lock sharedLock(mConnectionLock);
      hub = mContextHub;
    }
    if (hub == nullptr) {
      return fromHalError(HalError::BINDER_DISCONNECTED);
    }
    return func(hub);
  }

  bool isEndpointConnected(HostEndpointId hostEndpointId) {
    std::shared_lock sharedLock(mConnectedEndpointsLock);
    return mConnectedEndpoints.find(hostEndpointId) !=
           mConnectedEndpoints.end();
  }

  void insertConnectedEndpoint(const HostEndpointInfo &hostEndpointInfo) {
    std::lock_guard lockGuard(mConnectedEndpointsLock);
    mConnectedEndpoints[hostEndpointInfo.hostEndpointId] = hostEndpointInfo;
  }

  void removeConnectedEndpoint(HostEndpointId hostEndpointId) {
    std::lock_guard lockGuard(mConnectedEndpointsLock);
    mConnectedEndpoints.erase(hostEndpointId);
  }

  static ScopedAStatus fromHalError(HalError errorCode) {
    return errorCode == HalError::SUCCESS
               ? ScopedAStatus::ok()
               : ScopedAStatus::fromServiceSpecificError(
                     static_cast<int32_t>(errorCode));
  }

  /**
   * A watchdog task that monitors the time spent by a single callback call.
   *
   * @param timeThreshold time threshold to trigger the action.
   * @param action action to take when the timeThreshold is triggered.
   */
  void watchdogTask(
      std::chrono::milliseconds timeThreshold,
      std::function<void(const char *funcName, uint64_t tid)> action);

  void updateWatchdogSnapshot(const char *funcName, const int64_t timeMs) {
    std::lock_guard lock(mWatchdogMutex);
    mCallbackFunctionName = funcName;
    mCallbackTimestamp = timeMs;
    mCallbackThreadId = timeMs != 0 ? base::GetThreadId() : 0;
  }

  void logFatalClientCallbackTimeout(const char *funcName, uint64_t tid) const;

  // The mutex guarding the construction and destruction of the watchdog.
  // Because it's possible that a single HalClient instance is shared among
  // multiple threads of the same client, we must make sure the watchdog task is
  // only created once.
  std::mutex mWatchdogCreationMutex;
  // The watchdog task handler.
  std::thread GUARDED_BY(mWatchdogCreationMutex) mWatchdogTask;

  // The mutex guarding the variables below.
  std::mutex mWatchdogMutex;
  // Wait on this cv until a callback comes in or the watchdog needs to stop.
  std::condition_variable mWatchdogCv;
  // A true value indicates that the watchdog task should be stopped.
  bool mStopWatchdog GUARDED_BY(mWatchdogMutex) = false;
  // Timestamp recorded by the last callback, where 0 indicates no callback is
  // currently being called.
  int64_t mCallbackTimestamp GUARDED_BY(mWatchdogMutex) = 0;
  // Name of the callback function triggering the watchdog.
  const char *mCallbackFunctionName GUARDED_BY(mWatchdogMutex) = nullptr;
  // Thread id of the callback.
  uint64_t mCallbackThreadId GUARDED_BY(mWatchdogMutex) = 0;

  // Multi-contextHub is not supported at this moment.
  int32_t mContextHubId;

  // The lock guarding mConnectedEndpoints.
  std::shared_mutex mConnectedEndpointsLock;
  std::unordered_map<HostEndpointId, HostEndpointInfo> mConnectedEndpoints{};

  // The lock guarding the init connection flow.
  std::shared_mutex mConnectionLock;
  std::shared_ptr<IContextHub> mContextHub;
  std::atomic_bool mIsHalConnected = false;

  // Handler of the binder disconnection event with HAL.
  ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;

  std::shared_ptr<HalClientCallback> mCallback;

  std::string mClientName;

  // Lock guarding background connection threads.
  std::mutex mBackgroundConnectionFuturesLock;
  std::vector<std::future<void>> mBackgroundConnectionFutures;
};

}  // namespace android::chre
#endif  // CHRE_HOST_HAL_CLIENT_H_