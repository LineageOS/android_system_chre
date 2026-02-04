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

#define LOG_TAG "DataFlowEpollWaiter"

#include "data_flow_epoll_waiter.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <aidl/android/hardware/contexthub/DataFlowAlertFds.h>
#include <aidl/android/hardware/contexthub/DataFlowId.h>
#include <aidl/android/hardware/contexthub/EndpointId.h>
#include <android-base/macros.h>
#include <android-base/unique_fd.h>
#include <utils/Log.h>

#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::hardware::contexthub::common::implementation {
namespace {

pw::Status addTrigger(int epollFd, int fd, bool waking) {
  uint32_t events = waking ? EPOLLIN | EPOLLWAKEUP : EPOLLIN;
  struct epoll_event event = {.events = events, .data = {.fd = fd}};
  int rv = epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
  if (rv < 0) {
    ALOGE("Failed to register DataFlowEpollWaiter trigger on %d: %s", fd,
          strerror(errno));
    return pw::Status::Internal();
  }
  return pw::OkStatus();
}

void removeTrigger(int epollFd, int fd) {
  if (epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
    ALOGE("Failed to remove DataFlowEpollWaiter trigger on %d: %s", fd,
          strerror(errno));
  }
}

}  // namespace

pw::Result<std::unique_ptr<DataFlowEpollWaiter>> DataFlowEpollWaiter::create(
    Callback &callback) {
  base::unique_fd epollFd(epoll_create1(EPOLL_CLOEXEC));
  if (!epollFd.ok()) {
    return pw::Status::Internal();
  }
  base::unique_fd haltFd(eventfd(0, EFD_NONBLOCK));
  if (!haltFd.ok()) {
    ALOGE("Failed to create DataFlowEpollWaiter haltFd: %s", strerror(errno));
    return pw::Status::Internal();
  }
  PW_TRY(addTrigger(epollFd, haltFd, /*waking=*/false));
  return std::unique_ptr<DataFlowEpollWaiter>(
      new DataFlowEpollWaiter(std::move(epollFd), std::move(haltFd), callback));
}

DataFlowEpollWaiter::DataFlowEpollWaiter(base::unique_fd epollFd,
                                         base::unique_fd haltFd,
                                         Callback &callback)
    : mEpollFd(std::move(epollFd)),
      mHaltFd(std::move(haltFd)),
      mCallback(callback),
      mEpollThread(&DataFlowEpollWaiter::epollWaitLoop, this) {}

DataFlowEpollWaiter::~DataFlowEpollWaiter() {
  // Signal the epoll thread to exit.
  const uint64_t kNotZero = 1;
  ssize_t bytesWritten =
      TEMP_FAILURE_RETRY(write(mHaltFd.get(), &kNotZero, sizeof(kNotZero)));
  if (bytesWritten != sizeof(kNotZero)) {
    // There is no safe way to wake up the epoll thread at this point. This
    // should never happen, so we will crash here.
    LOG_ALWAYS_FATAL("Failed to write to DataFlowEpollWaiter haltFd: %s",
                     strerror(errno));
    return;
  }
  // Join the epoll thread.
  if (mEpollThread.joinable()) {
    mEpollThread.join();
  }
}

