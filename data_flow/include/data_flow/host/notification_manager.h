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

#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <android-base/thread_annotations.h>
#include <android-base/unique_fd.h>

#include "pw_function/function.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::contexthub::data_flow {

/** Handles sending and receiving notifications on data flow eventfds. */
class NotificationManager {
 public:
  /** Callback for a notification on a data flow this endpoint part of. */
  using NotificationCallback =
      pw::Function<void(::aidl::android::hardware::contexthub::DataFlowId)>;

  /** Handle for the eventfds associated with an endpoint on a data flow. */
  using NotificationDataHandle = void *;

  /** Interface for managing triggers on a thread looping on epoll_wait(). */
  class EpollWaiter {
   public:
    virtual ~EpollWaiter() = default;

    /** Registers NotificationManager instance on creation. */
    virtual void registerManager(NotificationManager &manager) = 0;

    /** Adds an epoll trigger on the given fd. */
    virtual void addFd(int fd) = 0;

    /** Removes an epoll trigger on the given fd. */
    virtual void removeFd(int fd) = 0;

   protected:
    EpollWaiter() = default;
  };

  /** Starts a background thread to handle incoming data flow notifications. */
  NotificationManager(std::unique_ptr<EpollWaiter> waiter,
                      NotificationCallback &&cb);

  /** Stops the background thread. */
  ~NotificationManager();

  /**
   * Prepares the eventfds for a new host producer data flow.
   *
   * @return On success, a handle for the pending eventfds will be associated
   * with a data flow after it is registered with the HAL, along with a
   * DataFlowInfo populated with dups of the eventfds. pw::Status::Internal() on
   * failure to set up the eventfds.
   */
  pw::Result<std::pair<::aidl::android::hardware::contexthub::DataFlowInfo,
                       NotificationDataHandle>>
  prepareHostProducerDataFlow() EXCLUDES(mLock);

  /**
   * Activates notifications for a new host producer data flow.
   *
   * @param id The HAL-generated id for the data flow.
   * @param handle The handle returned from setupHostProducerDataFlow().
   * @return pw::Status::NotFound() if the data flow is not known.
   * pw::Status::InvalidArgument() if the handle is invalid.
   * pw::Status::AlreadyExists() if the data flow is already active.
   */
  pw::Status activateHostProducerDataFlow(int id, NotificationDataHandle handle)
      EXCLUDES(mLock);

  /**
   * Stops notifications on the given data flow and cleans up the eventfds.
   *
   * @param id The id of the data flow to remove.
   * @return pw::Status::NotFound() if the data flow is not known.
   */
  pw::Status removeHostProducerDataFlow(int id) EXCLUDES(mLock);

  /**
   * Creates a consumer handle for a data flow produced by this endpoint.
   *
   * Initializes the eventfds for notifications to and from this consumer and
   * associates them so that they can be cleaned up when the offload endpoint is
   * pruned.
   *
   * @param dataFlow The data flow to create a consumer for.
   * @param consumer The offload endpoint to associate notifications with.
   * @return On success, a DataFlowConsumer populated only with eventfds.
   * pw::Status::NotFound() if the data flow is not known.
   */
  pw::Result<::aidl::android::hardware::contexthub::DataFlowConsumer>
  addOffloadConsumer(int dataFlow,
                     ::aidl::android::hardware::contexthub::EndpointId consumer)
      EXCLUDES(mLock);

  /**
   * Disables notification for an offload consumer and cleans up resources.
   *
   * @param consumer The id of the offload consumer to disable notification for.
   * @return pw::Status::NotFound() if the consumer is not known.
   */
  pw::Status removeOffloadConsumer(
      ::aidl::android::hardware::contexthub::EndpointId consumer)
      EXCLUDES(mLock);

  /**
   * Enables notifications for this endpoint on an offload producer data flow.
   *
   * @param consumer Contains the data flow id and eventfds to listen for
   * notifications and to send notifications on.
   * @return pw::Status::AlreadyExists() if this endpoint is already consuming
   * on a data flow with the id in consumer. pw::Status::InvalidArgument() if
   * the fds are not valid. pw::Status::Internal() on failure to enable
   * notifications.
   */
  pw::Status enableHostConsumer(
      ::aidl::android::hardware::contexthub::DataFlowConsumer &&consumer)
      EXCLUDES(mLock);

  /**
   * Disables notifications for this endpoint on an offload producer data flow.
   *
   * @param dataFlow The id of the data flow to disable notifications for.
   * @return pw::Status::NotFound() if the data flow is not known.
   */
  pw::Status disableHostConsumer(
      ::aidl::android::hardware::contexthub::DataFlowId dataFlow)
      EXCLUDES(mLock);

  /**
   * Sends an outgoing notification on a data flow.
   *
   * @param dataFlow The data flow to send a notification on.
   * @param waking Whether the notification should wake the other endpoint.
   * @return pw::Status::NotFound() if the data flow is not known.
   * pw::Status::Internal() if the notification fails.
   */
  pw::Status notify(::aidl::android::hardware::contexthub::DataFlowId dataFlow,
                    bool waking) EXCLUDES(mLock);

 private:
  /** Contains the data for handling notifications on one data flow. */
  struct NotificationData {
    ::aidl::android::hardware::contexthub::DataFlowId dataFlow;
    std::optional<::aidl::android::hardware::contexthub::EndpointId>
        offloadEndpoint;
    android::base::unique_fd waking;
    android::base::unique_fd nonWaking;
    android::base::unique_fd halAck;
  };

  /** Called by the EpollWaiter on an epoll event. */
  void handleNotification(int fd, int events) EXCLUDES(mLock);

  std::unique_ptr<EpollWaiter> mWaiter;
  NotificationCallback mNotifyCb;
  ::aidl::android::hardware::contexthub::EndpointId mEndpointId;

  std::mutex mLock;
  std::unordered_map<NotificationData *, std::unique_ptr<NotificationData>>
      mNotificationDataStorage GUARDED_BY(mLock);
  std::unordered_map<int, NotificationData *> mWaitFdToHandle GUARDED_BY(mLock);
  std::map<::aidl::android::hardware::contexthub::EndpointId,
           NotificationData *>
      mOffloadConsumerToHandle GUARDED_BY(mLock);
  std::map<::aidl::android::hardware::contexthub::DataFlowId,
           NotificationData *>
      mDataFlowToHandle GUARDED_BY(mLock);
  bool mDestroyed GUARDED_BY(mLock) = false;
};

}  // namespace android::contexthub::data_flow