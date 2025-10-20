/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_CONTEXT_H_
#define CHRE_PLATFORM_CONTEXT_H_

#include "chre/core/event_loop.h"

namespace chre {

/**
 * @return true to indicate that the current thread is the thread that is
 * currently blocked by the event loop. This is used by the event loop to
 * determine whether it needs to lock shared data structures or not.
 *
 * TODO(b/435246073): Deprecate this API in favor of getCurrentEventLoop().
 */
bool inEventLoopThread();

/**
 * @return A reference to the EventLoop that is running on the current thread.
 * Null if there is no event loop running on this thread.
 */
EventLoop *getCurrentEventLoop();

}  // namespace chre

#endif  // CHRE_CONTEXT_H
