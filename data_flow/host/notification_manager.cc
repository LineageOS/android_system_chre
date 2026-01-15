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

#define LOG_TAG "DATA_FLOW.NotificationManager"

#include "data_flow/host/notification_manager.h"

#include <errno.h>
#include <inttypes.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>

#include <mutex>

#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <utils/Log.h>

#include "android/binder_auto_utils.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowAlertFds;
using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::DataFlowInfo;
using ::aidl::android::hardware::contexthub::DataFlowSinkContext;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::aidl::android::hardware::contexthub::HubInfo;

pw::Result<ndk::ScopedFileDescriptor> tryGetFdOp(int fd, int line) {
  if (fd < 0) {
    ALOGE("Failed to get fd with errno %d at line %d", errno, line);
    return pw::Status::Internal();
  }
  return ndk::ScopedFileDescriptor(fd);
}

pw::Result<DataFlowAlertFds> createEventFds(bool needsHalAck) {
  DataFlowAlertFds fds;
  PW_TRY_ASSIGN(fds.waking, tryGetFdOp(eventfd(0, EFD_NONBLOCK), __LINE__));
  PW_TRY_ASSIGN(fds.nonWaking, tryGetFdOp(eventfd(0, EFD_NONBLOCK), __LINE__));
  if (needsHalAck) {
    PW_TRY_ASSIGN(fds.halAck, tryGetFdOp(eventfd(0, EFD_NONBLOCK), __LINE__));
  }
  return fds;
}

pw::Status sendNotification(int fd) {
  const uint64_t kOne = 1;
  if (TEMP_FAILURE_RETRY(write(fd, &kOne, sizeof(kOne))) < 0) {
    return pw::Status::Internal();
  }
  return pw::OkStatus();
}

}  // namespace

pw::Result<DataFlowAlertFds> dupEventFds(const DataFlowAlertFds &fds,
                                         bool needsHalAck) {
  DataFlowAlertFds dupFds;
  PW_TRY_ASSIGN(dupFds.waking, tryGetFdOp(dup(fds.waking.get()), __LINE__));
  PW_TRY_ASSIGN(dupFds.nonWaking,
                tryGetFdOp(dup(fds.nonWaking.get()), __LINE__));
  if (needsHalAck) {
    PW_TRY_ASSIGN(dupFds.halAck, tryGetFdOp(dup(fds.halAck.get()), __LINE__));
  }
  return dupFds;
}

NotificationManager::~NotificationManager() {
  std::lock_guard lock(mLock);
  // Disable all existing epoll triggers.
  for (const auto &[fd, data] : mWaitFdToHandle) {
    mWaiter->removeFd(fd);
  }
}

pw::Result<std::pair<DataFlowInfo, NotificationManager::NotificationDataHandle>>
NotificationManager::prepareHostProducerDataFlowInfo() {
  PW_TRY_ASSIGN(auto fdsAndHandle, prepareHostProducerDataFlowEventFds());
  return std::make_pair(DataFlowInfo{.alertFds = std::move(fdsAndHandle.first)},
                        fdsAndHandle.second);
}

pw::Result<
    std::pair<DataFlowAlertFds, NotificationManager::NotificationDataHandle>>
NotificationManager::prepareHostProducerDataFlowEventFds() {
  // Create the eventfds. Then dup them to create the DataFlowInfo to send to
  // the service.
  PW_TRY_ASSIGN(auto fds, createEventFds(/*needsHalAck=*/true));
  PW_TRY_ASSIGN(auto dupFds, dupEventFds(fds, /*needsHalAck=*/true));
  // Map the eventfds after successfully duplicating them.
  auto data = std::make_unique<NotificationData>(
      NotificationData{.eventFds = std::move(fds)});
  auto *dataPtr = data.get();
  std::lock_guard lock(mLock);
  mNotificationDataStorage[dataPtr] = std::move(data);
  return std::make_pair(std::move(dupFds), dataPtr);
}

