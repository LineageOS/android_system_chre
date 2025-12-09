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

#ifndef CHRE_HOST_CONCURRENT_FIXED_CAPACIITY_QUEUE_H_
#define CHRE_HOST_CONCURRENT_FIXED_CAPACIITY_QUEUE_H_

#include "chre_host/log.h"

#include <cinttypes>
#include <deque>
#include <mutex>
#include <sstream>
#include <string>

#include <android-base/thread_annotations.h>

namespace android::chre {

// Elements of FixedCapacityQueue must be printable.
template <typename T>
concept Printable = requires(T element) {
  { element.toString() } -> std::same_as<std::string>;
};

/**
 * A thread-safe queue that new element pushed will purge out the oldest element
 * if the queue is full.
 */
template <Printable T>
class ConcurrentFixedCapacityQueue {
 public:
  explicit ConcurrentFixedCapacityQueue(const uint8_t capacity)
      : mCapacity(capacity) {
    // Capacity 0 gives little benefit for using this concurrent queue and most
    // likely indicates an error in the client code.
    if (capacity == 0) {
      LOG_ALWAYS_FATAL("Capacity should always be > 0");
    }
  }

  void push(const T &message) {
    std::lock_guard lock(mLock);
    if (mQueue.size() == mCapacity) {
      mQueue.pop_front();
    }
    mQueue.push_back(message);
  }

  void pop() {
    std::lock_guard lock(mLock);
    if (!mQueue.empty()) {
      mQueue.pop_front();
    }
  }

  [[nodiscard]] size_t size() {
    std::lock_guard lock(mLock);
    return mQueue.size();
  }

  std::string toString() {
    std::ostringstream stream;
    std::lock_guard lock(mLock);
    if (mQueue.empty()) {
      stream << "[EMPTY]\n";
    } else {
      stream << "\n";
      for (const auto &message : mQueue) {
        stream << message.toString() << "\n";
      }
    }
    return stream.str();
  }

 private:
  std::mutex mLock;
  std::deque<T> mQueue GUARDED_BY(mLock){};
  const uint8_t mCapacity;
};
}  // namespace android::chre

#endif  // CHRE_HOST_CONCURRENT_FIXED_CAPACIITY_QUEUE_H_