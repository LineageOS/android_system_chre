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

#include "chre/shmem_spmc_queue/queue.h"

#include <vector>

#include "gtest/gtest.h"
#include "pw_allocator/first_fit.h"
#include "pw_bytes/span.h"

namespace chre::shmem_spmc_queue {
namespace {

class TestConsumerManager : public ConsumerManager {
 public:
  using ConsumerManager::ConsumerManager;
  using ConsumerManager::forAllConsumers;
};

class QueueTest : public ::testing::Test {
 protected:
  static constexpr size_t kBlockCapacity = 32;
  static constexpr size_t kBaseMinBlockCount = 3;
  static constexpr size_t kBaseMaxBlockCount = 3;

  void SetUp() override {
    mStorage.resize(1024);
    mAllocator.Init(pw::ByteSpan(mStorage.data(), mStorage.size()));
    auto maybeQueue = chre::shmem_spmc_queue::createQueue<int, kBlockCapacity>(
        mAllocator, /*local=*/true);
    ASSERT_EQ(maybeQueue.status(), pw::OkStatus());
    mQueue = *maybeQueue;
    mConsumerManager.emplace(basePtr(), size(), mAllocator);
  }

  void TearDown() override {
    if (mQueue) {
      mAllocator.Deallocate(mQueue);
    }
  }

  void setRemote() {
    static_cast<internal::Queue *>(mQueue)->localNotify = false;
  }

  void *basePtr() {
    return mStorage.data();
  }

  uintptr_t base() {
    return reinterpret_cast<uintptr_t>(basePtr());
  }

  uint32_t size() {
    return mStorage.size();
  }

  pw::Result<Producer<int>> createLocalProducer(LocalNotifyArgs notifyArgs) {
    return Producer<int>::createLocal(basePtr(), size(), mQueue, mAllocator,
                                      kBlockCapacity, kBaseMaxBlockCount,
                                      kBaseMinBlockCount, mDataNotifier,
                                      *mConsumerManager, notifyArgs);
  }

  pw::Result<Producer<int>> createRemoteProducer(RemoteNotifyArgs notifyArgs) {
    return Producer<int>::createRemote(
        basePtr(), size(), mQueue, mAllocator, kBlockCapacity,
        kBaseMaxBlockCount, kBaseMinBlockCount, mDataNotifier,
        *mConsumerManager, std::move(notifyArgs));
  }

  std::vector<std::byte> mStorage;
  pw::allocator::FirstFitAllocator<> mAllocator;
  void *mQueue;
  DataNotifier mDataNotifier;
  std::optional<TestConsumerManager> mConsumerManager;
};

TEST_F(QueueTest, ProducerCreateLocalAndDestroy) {
  EXPECT_EQ(createLocalProducer(
                {.fn = [](void * /*context*/) { return; }, .ctx = nullptr})
                .status(),
            pw::OkStatus());
}

TEST_F(QueueTest, ProducerCreateRemoteAndDestroy) {
  setRemote();
  RemoteNotifyArgs args = {.fn = [](pw::ConstByteSpan /*id*/) { return; },
                           .id = {std::byte(0)}};
  EXPECT_EQ(createRemoteProducer(std::move(args)).status(), pw::OkStatus());
}

TEST_F(QueueTest, ProducerCreateFailureToAllocateBlockRing) {
  // Allocate all remaining memory.
  std::vector<void *> tmps;
  auto layout = internal::blockLayout<int>(kBlockCapacity);
  auto *tmp = mAllocator.Allocate(layout);
  while (tmp) {
    tmps.push_back(tmp);
    tmp = mAllocator.Allocate(layout);
  }
  EXPECT_EQ(createLocalProducer(
                {.fn = [](void * /*context*/) { return; }, .ctx = nullptr})
                .status(),
            pw::Status::ResourceExhausted());
  // Deallocate the allocated memory to avoid allocator crashing on destruction.
  for (auto *tmp : tmps) {
    mAllocator.Deallocate(tmp);
  }
}

TEST_F(QueueTest, ProducerCreateLocalFailureInvalidNotifyFn) {
  EXPECT_EQ(createLocalProducer({.fn = nullptr, .ctx = nullptr}).status(),
            pw::Status::InvalidArgument());
}

TEST_F(QueueTest, ProducerCreateRemoteFailureInvalidNotifyFn) {
  setRemote();
  EXPECT_EQ(createRemoteProducer({}).status(), pw::Status::InvalidArgument());
}

TEST_F(QueueTest, ConsumerManagerAddConsumerSuccess) {
  pw::Result<uint32_t> result = mConsumerManager->addConsumer(mQueue);
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_NE(*result, internal::kOffsetInvalid);

  int consumerCount = 0;
  mConsumerManager->forAllConsumers(
      *static_cast<internal::Queue *>(mQueue),
      [&](internal::ConsumerDesc &) { consumerCount++; });
  EXPECT_EQ(consumerCount, 1);

  // Remove the consumer to avoid memory leaks.
  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, *result), pw::OkStatus());
}

