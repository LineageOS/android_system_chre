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

#include "data_flow_epoll_waiter.h"

#include <memory>
#include <optional>

#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::hardware::contexthub::common::implementation {

pw::Result<std::unique_ptr<DataFlowEpollWaiter>> DataFlowEpollWaiter::create(
    Callback & /*callback*/) {
  // TODO(b/480216336): Implement this.
  return pw::Status::Unimplemented();
}

DataFlowEpollWaiter::DataFlowEpollWaiter() {
  // TODO(b/480216336): Implement this.
}

DataFlowEpollWaiter::~DataFlowEpollWaiter() {
  // TODO(b/480216336): Implement this.
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

}  // namespace android::hardware::contexthub::common::implementation