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

namespace android::hardware::contexthub::common::implementation {

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
  struct epoll_event event = {.events = EPOLLIN, .data = {.fd = haltFd.get()}};
  int rv = epoll_ctl(epollFd, EPOLL_CTL_ADD, haltFd.get(), &event);
  if (rv < 0) {
    ALOGE("Failed to register DataFlowEpollWaiter haltFd trigger on %d: %s",
          haltFd.get(), strerror(errno));
    return pw::Status::Internal();
  }
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

pw::Status DataFlowEpollWaiter::addTriggers(
    DataFlowId /*dataFlowId*/, EndpointId /*endpointId*/,
    const DataFlowAlertFds & /*alertFds*/) {
  // TODO(b/480216336): Implement this.
  return pw::Status::Unimplemented();
}

pw::Status DataFlowEpollWaiter::removeTriggers(
    std::optional<DataFlowId> /*dataFlowId*/,
    std::optional<EndpointId> /*endpointId*/) {
  // TODO(b/480216336): Implement this.
  return pw::Status::Unimplemented();
}

void DataFlowEpollWaiter::epollWaitLoop() {
  while (true) {
    std::vector<struct epoll_event> events(1);
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