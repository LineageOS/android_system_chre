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

  void init(size_t storageSize) {
    mStorage.resize(storageSize);
    mAllocator.Init(pw::ByteSpan(mStorage.data(), mStorage.size()));
    mQueue = mAllocator.Allocate(chre::shmem_spmc_queue::queueLayout());
    ASSERT_NE(mQueue, nullptr);
    mConsumerManager.emplace(0, mQueue, mAllocator);
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
  init(/*storageSize=*/1024);
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
  init(/*storageSize=*/1024);
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
  init(/*storageSize=*/64);
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
  init(/*storageSize=*/1024);
  EXPECT_EQ(chre::shmem_spmc_queue::Producer<std::byte>::createLocal(
                base(), size(), mQueue, mAllocator, /*blockCapacity=*/64,
                /*maxBlockCount=*/5, /*minBlockCount=*/5, mDataNotifier,
                *mConsumerManager, {.fn = nullptr, .ctx = nullptr}, nullptr)
                .status(),
            pw::Status::InvalidArgument());
}

TEST_F(BasicProducerTest, CreateRemoteFailureInvalidNotifyFn) {
  init(/*storageSize=*/1024);
  EXPECT_EQ(chre::shmem_spmc_queue::Producer<std::byte>::createRemote(
                base(), size(), mQueue, mAllocator, /*blockCapacity=*/64,
                /*maxBlockCount=*/5, /*minBlockCount=*/5, mDataNotifier,
                *mConsumerManager, {}, nullptr)
                .status(),
            pw::Status::InvalidArgument());
}

}  // namespace
}  // namespace chre::shmem_spmc_queue
