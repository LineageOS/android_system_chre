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
#include <unordered_set>

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <android-base/thread_annotations.h>

#include "android/binder_auto_utils.h"
#include "pw_function/function.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::contexthub::data_flow {

/**
 * Does a deep-copy of the DataFlowAlertFds.
 *
 * @param fds The DataFlowAlertFds to dup.
 * @param needsHalAck Whether the DataFlowAlertFds needs a halAck fd. If
 * false, an empty ScopedFileDescriptor will be used.
 * @return On success, a DataFlowAlertFds with dups of the input fds.
 */
pw::Result<::aidl::android::hardware::contexthub::DataFlowAlertFds> dupEventFds(
    const ::aidl::android::hardware::contexthub::DataFlowAlertFds &fds,
    bool needsHalAck);

/** Handles sending and receiving notifications on data flow eventfds. */
class NotificationManager {
  struct NotificationData;

 public:
  /** Callback for a notification on a data flow this endpoint part of. */
  using NotificationCallback = pw::Function<void(
      ::aidl::android::hardware::contexthub::DataFlowId /*dataFlow*/,
      bool /*waking*/)>;

  /** Handle for the eventfds associated with an endpoint on a data flow. */
  using NotificationDataHandle = NotificationData *;

  /** Interface for managing triggers on a thread looping on epoll_wait(). */
  class EpollWaiter {
   public:
    virtual ~EpollWaiter() = default;

    /** Registers NotificationManager instance on creation. */
    void registerManager(std::weak_ptr<NotificationManager> manager) {
      mManager = manager;
    }

    /** Adds an epoll trigger for input events on the given fd. */
    virtual void addFd(int fd) = 0;

    /** Removes the epoll trigger on the given fd. */
    virtual void removeFd(int fd) = 0;

   protected:
    EpollWaiter() = default;

    /** Passes an epoll event into the NotificationManager instance if valid. */
    void handleNotification(int fd, bool error) {
      if (auto manager = mManager.lock(); manager) {
        manager->handleNotification(fd, error);
      }
    }

    std::weak_ptr<NotificationManager> mManager;
  };

  static std::shared_ptr<NotificationManager> create(
      std::unique_ptr<EpollWaiter> waiter, NotificationCallback &&cb) {
    auto manager = std::shared_ptr<NotificationManager>(
        new NotificationManager(std::move(waiter), std::move(cb)));
    manager->mWaiter->registerManager(manager);
    return manager;
  }

  /** Removes any outstanding epoll triggers. */
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
  prepareHostProducerDataFlowInfo() EXCLUDES(mLock);

  /** Version of prepareHostProducerDataFlowInfo() that returns just the
   * DataFlowAlertFds. */
  pw::Result<std::pair<::aidl::android::hardware::contexthub::DataFlowAlertFds,
                       NotificationDataHandle>>
  prepareHostProducerDataFlowEventFds() EXCLUDES(mLock);

  /** Discards the eventfds associated with the given handle.
   *
   * This should only be called if the data flow the eventfds were prepared for
   * was not successfully registered with the HAL.
   *
   * @param handle The handle returned from prepareHostProducerDataFlow().
   * @return pw::Status::NotFound() if the handle is not known.
   */
  pw::Status discardNotificationDataHandle(NotificationDataHandle handle)
      EXCLUDES(mLock);

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
   * @return On success, a DataFlowConsumerHandle populated only with eventfds.
   * pw::Status::NotFound() if the data flow is not known.
   */
  pw::Result<::aidl::android::hardware::contexthub::DataFlowSinkContext>
  addOffloadConsumerAndCreateHandle(
      int dataFlow, ::aidl::android::hardware::contexthub::EndpointId consumer)
      EXCLUDES(mLock);

