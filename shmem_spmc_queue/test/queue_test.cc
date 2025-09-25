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

class BasicProducerTest : public ::testing::Test {
 protected:
  void TearDown() override {
    if (mQueue) {
      mAllocator.Deallocate(mQueue);
    }
  }

  void init(size_t storageSize, bool local) {
    mStorage.resize(storageSize);
    mAllocator.Init(pw::ByteSpan(mStorage.data(), mStorage.size()));
    auto maybeQueue =
        chre::shmem_spmc_queue::createQueue<std::byte, 64>(mAllocator, local);
    ASSERT_EQ(maybeQueue.status(), pw::OkStatus());
    mQueue = *maybeQueue;
    mConsumerManager.emplace(base(), size(), mAllocator);
  }

  void *base() {
    return mStorage.data();
  }

  size_t size() {
    return mStorage.size();
  }

  std::vector<std::byte> mStorage;
  pw::allocator::FirstFitAllocator<> mAllocator;
  void *mQueue;
  DataNotifier mDataNotifier;
  std::optional<ConsumerManager> mConsumerManager;
};

TEST_F(BasicProducerTest, CreateLocalAndDestroy) {
  init(/*storageSize=*/1024, /*local=*/true);
  EXPECT_EQ(
      chre::shmem_spmc_queue::Producer<std::byte>::createLocal(
          base(), size(), mQueue, mAllocator, /*blockCapacity=*/64,
          /*maxBlockCount=*/5, /*minBlockCount=*/5, mDataNotifier,
          *mConsumerManager,
          {.fn = [](void * /*context*/) { return; }, .ctx = nullptr}, nullptr)
          .status(),
      pw::OkStatus());
}

TEST_F(BasicProducerTest, CreateRemoteAndDestroy) {
  init(/*storageSize=*/1024, /*local=*/false);
  RemoteNotifyArgs args = {.fn = [](pw::ConstByteSpan /*id*/) { return; },
                           .id = {std::byte(0)}};
  EXPECT_EQ(chre::shmem_spmc_queue::Producer<std::byte>::createRemote(
                base(), size(), mQueue, mAllocator, /*blockCapacity=*/64,
                /*maxBlockCount=*/5, /*minBlockCount=*/5, mDataNotifier,
                *mConsumerManager, std::move(args), nullptr)
                .status(),
            pw::OkStatus());
}

TEST_F(BasicProducerTest, CreateFailureToAllocateBlockRing) {
  init(/*storageSize=*/64, /*local=*/true);
  EXPECT_EQ(
      chre::shmem_spmc_queue::Producer<std::byte>::createLocal(
          base(), size(), mQueue, mAllocator, /*blockCapacity=*/64,
          /*maxBlockCount=*/5, /*minBlockCount=*/5, mDataNotifier,
          *mConsumerManager,
          {.fn = [](void * /*context*/) { return; }, .ctx = nullptr}, nullptr)
          .status(),
      pw::Status::ResourceExhausted());
}

TEST_F(BasicProducerTest, CreateLocalFailureInvalidNotifyFn) {
  init(/*storageSize=*/1024, /*local=*/true);
  EXPECT_EQ(chre::shmem_spmc_queue::Producer<std::byte>::createLocal(
                base(), size(), mQueue, mAllocator, /*blockCapacity=*/64,
                /*maxBlockCount=*/5, /*minBlockCount=*/5, mDataNotifier,
                *mConsumerManager, {.fn = nullptr, .ctx = nullptr}, nullptr)
                .status(),
            pw::Status::InvalidArgument());
}

TEST_F(BasicProducerTest, CreateRemoteFailureInvalidNotifyFn) {
  init(/*storageSize=*/1024, /*local=*/false);
  EXPECT_EQ(chre::shmem_spmc_queue::Producer<std::byte>::createRemote(
                base(), size(), mQueue, mAllocator, /*blockCapacity=*/64,
                /*maxBlockCount=*/5, /*minBlockCount=*/5, mDataNotifier,
                *mConsumerManager, {}, nullptr)
                .status(),
            pw::Status::InvalidArgument());
}

class TestConsumerManager : public ConsumerManager {
 public:
  using ConsumerManager::ConsumerManager;
  using ConsumerManager::forAllConsumers;
};

class ConsumerManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mStorage.resize(1024);
    mAllocator.Init(pw::ByteSpan(mStorage.data(), mStorage.size()));
    auto maybeQueue = chre::shmem_spmc_queue::createQueue<std::byte, 64>(
        mAllocator, /*local=*/true);
    ASSERT_EQ(maybeQueue.status(), pw::OkStatus());
    mQueue = *maybeQueue;
    mConsumerManager.emplace(base(), size(), mAllocator);
  }

  void TearDown() override {
    if (mQueue) {
      mAllocator.Deallocate(mQueue);
    }
  }

  void *base() {
    return mStorage.data();
  }

  size_t size() {
    return mStorage.size();
  }

  std::vector<std::byte> mStorage;
  pw::allocator::FirstFitAllocator<> mAllocator;
  void *mQueue;
  std::optional<TestConsumerManager> mConsumerManager;
};

TEST_F(ConsumerManagerTest, AddConsumerSuccess) {
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

TEST_F(ConsumerManagerTest, AddConsumerFailureNullQueue) {
  pw::Result<uint32_t> result = mConsumerManager->addConsumer(nullptr);
  EXPECT_EQ(result.status(), pw::Status::InvalidArgument());
}

TEST_F(ConsumerManagerTest, AddConsumerFailureNoMemory) {
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

TEST_F(ConsumerManagerTest, RemoveConsumerSuccess) {
  pw::Result<uint32_t> result = mConsumerManager->addConsumer(mQueue);
  ASSERT_EQ(result.status(), pw::OkStatus());

  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, *result), pw::OkStatus());

  int consumerCount = 0;
  mConsumerManager->forAllConsumers(
      *static_cast<internal::Queue *>(mQueue),
      [&](internal::ConsumerDesc &) { consumerCount++; });
  EXPECT_EQ(consumerCount, 0);
}

TEST_F(ConsumerManagerTest, RemoveConsumerMultiple) {
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

TEST_F(ConsumerManagerTest, RemoveConsumerFailureInvalidOffset) {
  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, internal::kOffsetInvalid),
            pw::Status::InvalidArgument());
}

TEST_F(ConsumerManagerTest, RemoveConsumerFailureNotFound) {
  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, 12345),
            pw::Status::NotFound());
}

TEST_F(ConsumerManagerTest, RemoveConsumerFailureNullQueue) {
  pw::Result<uint32_t> result = mConsumerManager->addConsumer(mQueue);
  ASSERT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(mConsumerManager->removeConsumer(nullptr, *result),
            pw::Status::InvalidArgument());

  // Remove the consumer to avoid memory leaks.
  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, *result), pw::OkStatus());
}

}  // namespace
}  // namespace chre::shmem_spmc_queue
