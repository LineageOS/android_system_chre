/*
 * Copyright (C) 2026 The Android Open Source Project
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

#pragma once

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

#include <aidl/android/hardware/contexthub/DataFlowAlertFds.h>
#include <aidl/android/hardware/contexthub/DataFlowId.h>
#include <aidl/android/hardware/contexthub/EndpointId.h>
#include <android-base/thread_annotations.h>
#include <android-base/unique_fd.h>

#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::hardware::contexthub::common::implementation {

using ::aidl::android::hardware::contexthub::DataFlowAlertFds;
using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::EndpointId;

/**
 * Helper which manages and performs epoll_wait() on all eventfds relating to
 * data flow alerts which are written to by host endpoints. This class is
 * thread-safe.
 */
class DataFlowEpollWaiter {
 public:
  /**
   * Callback interface used to deliver epoll events. Methods are called on the
   * internal thread and must be non-blocking.
   */
  class Callback {
   public:
    virtual ~Callback() = default;

    /**
     * Called to alert an embedded endpoint on the given data flow.
     *
     * @param dataFlowId The ID of the data flow
     * @param endpointId The ID of the embedded endpoint to alert
     * @param waking True if the alert is waking
     */
    virtual void onAlert(DataFlowId dataFlowId, EndpointId endpointId,
                         bool waking) = 0;

    /**
     * Called when a host endpoint acknowledges an alert wake-up.
     *
     * @param dataFlowId The ID of the data flow
     * @param endpointId The ID of the host endpoint acking waking alerts
     * @param wakeCount The number of wakeups being acked by the host endpoint
     * (i.e. the count read from the waking eventfd by the host endpoint)
     */
    virtual void onWakingAck(DataFlowId dataFlowId, EndpointId endpointId,
                             uint64_t wakeCount) = 0;

   protected:
    Callback() = default;
  };

  /**
   * Creates a new DataFlowEpollWaiter instance.
   *
   * @param callback The methods to be invoked on an epoll event. Must outlive
   * the created instance.
   * @return pw::Status::Internal() if we fail to create the instance, or
   * pw::Status::Ok() otherwise
   */
  static pw::Result<std::unique_ptr<DataFlowEpollWaiter>> create(
      Callback &callback);

  /** Cleans up all resources. */
  virtual ~DataFlowEpollWaiter();

  /**
   * Adds the appropriate epoll triggers for the given endpoint on a data flow.
   *
   * The alertFds determine whether this is a host or embedded endpoint based on
   * the presence of a halAck fd. If the endpoint is a host endpoint, a trigger
   * will be added for its halAck fd. If the endpoint is embedded, a trigger
   * will be added for its waking and nonWaking fds.
   *
   * @param dataFlowId The ID of the data flow
   * @param endpointId The ID of the endpoint
   * @param alertFds The eventfds associated with the endpoint
   * @return pw::Status::InvalidArgument() if the provided alertFds are invalid,
   * pw::Status::AlreadyExists() if triggers are already registered for the
   * given endpoint and data flow, or pw::Status::Ok() otherwise
   */
  pw::Status addTriggers(DataFlowId dataFlowId, EndpointId endpointId,
                         const DataFlowAlertFds &alertFds) EXCLUDES(mLock);

  /**
   * Removes the triggers associated with a data flow and/or endpoint.
   *
   * At least one of the parameters must be provided. If only the endpointId,
   * all triggers associated with that endpoint will be removed. If only the
   * dataFlowId, all triggers associated with that data flow will be removed. If
   * both are provided, the intersection will be removed.
   *
   * @param dataFlowId [optional] The ID of the data flow
   * @param endpointId [optional] The ID of the endpoint
   * @return pw::Status::InvalidArgument() if both arguments are empty,
   * pw::Status::NotFound() if no triggers are found for the given parameters,
   * or pw::Status::Ok() otherwise
   */
  pw::Status removeTriggers(std::optional<DataFlowId> dataFlowId,
                            std::optional<EndpointId> endpointId)
      EXCLUDES(mLock);

 protected:
  /** Stores the details of a registered epoll trigger. */
  struct Trigger {
    DataFlowId dataFlowId;
    EndpointId endpointId;
    // Only contains the fds that have an epoll trigger registered.
    DataFlowAlertFds alertFds;
  };

  DataFlowEpollWaiter(base::unique_fd epollFd, base::unique_fd haltFd,
                      Callback &callback);

  /** Main loop for the epoll thread. */
  void epollWaitLoop() EXCLUDES(mLock);

  /** Process a non-halt epoll event, invoking the appropriate callback. */
  void processEvent(int fd) EXCLUDES(mLock);

  base::unique_fd mEpollFd;
  base::unique_fd mHaltFd;
  Callback &mCallback;
  std::thread mEpollThread;

  std::mutex mLock;
  std::list<std::unique_ptr<Trigger>> mTriggers GUARDED_BY(mLock);
  std::unordered_map<int, Trigger *> mFdToTrigger GUARDED_BY(mLock);
  std::map<std::pair<DataFlowId, EndpointId>, Trigger *>
      mDataFlowEndpointToTrigger GUARDED_BY(mLock);
};

}  // namespace android::hardware::contexthub::common::implementation