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

#include "chre_host/concurrent_fixed_capacity_queue.h"

#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace android::chre {

namespace {

using testing::HasSubstr;
using testing::Not;

constexpr size_t kQueueCapacity = 3;

// Define a test struct that is printable
struct MyTestElement {
  int value;
  [[nodiscard]] std::string toString() const {
    std::ostringstream ss;
    ss << '[' << value << "],";
    return ss.str();
  }
};
}  // namespace

TEST(ConcurrentFixedCapacityQueueTest, PushAndPop) {
  ConcurrentFixedCapacityQueue<MyTestElement> queue(kQueueCapacity);
  queue.push({1});
  queue.push({2});
  ASSERT_EQ(queue.size(), 2);

  queue.pop();
  ASSERT_EQ(queue.size(), 1);

  queue.pop();
  ASSERT_EQ(queue.size(), 0);

  queue.pop();
  ASSERT_EQ(queue.size(), 0);
}

TEST(ConcurrentFixedCapacityQueueTest, OverPushed) {
  ConcurrentFixedCapacityQueue<MyTestElement> queue(kQueueCapacity);
  for (int i = 1; i <= kQueueCapacity + 2; i++) {
    queue.push({i});
  }

  ASSERT_EQ(queue.size(), kQueueCapacity);

  std::string queue_content = queue.toString();
  std::string elementMustHave = MyTestElement(kQueueCapacity + 2).toString();
  // Elements purged out:
  EXPECT_THAT(queue_content, Not(HasSubstr("[1]")));
  EXPECT_THAT(queue_content, Not(HasSubstr("[2]")));
  // Element must have:
  EXPECT_THAT(queue_content, HasSubstr(elementMustHave));
}

TEST(ConcurrentFixedCapacityQueueTest, EmptyQueue) {
  ConcurrentFixedCapacityQueue<MyTestElement> queue(kQueueCapacity);
  ASSERT_EQ(queue.size(), 0);
  ASSERT_EQ(queue.toString(), "[EMPTY]\n");
}

TEST(ConcurrentFixedCapacityQueueTest, MultipleThreadsPush) {
  ConcurrentFixedCapacityQueue<MyTestElement> queue(kQueueCapacity);
  constexpr int kNumThreads = 10;
  constexpr int kElementsPerThread = 100;

  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < kElementsPerThread; ++j) {
        queue.push({i * kElementsPerThread + j});
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  // The queue should have at most the capacity size elements
  ASSERT_LE(queue.size(), kQueueCapacity);
}

TEST(ConcurrentFixedCapacityQueueTest, MultipleThreadsPushAndPop) {
  ConcurrentFixedCapacityQueue<MyTestElement> queue(kQueueCapacity);
  constexpr int kNumThreads = 10;
  constexpr int kElementsPerThread = 100;

  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < kElementsPerThread; ++j) {
        queue.push({i * kElementsPerThread + j});
        queue.pop();
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  // The queue should have at most the capacity size elements
  ASSERT_LE(queue.size(), kQueueCapacity);
}
}  // namespace android::chre