pw::Status NotificationManager::discardNotificationDataHandle(
    NotificationDataHandle handle) {
  std::lock_guard lock(mLock);
  auto it = mNotificationDataStorage.find(handle);
  if (it == mNotificationDataStorage.end()) {
    ALOGE("Attempted to discard unknown data flow");
    return pw::Status::NotFound();
  } else if (it->second->dataFlow.id != 0) {
    ALOGE("Attempted to discard active data flow");
    return pw::Status::FailedPrecondition();
  }
  mNotificationDataStorage.erase(it);
  return pw::OkStatus();
}

pw::Status NotificationManager::activateHostProducerDataFlow(
    int id, NotificationDataHandle handle) {
  std::lock_guard lock(mLock);
  auto it = mNotificationDataStorage.find(handle);
  if (it == mNotificationDataStorage.end()) {
    ALOGE("Attempted to activate unknown data flow");
    return pw::Status::NotFound();
  }
  auto &data = *it->second;
  if (mHostDataFlowToHandles.find(id) != mHostDataFlowToHandles.end()) {
    ALOGE("Attempted to activate duplicate data flow");
    return pw::Status::AlreadyExists();
  }
  // Link the data flow to the notification data.
  data.dataFlow = {.hubId = HubInfo::HUB_ID_INVALID, .id = id};
  mHostDataFlowToHandles[id].insert(&data);
  enableNotifications(&data);
  return pw::OkStatus();
}

pw::Status NotificationManager::removeHostProducerDataFlow(int id) {
  std::lock_guard lock(mLock);
  auto it = mHostDataFlowToHandles.find(id);
  if (it == mHostDataFlowToHandles.end()) {
    ALOGE("Attempted to remove unknown data flow");
    return pw::Status::NotFound();
  }
  // Loop through the producer and consumer eventfds to discard the eventfds.
  // For the producer eventfds, first disable the epoll waiting.
  for (auto *data : it->second) {
    if (!data->offloadEndpoint.has_value()) {
      disableNotifications(data);
    }
    // Discard the fds.
    mNotificationDataStorage.erase(data);
  }
  // Unmap the data flow.
  mHostDataFlowToHandles.erase(it);
  return pw::OkStatus();
}

pw::Result<::aidl::android::hardware::contexthub::DataFlowSinkContext>
NotificationManager::addOffloadConsumerAndCreateHandle(int dataFlow,
                                                       EndpointId consumer) {
  PW_TRY_ASSIGN(auto fds, addOffloadConsumerAndGetEventFds(dataFlow, consumer));
  return DataFlowSinkContext{.alertFds = std::move(fds)};
}

pw::Result<DataFlowAlertFds>
NotificationManager::addOffloadConsumerAndGetEventFds(int dataFlow,
                                                      EndpointId consumer) {
  std::lock_guard lock(mLock);
  auto it = mHostDataFlowToHandles.find(dataFlow);
  if (it == mHostDataFlowToHandles.end()) {
    ALOGE("Attempted to add consumer to unknown data flow");
    return pw::Status::NotFound();
  }
  for (auto *data : it->second) {
    if (data->offloadEndpoint && *data->offloadEndpoint == consumer) {
      ALOGE("Attempted to add duplicate consumer to data flow");
      return pw::Status::AlreadyExists();
    }
  }
  // Create and duplicate the eventfds. The halAck fd is not needed since this
  // endpoint will not be receiving notifications from the HAL on these fds.
  PW_TRY_ASSIGN(auto fds, createEventFds(/*needsHalAck=*/false));
  PW_TRY_ASSIGN(auto dupFds, dupEventFds(fds, /*needsHalAck=*/false));
  auto data = std::make_unique<NotificationData>(NotificationData{
      .dataFlow = {.hubId = HubInfo::HUB_ID_INVALID, .id = dataFlow},
      .offloadEndpoint = consumer,
      .eventFds = std::move(fds)});
  auto *dataPtr = data.get();
  // Store the notification data and map both the data flow and consumer id to
  // the data.
  mNotificationDataStorage[dataPtr] = std::move(data);
  it->second.insert(dataPtr);
  mOffloadConsumerToHandle[consumer] = dataPtr;
  return std::move(dupFds);
}

