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

#if __has_include(<aidl/android/hardware/contexthub/SharedDataRegion.h>)
#include <aidl/android/hardware/contexthub/SharedDataRegion.h>
#else  // __has_include(<aidl/android/hardware/contexthub/SharedDataRegion.h>)
#include "data_flow/internal/SharedDataRegion.h"
#endif  // __has_include(<aidl/android/hardware/contexthub/SharedDataRegion.h>)

#include "pw_allocator/allocator.h"
#include "pw_function/function.h"

namespace android::contexthub::data_flow {

/** Describes the version of this implementation of the shared memory ABI. */
using Version =
    ::aidl::android::hardware::contexthub::SharedDataRegion::Version;

/** The version of this implementation. */
constexpr Version kVersion{.major = 0x01, .minor = 0x00, .patch = 0x0000};

/**
 * The minimum version that a remote endpoint must be on to be compatible with
 * this implementation.
 */
constexpr uint8_t kMinCompatibleMajorVersion = 0x01;

/** Sends a notification to an endpoint within the same "process". */
using LocalNotifyFn = void (*)(void *context);

/** Arguments passed to endpoints on a queue using local notifications. */
struct LocalNotifyArgs {
  LocalNotifyFn fn;
  void *ctx;
};

/** Endpoint id type for use with ContextHub messaging network. */
using AidlEndpointId = ::aidl::android::hardware::contexthub::SharedDataRegion::
    EndpointIdFixedSize;
static_assert(sizeof(AidlEndpointId) == 16);

/** Union of endpoint types to allow for different endpoint id types. */
union RemoteEndpointId {
  AidlEndpointId aidlId;
};
static_assert(sizeof(RemoteEndpointId) == sizeof(AidlEndpointId));

/** Sends an out-of-band notification to an endpoint described by id. */
using RemoteNotifyFn = pw::Function<void(const RemoteEndpointId &id)>;

/** Arguments passed to endpoints on a queue using out-of-band notifications. */
struct RemoteNotifyArgs {
  RemoteNotifyFn fn;
  RemoteEndpointId id;
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

/** Represents a shared memory region. */
struct Region {
  uintptr_t base;
  uint32_t size;
};

/** Represents a shared memory region that can be allocated from. */
struct AllocatorRegion : public Region {
  pw::Allocator *allocator;
};

}  // namespace android::contexthub::data_flow
