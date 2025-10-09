/*
 * Copyright (C) 2017 The Android Open Source Project
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
#include "chre/platform/linux/thread_context.h"

namespace chre {

namespace {
// Note: thread_local may not be well-supported on all platforms, so beware of
// its usage outside this linux impl.
thread_local EventLoop *gEventLoop = nullptr;
}  // anonymous namespace

bool inEventLoopThread() {
  // TODO: Implement this.
  return true;
}

EventLoop *getCurrentEventLoop() {
  return gEventLoop;
}

void registerThreadContext(EventLoop *eventLoop) {
  gEventLoop = eventLoop;
}

}  // namespace chre
