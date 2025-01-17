/*
 * Copyright (C) 2024 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_LINUX_NOTIFIER_BASE_H_
#define CHRE_PLATFORM_LINUX_NOTIFIER_BASE_H_

#include <pthread.h>

#include <condition_variable>
#include <mutex>
#include <optional>

namespace chre {

/**
 * Storage for Linux implementation of a direct task notifier.
 */
class NotifierBase {
 protected:
  static constexpr uint32_t kWaitFlag = 0x80000000;

  //! The task this Notifier instance is bound to.
  std::optional<pthread_t> mTarget;

  //! Mutex protecting the notified state.
  std::mutex mLock;

  //! Condition variable used to notify the waiting task.
  std::condition_variable mCondVar;

  //! Whether the Notifier has been notified.
  bool mNotified = false;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_LINUX_NOTIFIER_BASE_H_
