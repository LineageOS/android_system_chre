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

#include <cstddef>
#include <cstdint>
#include <variant>

#include "data_flow/queue.h"
#include "data_flow/queue_defs.h"
#include "data_flow/untyped_queue.h"
#include "pw_result/result.h"

namespace android::contexthub::data_flow {

/**
 * Creates a remote consumer without knowledge of the data configuration.
 *
 * The user provides the mapped region(s) containing the queue, metadata, and
 * consumer descriptor, along with the remote notification arguments. This
 * method determines the data configuration based on the queue metadata and
 * returns an instance of the appropriate type.
 *
 * @param region The region to create the consumer for.
 * @param descRegion The region to create the consumer for.
 * @param queueOffset The offset into the region to the queue.
 * @param descOffset The offset into the region to the consumer descriptor.
 * @param notifyArgs The arguments to use for notifying the consumer.
 * @return An untyped fixed-size data consumer or variable-size data consumer.
 */
pw::Result<std::variant<UntypedConsumer, VariableDataConsumer>>
createRemoteConsumer(Region region, std::optional<Region> descRegion,
                     uint32_t queueOffset, uint32_t descOffset,
                     RemoteNotifyArgs notifyArgs);

}  // namespace android::contexthub::data_flow