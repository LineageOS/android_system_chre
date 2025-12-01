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

#include "data_flow/host/remote_consumer.h"

#include <cstddef>
#include <cstdint>
#include <variant>

#include "data_flow/internal/queue_internal.h"
#include "data_flow/queue.h"
#include "data_flow/queue_defs.h"
#include "data_flow/untyped_queue.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace android::contexthub::data_flow {

pw::Result<std::variant<UntypedConsumer, VariableDataConsumer>>
createRemoteConsumer(Region region, std::optional<Region> descRegion,
                     uint32_t queueOffset, uint32_t descOffset,
                     RemoteNotifyArgs notifyArgs) {
  auto *queue = internal::fromOffset<internal::Queue>(region, queueOffset);
  if (!queue) {
    return pw::Status::InvalidArgument();
  }
  switch (queue->config.mode) {
    case internal::Queue::DataConfig::Mode::kFixedSize: {
      PW_TRY_ASSIGN(auto consumer, UntypedConsumer::createRemote(
                                       region, descRegion, queueOffset,
                                       descOffset, std::move(notifyArgs),
                                       /*memAccess=*/nullptr));
      return consumer;
    }
    case internal::Queue::DataConfig::Mode::kVariableSizeBasic: {
      PW_TRY_ASSIGN(auto consumer, VariableDataConsumer::createRemote(
                                       region, descRegion, queueOffset,
                                       descOffset, std::move(notifyArgs),
                                       /*memAccess=*/nullptr));
      return consumer;
    }
    case internal::Queue::DataConfig::Mode::kVariableSizeAligned:
      return pw::Status::Unimplemented();
    default:
      return pw::Status::InvalidArgument();
  }
}

}  // namespace android::contexthub::data_flow