  /**
   * Version of addOffloadConsumerAndCreateHandle() that returns just the
   * ::aidl::android::hardware::contexthub::DataFlowAlertFds.
   */
  pw::Result<::aidl::android::hardware::contexthub::DataFlowAlertFds>
  addOffloadConsumerAndGetEventFds(
      int dataFlow, ::aidl::android::hardware::contexthub::EndpointId consumer)
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
  pw::Status enableHostConsumerFromHandle(
      const ::aidl::android::hardware::contexthub::DataFlowSinkContext
          &consumer) EXCLUDES(mLock);

  /**
   * Version of enableHostConsumerFromHandle() that takes
   * DataFlowAlertFds instead of a DataFlowConsumerHandle.
   *
   * @param dataFlow The data flow the consumer will read from.
   * @param notifyHostFds The eventfds the host endpoint will listen for
   * notifications on. Must contain the halAck fd.
   * @param notifyOffloadFds The eventfds to send notifications to the offload
   * endpoint on.
   * @return pw::Status::AlreadyExists() if this endpoint is already consuming
   * on a data flow with the id in consumer. pw::Status::InvalidArgument() if
   * the fds are not valid. pw::Status::Internal() on failure to enable
   * notifications.
   */
  pw::Status enableHostConsumerFromEventFds(
      ::aidl::android::hardware::contexthub::DataFlowId dataFlow,
      ::aidl::android::hardware::contexthub::DataFlowAlertFds &&notifyHostFds,
      ::aidl::android::hardware::contexthub::DataFlowAlertFds
          &&notifyOffloadFds) EXCLUDES(mLock);

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
   * Sends an outgoing notification to an offload producer.
   *
   * @param dataFlow The data flow to send a notification on.
   * @param waking Whether the notification should wake the other endpoint.
   * @return pw::Status::NotFound() if the data flow is not known.
   * pw::Status::Internal() if the notification fails.
   */
  pw::Status notifyOffloadProducer(
      ::aidl::android::hardware::contexthub::DataFlowId dataFlow, bool waking)
      EXCLUDES(mLock);

  /**
   * Sends an outgoing notification to an offload consumer.
   *
   * @param consumer The offload consumer to send a notification to.
   * @param waking Whether the notification should wake the other endpoint.
   * @return pw::Status::NotFound() if the consumer is not known.
   * pw::Status::Internal() if the notification fails.
   */
  pw::Status notifyOffloadConsumer(
      ::aidl::android::hardware::contexthub::EndpointId consumer, bool waking)
      EXCLUDES(mLock);

 private:
  /** Contains the data for handling notifications on one data flow. */
  struct NotificationData {
    ::aidl::android::hardware::contexthub::DataFlowId dataFlow;
    std::optional<::aidl::android::hardware::contexthub::EndpointId>
        offloadEndpoint;
    ::aidl::android::hardware::contexthub::DataFlowAlertFds eventFds;
  };

  NotificationManager(std::unique_ptr<EpollWaiter> waiter,
                      NotificationCallback &&cb)
      : mWaiter(std::move(waiter)), mNotifyCb(std::move(cb)) {}

  /** Called by the EpollWaiter on an input or error epoll event. */
  void handleNotification(int fd, bool error) EXCLUDES(mLock);

  /** Enables waiting and maps the eventfds for a NotificationData. */
  void enableNotifications(NotificationData *data)
      EXCLUSIVE_LOCKS_REQUIRED(mLock);

  /** Disables waiting and unmaps the eventfds for a NotificationData.   */
  void disableNotifications(NotificationData *data)
      EXCLUSIVE_LOCKS_REQUIRED(mLock);

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
  // The mapped set contains the NotificationData* for receiving notifications
  // from consumers, as well as the NotificationData*s for notifying every
  // consumer.
  std::unordered_map<int, std::unordered_set<NotificationData *>>
      mHostDataFlowToHandles GUARDED_BY(mLock);
  // The first NotificationData* in the mapped pair is for notifying the
  // producer, the second is for waiting on notifications from the producer.
  std::map<::aidl::android::hardware::contexthub::DataFlowId,
           std::pair<NotificationData *, NotificationData *>>
      mOffloadDataFlowToHandles GUARDED_BY(mLock);
};

}  // namespace android::contexthub::data_flow