pw::Status DataFlowEpollWaiter::addTriggers(DataFlowId dataFlowId,
                                            EndpointId endpointId,
                                            const DataFlowAlertFds &alertFds) {
  std::lock_guard lock(mLock);
  auto trigger = std::make_unique<Trigger>(
      Trigger{.dataFlowId = dataFlowId, .endpointId = endpointId});
  auto it = mDataFlowEndpointToTrigger.find({dataFlowId, endpointId});
  if (it != mDataFlowEndpointToTrigger.end()) {
    ALOGE("Trigger already registered for endpoint (%s), data flow (%s)",
          endpointId.toString().c_str(), dataFlowId.toString().c_str());
    return pw::Status::AlreadyExists();
  }
  bool isHostEndpoint = alertFds.halAck.get() >= 0;
  if (isHostEndpoint) {
    // For a host endpoint, register and map a trigger for the fd used to ack
    // waking notifications.
    trigger->alertFds.halAck = alertFds.halAck.dup();
    PW_TRY(
        addTrigger(mEpollFd, trigger->alertFds.halAck.get(), /*waking=*/false));
    mFdToTrigger[trigger->alertFds.halAck.get()] = trigger.get();
  } else {
    if (alertFds.waking.get() < 0 || alertFds.nonWaking.get() < 0) {
      ALOGE("Invalid alertFds for embedded endpoint (%s), data flow (%s)",
            endpointId.toString().c_str(), dataFlowId.toString().c_str());
      return pw::Status::InvalidArgument();
    }
    // For an embedded endpoint, register and map triggers for the waking and
    // non-waking alert fds.
    trigger->alertFds.waking = alertFds.waking.dup();
    trigger->alertFds.nonWaking = alertFds.nonWaking.dup();
    PW_TRY(
        addTrigger(mEpollFd, trigger->alertFds.waking.get(), /*waking=*/true));
    auto status = addTrigger(mEpollFd, trigger->alertFds.nonWaking.get(),
                             /*waking=*/false);
    if (!status.ok()) {
      removeTrigger(mEpollFd, trigger->alertFds.waking.get());
      return status;
    }
    mFdToTrigger[trigger->alertFds.waking.get()] = trigger.get();
    mFdToTrigger[trigger->alertFds.nonWaking.get()] = trigger.get();
  }
  mDataFlowEndpointToTrigger[{dataFlowId, endpointId}] = trigger.get();
  mTriggers.push_back(std::move(trigger));
  return pw::OkStatus();
}

pw::Status DataFlowEpollWaiter::removeTriggers(
    std::optional<DataFlowId> dataFlowId,
    std::optional<EndpointId> endpointId) {
  if (!dataFlowId && !endpointId) {
    ALOGE("At least one of dataFlowId or endpointId must be provided");
    return pw::Status::InvalidArgument();
  }
  std::lock_guard lock(mLock);
  auto removeCount =
      mTriggers.remove_if([&](const std::unique_ptr<Trigger> &trigger) {
        if ((dataFlowId && trigger->dataFlowId != *dataFlowId) ||
            (endpointId && trigger->endpointId != *endpointId)) {
          return false;
        }
        // Remove and unmap the epoll triggers for the fds in this Trigger.
        if (trigger->alertFds.halAck.get() >= 0) {
          removeTrigger(mEpollFd, trigger->alertFds.halAck.get());
          mFdToTrigger.erase(trigger->alertFds.halAck.get());
        } else {
          removeTrigger(mEpollFd, trigger->alertFds.waking.get());
          removeTrigger(mEpollFd, trigger->alertFds.nonWaking.get());
          mFdToTrigger.erase(trigger->alertFds.waking.get());
          mFdToTrigger.erase(trigger->alertFds.nonWaking.get());
        }
        mDataFlowEndpointToTrigger.erase(
            {trigger->dataFlowId, trigger->endpointId});
        return true;
      });
  return removeCount > 0 ? pw::OkStatus() : pw::Status::NotFound();
}

void DataFlowEpollWaiter::epollWaitLoop() {
  std::vector<struct epoll_event> events;
  while (true) {
    {
      std::lock_guard lock(mLock);
      events.resize(mFdToTrigger.size() + 1 /*haltFd*/);
    }
    int rv = TEMP_FAILURE_RETRY(epoll_wait(mEpollFd.get(), events.data(),
                                           events.size(),
                                           /*timeout=*/-1));
    if (rv <= 0) {
      ALOGE("DataFlowEpollWaiter epoll_wait() failed: %s", strerror(errno));
      mEpollFd.reset();
      return;
    }
    // Check for a halt event before processing any other events.
    for (auto i = 0; i < rv; ++i) {
      if (events[i].data.fd == mHaltFd.get()) {
        ALOGI("DataFlowEpollWaiter epoll_wait() halted");
        mEpollFd.reset();
        return;
      }
    }
    // TODO(b/480216336): Process other epoll events.
  }
}

}  // namespace android::hardware::contexthub::common::implementation