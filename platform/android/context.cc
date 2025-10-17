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

#include "chre/platform/context.h"
#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/variant/config.h"

namespace chre {

bool inEventLoopThread() {
  // TODO(b/445584823): Implement this.
  return true;
}

EventLoop *getCurrentEventLoop() {
  static_assert(CHRE_MULTI_THREADING_ENABLED == 0,
                "CHRE multi-threading is not implemented on this platform");
  return inEventLoopThread() ? &EventLoopManagerSingleton::get()->getEventLoop()
                             : nullptr;
}

}  // namespace chre
