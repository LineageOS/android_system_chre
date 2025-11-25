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

#include "data_flow/host/region_manager.h"

#include <cstddef>

#include <aidl/android/hardware/contexthub/IContextHub.h>

#include "data_flow/queue_defs.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::contexthub::data_flow {

pw::Result<AllocatorRegion> RegionManager::mapHostProducerRegion(
    SharedDataRegion && /*region*/) {
  return pw::Status::Unimplemented();
}

pw::Result<AllocatorRegion> RegionManager::getHostProducerRegion(int /*id*/) {
  return pw::Status::Unimplemented();
}

pw::Status RegionManager::unmapHostProducerRegion(int /*id*/) {
  return pw::Status::Unimplemented();
}

pw::Status RegionManager::linkHostProducerDataFlowToRegion(int /*region*/,
                                                           int /*dataFlow*/) {
  return pw::Status::Unimplemented();
}

pw::Result<size_t> RegionManager::unlinkHostProducerDataFlow(int /*id*/) {
  return pw::Status::Unimplemented();
}

pw::Result<AllocatorRegion> RegionManager::mapOffloadConsumerRegion(
    SharedDataRegion && /*region*/, const EndpointId & /*consumer*/,
    int /*dataFlow*/) {
  return pw::Status::Unimplemented();
}

pw::Result<Region> RegionManager::mapHostConsumerRegion(
    SharedDataRegion && /*region*/, const DataFlowId & /*dataFlow*/,
    const EndpointId & /*producer*/) {
  return pw::Status::Unimplemented();
}

pw::Status RegionManager::unlinkHostConsumerDataFlow(
    const DataFlowId & /*dataFlow*/) {
  return pw::Status::Unimplemented();
}

pw::Status RegionManager::unlinkOffloadEndpoint(
    const EndpointId & /*endpoint*/) {
  return pw::Status::Unimplemented();
}

}  // namespace android::contexthub::data_flow