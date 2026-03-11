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

#ifndef CHRE_PLATFORM_PLATFORM_EVENT_LOOP_H_
#define CHRE_PLATFORM_PLATFORM_EVENT_LOOP_H_

#include <cstddef>
#include <cstdint>

#include "chre/core/event.h"
#include "chre/target_platform/platform_event_loop_base.h"
#include "chre/util/non_copyable.h"

namespace chre {

class EventLoop;

/**
 * The common interface to EventLoop functionality that has platform-specific
 * implementation but must be supported for every platform.
 */
class PlatformEventLoop : public PlatformEventLoopBase, public NonCopyable {
 public:
  /**
   * Distributes an event to this event loop. This method can be customized for
   * platforms to perform additional operations beyond distributing the event to
   * nanoapps, which is provided by the common helper
   * EventLoop::distributeEventCommon().
   *
   * This method must free the event using eventLoop->freeEvent(event) during
   * this call.
   */
  void distributeEvent(EventLoop *eventLoop, Event *event);
};

}  // namespace chre

/* The platform can optionally provide an inlined implementation */
#if __has_include("chre/target_platform/platform_event_loop_impl.h")
#include "chre/target_platform/platform_event_loop_impl.h"
#endif  // __has_include("chre/target_platform/platform_event_loop_impl.h")

#endif  // CHRE_PLATFORM_PLATFORM_EVENT_LOOP_H_
