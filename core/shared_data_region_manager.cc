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

#if defined(CHRE_DATA_FLOW_SUPPORT_ENABLED)

#include "chre/core/shared_data_region_manager.h"

namespace chre {

void SharedDataRegionManager::handleAllocateDataFlowRegionAsyncResult(
    uintptr_t /*cookie*/, pw::Status /*status*/, int32_t /*regionId*/,
    const android::contexthub::data_flow::AllocatorRegion & /*region*/,
    android::contexthub::data_flow::MemoryAccess * /*memoryAccess*/) {
  // TODO(b/457453613): Implement this.
}

void SharedDataRegionManager::handleRegionInvalidated(int32_t /*regionId*/) {
  // TODO(b/457453613): Implement this.
}

}  // namespace chre

#endif  // defined(CHRE_DATA_FLOW_SUPPORT_ENABLED)
