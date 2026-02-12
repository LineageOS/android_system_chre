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

#include "data_flow_manager.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::hardware::contexthub::common::implementation {

DataFlowManager::DataFlowManager(
    const std::shared_ptr<RegionAllocator> & /* regionAllocator */,
    const std::shared_ptr<WakelockManager> & /* wakelockManager */,
    SendAlertFn /* sendAlertFn */) {
  // TODO(b/463163051): Implement this.
}

pw::Result<SharedDataRegion> DataFlowManager::allocateRegion(
    const SharedDataRegionRequirements & /* requirements */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

pw::Status DataFlowManager::releaseRegion(int32_t /* region */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

pw::Status DataFlowManager::addHostSourceDataFlow(
    EndpointId /* source */, DataFlowId /* id */,
    const DataFlowInfo & /* info */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

pw::Result<std::pair<DataFlowInfo, std::optional<SharedDataRegion>>>
DataFlowManager::addOffloadSink(DataFlowSinkRegistrationParams /* params */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

pw::Result<DataFlowSinkContext> DataFlowManager::addHostSink(
    DataFlowId /* dataFlow */, EndpointId /* source */, EndpointId /* sink */,
    int32_t /* primaryRegionId */, int32_t /* sinkMetadataRegionId */,
    uint32_t /* metadataOffset */, uint32_t /* sinkMetadataOffset */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

pw::Result<std::vector<EndpointId>> DataFlowManager::removeDataFlow(
    DataFlowId /* id */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

pw::Result<EndpointId> DataFlowManager::removeSink(DataFlowId /* dataFlow */,
                                                   EndpointId /* sink */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

pw::Result<std::vector<DataFlowManager::PrunedEndpointDataFlowEntry>>
DataFlowManager::pruneEndpoint(EndpointId /* endpoint */) {
  // TODO(b/463163051): Implement this.
  return pw::Status::Unimplemented();
}

void DataFlowManager::onAlert(DataFlowId /* dataFlowId */,
                              EndpointId /* endpointId */, bool /* waking */) {
  // TODO(b/463163051): Implement this.
}

void DataFlowManager::onWakingAck(DataFlowId /* dataFlowId */,
                                  EndpointId /* endpointId */,
                                  uint64_t /* wakeCount */) {
  // TODO(b/463163051): Implement this.
}

}  // namespace android::hardware::contexthub::common::implementation