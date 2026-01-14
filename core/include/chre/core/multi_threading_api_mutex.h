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

#include "chre/util/non_copyable.h"
#include "chre/util/thread_annotations.h"
#include "chre/variant/config.h"

#if CHRE_MULTI_THREADING_ENABLED
#include "chre/platform/mutex.h"
#endif  // CHRE_MULTI_THREADING_ENABLED

namespace chre {

/**
 * A wrapper mutex for multi-threading global mutex optimized to be
 * no-op when the CHRE_MULTI_THREADING_ENABLED is off.
 *
 * This mutex is used to synchronize concurrent CHRE API calls across
 * potentially multiple threads.
 */
class CHRE_CAPABILITY("mutex") MultiThreadingApiMutex : public NonCopyable {
 public:
  void lock() CHRE_ACQUIRE() {
#if CHRE_MULTI_THREADING_ENABLED
    mMutex.lock();
#endif  // CHRE_MULTI_THREADING_ENABLED
  }

  void unlock() CHRE_RELEASE() {
#if CHRE_MULTI_THREADING_ENABLED
    mMutex.unlock();
#endif  // CHRE_MULTI_THREADING_ENABLED
  }

#if CHRE_MULTI_THREADING_ENABLED
 private:
  Mutex mMutex;
#endif  // CHRE_MULTI_THREADING_ENABLED
};

}  // namespace chre
