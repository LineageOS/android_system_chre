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

#include <array>
#include <cstddef>
#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_span/span.h"

namespace chre::shmem_spmc_queue {

/** Sends a notification to an endpoint within the same "process". */
using LocalNotifyFn = void (*)(void *context);

/** Arguments passed to endpoints on a queue using local notifications. */
struct LocalNotifyArgs {
  LocalNotifyFn fn;
  void *ctx;
};

/** Sends an out-of-band notification to an endpoint described by id. */
using RemoteNotifyFn = pw::Function<void(pw::ConstByteSpan id)>;

/** Arguments passed to endpoints on a queue using out-of-band notifications. */
struct RemoteNotifyArgs {
  RemoteNotifyFn fn;
  std::array<std::byte, 16> id;
};

/** Notification policies a consumer can set. */
enum class NotificationPolicy : uint8_t {
  kNever = 0x0,          // Never notify
  kOpportunistic = 0x1,  // Above low watermark, notify if awake
  kHighWaterMark = 0x2,  // Notify above high watermark
  kPeriodic = 0x3,       // After tick threshold, notify if available
  kStreaming = 0x4,      // Always notify
  kMask = 0xf,           // Mask for extracting notification policy bits.
};

/** Overwrite policies a consumer can set. */
enum class OverwritePolicy : uint8_t {
  kAllowed = 0x0 << 4,     // Producer may overwrite the Consumer.
  kDisallowed = 0x1 << 4,  // Producer may not overwrite the Consumer.
  kMask = 0xf << 4,        // Mask for extracting overwrite policy bits.
};

}  // namespace chre::shmem_spmc_queue