TEST_F(QueueTest, ConsumerManagerAddConsumerFailureNullQueue) {
  pw::Result<uint32_t> result = mConsumerManager->addConsumer(nullptr);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
}

TEST_F(QueueTest, ConsumerManagerAddConsumerFailureNoMemory) {
  // Allocate all remaining memory.
  std::vector<void *> tmps;
  auto layout = pw::allocator::Layout::Of<internal::ConsumerDesc>();
  auto *tmp = mAllocator.Allocate(layout);
  while (tmp) {
    tmps.push_back(tmp);
    tmp = mAllocator.Allocate(layout);
  }
  pw::Result<uint32_t> result = mConsumerManager->addConsumer(mQueue);
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
  for (auto *tmp : tmps) {
    mAllocator.Deallocate(tmp);
  }
}

TEST_F(QueueTest, ConsumerManagerRemoveConsumerSuccess) {
  pw::Result<uint32_t> result = mConsumerManager->addConsumer(mQueue);
  ASSERT_EQ(result.status(), pw::OkStatus());

  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, *result), pw::OkStatus());

  int consumerCount = 0;
  mConsumerManager->forAllConsumers(
      *static_cast<internal::Queue *>(mQueue),
      [&](internal::ConsumerDesc &) { consumerCount++; });
  EXPECT_EQ(consumerCount, 0);
}

TEST_F(QueueTest, ConsumerManagerRemoveConsumerMultiple) {
  pw::Result<uint32_t> result1 = mConsumerManager->addConsumer(mQueue);
  ASSERT_EQ(result1.status(), pw::OkStatus());
  pw::Result<uint32_t> result2 = mConsumerManager->addConsumer(mQueue);
  ASSERT_EQ(result2.status(), pw::OkStatus());

  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, *result1), pw::OkStatus());

  int consumerCount = 0;
  uint32_t foundOffset = 0;
  mConsumerManager->forAllConsumers(*static_cast<internal::Queue *>(mQueue),
                                    [&](internal::ConsumerDesc &desc) {
                                      consumerCount++;
                                      foundOffset = internal::toOffset(
                                          reinterpret_cast<uintptr_t>(base()),
                                          &desc);
                                    });
  EXPECT_EQ(consumerCount, 1);
  EXPECT_EQ(foundOffset, *result2);

  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, *result2), pw::OkStatus());

  consumerCount = 0;
  mConsumerManager->forAllConsumers(
      *static_cast<internal::Queue *>(mQueue),
      [&](internal::ConsumerDesc &) { consumerCount++; });
  EXPECT_EQ(consumerCount, 0);
}

TEST_F(QueueTest, ConsumerManagerRemoveConsumerFailureInvalidOffset) {
  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, internal::kOffsetInvalid),
            pw::Status::InvalidArgument());
}

TEST_F(QueueTest, ConsumerManagerRemoveConsumerFailureNotFound) {
  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, 12345),
            pw::Status::NotFound());
}

TEST_F(QueueTest, ConsumerManagerRemoveConsumerFailureNullQueue) {
  pw::Result<uint32_t> result = mConsumerManager->addConsumer(mQueue);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(mConsumerManager->removeConsumer(nullptr, *result),
            pw::Status::InvalidArgument());

  // Remove the consumer to avoid memory leaks.
  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, *result), pw::OkStatus());
}

}  // namespace
}  // namespace chre::shmem_spmc_queue
