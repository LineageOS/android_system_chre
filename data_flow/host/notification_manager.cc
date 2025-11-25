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

#include "data_flow/host/notification_manager.h"

#include <aidl/android/hardware/contexthub/IContextHub.h>

#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::contexthub::data_flow {

using ::aidl::android::hardware::contexthub::DataFlowConsumer;
using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::DataFlowInfo;
using ::aidl::android::hardware::contexthub::EndpointId;

NotificationManager::NotificationManager(
    std::unique_ptr<EpollWaiter> /*waiter*/, NotificationCallback && /*cb*/) {}

NotificationManager::~NotificationManager() {}

pw::Result<std::pair<DataFlowInfo, NotificationManager::NotificationDataHandle>>
NotificationManager::prepareHostProducerDataFlow() {
  return pw::Status::Unimplemented();
}

pw::Status NotificationManager::activateHostProducerDataFlow(
    int /*id*/, NotificationDataHandle && /*handle*/) {
  return pw::Status::Unimplemented();
}

pw::Status NotificationManager::removeHostProducerDataFlow(int /*id*/) {
  return pw::Status::Unimplemented();
}

pw::Result<::aidl::android::hardware::contexthub::DataFlowConsumer>
NotificationManager::addOffloadConsumer(int /*dataFlow*/,
                                        EndpointId /*consumer*/) {
  return pw::Status::Unimplemented();
}

pw::Status NotificationManager::removeOffloadConsumer(EndpointId /*consumer*/) {
  return pw::Status::Unimplemented();
}

pw::Status NotificationManager::enableHostConsumer(
    DataFlowConsumer && /*consumer*/) {
  return pw::Status::Unimplemented();
}

pw::Status NotificationManager::disableHostConsumer(DataFlowId /*dataFlow*/) {
  return pw::Status::Unimplemented();
}

pw::Status NotificationManager::notify(DataFlowId /*dataFlow*/,
                                       bool /*waking*/) {
  return pw::Status::Unimplemented();
}

void NotificationManager::handleNotification(int /*fd*/, int /*events*/) {}

}  // namespace android::contexthub::data_flow