pw::Status NotificationManager::removeOffloadConsumer(EndpointId consumer) {
  std::lock_guard lock(mLock);
  auto it = mOffloadConsumerToHandle.find(consumer);
  if (it == mOffloadConsumerToHandle.end()) {
    ALOGE("Attempted to remove unknown consumer");
    return pw::Status::NotFound();
  }
  auto *data = it->second;
  // Remove the consumer from the data flow mapping.
  mHostDataFlowToHandles[data->dataFlow.id].erase(data);
  // Unmap the consumer and discard the fds.
  mOffloadConsumerToHandle.erase(it);
  mNotificationDataStorage.erase(data);
  return pw::OkStatus();
}

pw::Status NotificationManager::enableHostConsumerFromHandle(
    const DataFlowSinkContext &consumer) {
  if (!consumer.info.has_value()) {
    ALOGE("Attempted to enable consumer without producer notify fds.");
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(auto notifyHostFds,
                dupEventFds(consumer.alertFds, /*needsHalAck=*/true));
  PW_TRY_ASSIGN(auto notifyOffloadFds,
                dupEventFds(consumer.info->alertFds, /*needsHalAck=*/false));
  return enableHostConsumerFromEventFds(consumer.id, std::move(notifyHostFds),
                                        std::move(notifyOffloadFds));
}

pw::Status NotificationManager::enableHostConsumerFromEventFds(
    DataFlowId dataFlow, DataFlowAlertFds &&notifyHostFds,
    DataFlowAlertFds &&notifyOffloadFds) {
  std::lock_guard lock(mLock);
  auto it = mOffloadDataFlowToHandles.find(dataFlow);
  if (it != mOffloadDataFlowToHandles.end()) {
    ALOGE("Attempted to add duplicate handle for offload data flow (%" PRIx64
          ", %" PRIx32 ")",
          dataFlow.hubId, dataFlow.id);
    return pw::Status::AlreadyExists();
  } else if (notifyHostFds.waking.get() < 0 ||
             notifyHostFds.nonWaking.get() < 0 ||
             notifyHostFds.halAck.get() < 0 ||
             notifyOffloadFds.waking.get() < 0 ||
             notifyOffloadFds.nonWaking.get() < 0) {
    ALOGE(
        "Received invalid event fds from the HAL for offload data flow %" PRIx64
        ", %" PRIx32 ")",
        dataFlow.hubId, dataFlow.id);
    return pw::Status::InvalidArgument();
  }
  auto &dataPair = mOffloadDataFlowToHandles[dataFlow];
  // Set up the NotificationData for incoming notifications to this consumer.
  auto data = std::make_unique<NotificationData>(NotificationData{
      .dataFlow = dataFlow, .eventFds = std::move(notifyHostFds)});
  auto *dataPtr = data.get();
  dataPair.second = dataPtr;
  enableNotifications(dataPtr);
  mNotificationDataStorage[dataPtr] = std::move(data);
  // Set up the NotificationData for outgoing notifications to the producer.
  data = std::make_unique<NotificationData>(NotificationData{
      .dataFlow = dataFlow, .eventFds = std::move(notifyOffloadFds)});
  dataPtr = data.get();
  dataPair.first = dataPtr;
  mNotificationDataStorage[dataPtr] = std::move(data);
  return pw::OkStatus();
}

pw::Status NotificationManager::disableHostConsumer(DataFlowId dataFlow) {
  std::lock_guard lock(mLock);
  auto it = mOffloadDataFlowToHandles.find(dataFlow);
  if (it == mOffloadDataFlowToHandles.end()) {
    ALOGE("Attempted to disable unknown offload data flow (%" PRIx64
          ", %" PRIx32 ")",
          dataFlow.hubId, dataFlow.id);
    return pw::Status::NotFound();
  }
  auto [producerData, consumerData] = it->second;
  // Disable the epoll triggers for the consumer.
  disableNotifications(consumerData);
  // Unmap the data flow and discard the fds.
  mOffloadDataFlowToHandles.erase(it);
  mNotificationDataStorage.erase(consumerData);
  mNotificationDataStorage.erase(producerData);
  return pw::OkStatus();
}

pw::Status NotificationManager::notifyOffloadProducer(DataFlowId dataFlow,
                                                      bool waking) {
  std::lock_guard lock(mLock);
  auto it = mOffloadDataFlowToHandles.find(dataFlow);
  if (it == mOffloadDataFlowToHandles.end()) {
    ALOGE("Attempted to notify unknown offload data flow (%" PRIx64 ", %" PRIx32
          ")",
          dataFlow.hubId, dataFlow.id);
    return pw::Status::NotFound();
  }
  auto *producerData = it->second.first;
  auto fd = waking ? producerData->eventFds.waking.get()
                   : producerData->eventFds.nonWaking.get();
  if (!sendNotification(fd).ok()) {
    ALOGE(
        "Failed to write notification to producer of offload data flow "
        "(%" PRIx64 ", %" PRIx32 ") with %" PRId32,
        dataFlow.hubId, dataFlow.id, errno);
    return pw::Status::Internal();
  }
  return pw::OkStatus();
}

pw::Status NotificationManager::notifyOffloadConsumer(EndpointId consumer,
                                                      bool waking) {
  std::lock_guard lock(mLock);
  auto it = mOffloadConsumerToHandle.find(consumer);
  if (it == mOffloadConsumerToHandle.end()) {
    ALOGE("Could not notify unmapped offload consumer (%" PRIx64 ", %" PRIx64
          ")",
          consumer.hubId, consumer.id);
    return pw::Status::NotFound();
  }
  auto *data = it->second;
  auto fd =
      waking ? data->eventFds.waking.get() : data->eventFds.nonWaking.get();
  if (!sendNotification(fd).ok()) {
    ALOGE("Failed to write notification to consumer (%" PRIx64 ", %" PRIx64
          ") on data flow %" PRIx32 " with %" PRId32,
          consumer.hubId, consumer.id, data->dataFlow.id, errno);
    return pw::Status::Internal();
  }
  return pw::OkStatus();
}

void NotificationManager::handleNotification(int fd, bool error) {
  std::unique_lock lock(mLock);
  // Look up the data flow based on the fd.
  auto it = mWaitFdToHandle.find(fd);
  if (it == mWaitFdToHandle.end()) {
    ALOGI("Ignoring unmapped epoll trigger, data flow likely removed.");
    return;
  }
  auto &data = *it->second;
  bool waking = fd == data.eventFds.waking.get();
  if (error) {
    ALOGE("Error event on epoll trigger for fd %d, disabling.", fd);
    // Disable the epoll triggers. This will be cleaned up later.
    mWaiter->removeFd(fd);
    if (waking) {
      data.eventFds.waking.set(-1);
    } else {
      data.eventFds.nonWaking.set(-1);
    }
    mWaitFdToHandle.erase(it);
    return;
  }
  // Read the notification count. For a waking notification, this count will be
  // written back to the HAL.
  uint64_t wakeCount = 0;
  int rv = TEMP_FAILURE_RETRY(read(fd, &wakeCount, sizeof(wakeCount)));
  if (rv < 0) {
    ALOGE("Failed to read wake count with %d", errno);
  }
  // Invoke the callback outside of the lock.
  lock.unlock();
  mNotifyCb(data.dataFlow, waking);
  lock.lock();
  // If this was waking and the data flow is still active, ack it to the HAL.
  if (auto it = mWaitFdToHandle.find(fd);
      it != mWaitFdToHandle.end() && waking && rv >= 0) {
    TEMP_FAILURE_RETRY(
        write(data.eventFds.halAck.get(), &wakeCount, sizeof(wakeCount)));
  }
}

void NotificationManager::enableNotifications(NotificationData *data) {
  mWaitFdToHandle[data->eventFds.waking.get()] = data;
  mWaiter->addFd(data->eventFds.waking.get());
  mWaitFdToHandle[data->eventFds.nonWaking.get()] = data;
  mWaiter->addFd(data->eventFds.nonWaking.get());
}

void NotificationManager::disableNotifications(NotificationData *data) {
  if (data->eventFds.waking.get() >= 0) {
    mWaiter->removeFd(data->eventFds.waking.get());
  }
  mWaitFdToHandle.erase(data->eventFds.waking.get());
  if (data->eventFds.nonWaking.get() >= 0) {
    mWaiter->removeFd(data->eventFds.nonWaking.get());
  }
  mWaitFdToHandle.erase(data->eventFds.nonWaking.get());
}

}  // namespace android::contexthub::data_flow
