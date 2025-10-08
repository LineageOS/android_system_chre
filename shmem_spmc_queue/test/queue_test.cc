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

#include <optional>
#include <vector>

#include "chre/shmem_spmc_queue/queue_defs.h"
#include "gtest/gtest.h"
#include "pw_allocator/first_fit.h"
#include "pw_bytes/span.h"

namespace chre::shmem_spmc_queue {
namespace {

constexpr LocalNotifyArgs kEmptyLocalNotifyArgs = {
    .fn = [](void * /*context*/) { return; }, .ctx = nullptr};

RemoteNotifyFn getEmptyRemoteNotifyFn() {
  return [](pw::ConstByteSpan /*id*/) { return; };
}

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
    mConsumers.clear();
    mProducer.reset();
    mAllocator.Deallocate(mQueue);
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

  pw::Result<Consumer<int>> createLocalConsumer(
      LocalNotifyArgs notifyArgs, ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(uint32_t descOffset, mConsumerManager->addConsumer(mQueue));
    return Consumer<int>::createLocal(
        basePtr(), size(), reinterpret_cast<uintptr_t>(mQueue) - base(),
        descOffset, notifyArgs, policyBuilder);
  }

  pw::Result<Consumer<int>> createRemoteConsumer(
      RemoteNotifyArgs notifyArgs, ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(uint32_t descOffset, mConsumerManager->addConsumer(mQueue));
    return Consumer<int>::createRemote(
        basePtr(), size(), reinterpret_cast<uintptr_t>(mQueue) - base(),
        descOffset, std::move(notifyArgs), policyBuilder);
  }

  void initLocalEndpoints(
      LocalNotifyArgs producerNotifyArgs,
      std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>>
          &consumerArgs) {
    auto maybeProducer = createLocalProducer(producerNotifyArgs);
    ASSERT_EQ(maybeProducer.status(), pw::OkStatus());
    mProducer.emplace(std::move(*maybeProducer));
    for (auto &[consumerNotifyArgs, policyBuilder] : consumerArgs) {
      auto maybeConsumer =
          createLocalConsumer(consumerNotifyArgs, policyBuilder);
      ASSERT_EQ(maybeConsumer.status(), pw::OkStatus());
      mConsumers.emplace_back(std::move(*maybeConsumer));
    }
  }

  void initRemoteEndpoints(
      RemoteNotifyArgs producerNotifyArgs,
      std::vector<std::pair<RemoteNotifyFn, ConsumerPolicyBuilder>>
          &consumerArgs) {
    setRemote();
    auto maybeProducer = createRemoteProducer(std::move(producerNotifyArgs));
    ASSERT_EQ(maybeProducer.status(), pw::OkStatus());
    mProducer.emplace(std::move(*maybeProducer));
    for (auto i = 0; i < consumerArgs.size(); ++i) {
      auto maybeConsumer = createRemoteConsumer(
          {.fn = std::move(consumerArgs[i].first), .id = {std::byte(i + 1)}},
          consumerArgs[i].second);
      ASSERT_EQ(maybeConsumer.status(), pw::OkStatus());
      mConsumers.emplace_back(std::move(*maybeConsumer));
    }
  }

  std::vector<std::byte> mStorage;
  pw::allocator::FirstFitAllocator<> mAllocator;
  void *mQueue;
  DataNotifier mDataNotifier;
  std::optional<TestConsumerManager> mConsumerManager;
  std::optional<Producer<int>> mProducer;
  // Ensure Consumers are destroyed before the Producer so that the Producer
  // cleans them up on destruction.
  std::vector<Consumer<int>> mConsumers;
};

TEST_F(QueueTest, ProducerCreateLocalAndDestroy) {
  EXPECT_EQ(createLocalProducer(kEmptyLocalNotifyArgs).status(),
            pw::OkStatus());
}

TEST_F(QueueTest, ProducerCreateRemoteAndDestroy) {
  setRemote();
  RemoteNotifyArgs args = {.fn = getEmptyRemoteNotifyFn(),
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
  EXPECT_EQ(createLocalProducer(kEmptyLocalNotifyArgs).status(),
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
      *static_cast<internal::Queue *>(mQueue), /*excludeMask=*/0,
      [&](internal::ConsumerDesc &, uint32_t) { consumerCount++; });
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
      *static_cast<internal::Queue *>(mQueue), /*excludeMask=*/0,
      [&](internal::ConsumerDesc &, uint32_t) { consumerCount++; });
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
  mConsumerManager->forAllConsumers(
      *static_cast<internal::Queue *>(mQueue), /*excludeMask=*/0,
      [&](internal::ConsumerDesc &desc, uint32_t) {
        consumerCount++;
        foundOffset =
            internal::toOffset(reinterpret_cast<uintptr_t>(base()), &desc);
      });
  EXPECT_EQ(consumerCount, 1);
  EXPECT_EQ(foundOffset, *result2);

  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, *result2), pw::OkStatus());

  consumerCount = 0;
  mConsumerManager->forAllConsumers(
      *static_cast<internal::Queue *>(mQueue), /*excludeMask=*/0,
      [&](internal::ConsumerDesc &, uint32_t) { consumerCount++; });
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

TEST_F(QueueTest, ConsumerManagerForAllConsumersExcludeMask) {
  pw::Result<uint32_t> result = mConsumerManager->addConsumer(mQueue);
  ASSERT_EQ(result.status(), pw::OkStatus());

  int consumerCount = 0;
  uint32_t mask = static_cast<uint32_t>(internal::ProducerFlags::kPendingInit);
  mConsumerManager->forAllConsumers(
      *static_cast<internal::Queue *>(mQueue), mask,
      [&](internal::ConsumerDesc &, uint32_t) { consumerCount++; });
  EXPECT_EQ(consumerCount, 0);

  // Remove the consumer to avoid memory leaks.
  EXPECT_EQ(mConsumerManager->removeConsumer(mQueue, *result), pw::OkStatus());
}

TEST_F(QueueTest, ConsumerCreateLocalAndDestroy) {
  // First create a Producer so that on destruction it iterates through the
  // consumers and cleans up the consumer descriptor. This also tests that
  // ConsumerManager::forAllConsumers() cleans up gracefully removed Consumers
  // as expected.
  auto producer = createLocalProducer(kEmptyLocalNotifyArgs);
  ASSERT_EQ(producer.status(), pw::OkStatus());

  ConsumerPolicyBuilder policyBuilder;
  EXPECT_EQ(createLocalConsumer(kEmptyLocalNotifyArgs, policyBuilder).status(),
            pw::OkStatus());
}

TEST_F(QueueTest, ConsumerCreateRemoteAndDestroy) {
  // First create a Producer so that on destruction it iterates through the
  // consumers and cleans up the consumer descriptor. This also tests that
  // ConsumerManager::forAllConsumers() cleans up gracefully removed Consumers
  // as expected.
  setRemote();
  RemoteNotifyArgs args = {getEmptyRemoteNotifyFn(), .id = {std::byte(0)}};
  auto producer = createRemoteProducer(std::move(args));
  ASSERT_EQ(producer.status(), pw::OkStatus());

  ConsumerPolicyBuilder policyBuilder;
  args = {.fn = [](pw::ConstByteSpan /*id*/) { return; }, .id = {std::byte(1)}};
  EXPECT_EQ(createRemoteConsumer(std::move(args), policyBuilder).status(),
            pw::OkStatus());
}

TEST_F(QueueTest, PushNonoverwritableConsumer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 1);
}

TEST_F(QueueTest, PushOverwritableConsumer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 1);
}

TEST_F(QueueTest, PushBlockedNonOverwritableConsumer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  // Fill the queue to the point of blocking the producer. Pushing a single
  // element after should fail.
  std::vector<int> data(mProducer->capacity());
  auto res = mProducer->push(data);
  EXPECT_EQ(res.status(), pw::OkStatus());
  EXPECT_EQ(res.value(), data.size());
  EXPECT_EQ(mProducer->size(), mProducer->capacity());
  EXPECT_EQ(mProducer->push(1), pw::Status::ResourceExhausted());
}

TEST_F(QueueTest, PushBlockedOverwritableConsumer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  // Fill the queue to the point of overwriting the consumer. Pushing a single
  // element should succeed, overwriting the consumer. Since the consumer is
  // overwritten, it is ignored for calculation of the queue size.
  std::vector<int> data(mProducer->capacity());
  auto res = mProducer->push(data);
  EXPECT_EQ(res.status(), pw::OkStatus());
  EXPECT_EQ(res.value(), data.size());
  EXPECT_EQ(mProducer->size(), mProducer->capacity());
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 0);
}

TEST_F(QueueTest, NewConsumerSyncsToProducer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs;
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  auto consumer = createLocalConsumer(
      kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable());
  ASSERT_EQ(consumer.status(), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 0);
}

TEST_F(QueueTest, ReserveAndCommit) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto maybeRes = mProducer->reserve(1);
  ASSERT_EQ(maybeRes.status(), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_EQ(mProducer->size(/*includeReserved=*/true), 1);
  EXPECT_EQ(mProducer->commit(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 1);
}

TEST_F(QueueTest, PushFailsWithReservation) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs;
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto maybeRes = mProducer->reserve(1);
  ASSERT_EQ(maybeRes.status(), pw::OkStatus());
  EXPECT_EQ(mProducer->push(1), pw::Status::FailedPrecondition());
}

TEST_F(QueueTest, MultipleReserveSingleCommit) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto maybeRes = mProducer->reserve(1);
  ASSERT_EQ(maybeRes.status(), pw::OkStatus());
  EXPECT_EQ(maybeRes.value().size(), 1);
  EXPECT_EQ(mProducer->size(/*includeReserved=*/true), 1);
  maybeRes = mProducer->reserve(1);
  ASSERT_EQ(maybeRes.status(), pw::OkStatus());
  EXPECT_EQ(mProducer->size(/*includeReserved=*/true), 2);
  EXPECT_EQ(mProducer->commit(2), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 2);
}

TEST_F(QueueTest, SingleReserveMultipleCommit) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto maybeRes = mProducer->reserve(2);
  ASSERT_EQ(maybeRes.status(), pw::OkStatus());
  EXPECT_EQ(maybeRes.value().size(), 2);
  EXPECT_EQ(mProducer->size(/*includeReserved=*/true), 2);
  EXPECT_EQ(mProducer->commit(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 1);
  EXPECT_EQ(mProducer->commit(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 2);
}

TEST_F(QueueTest, CommitFailsMoreThanReserved) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto maybeRes = mProducer->reserve(1);
  ASSERT_EQ(maybeRes.status(), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_EQ(mProducer->size(/*includeReserved=*/true), 1);
  EXPECT_EQ(mProducer->commit(2), pw::Status::OutOfRange());
  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_EQ(mProducer->size(/*includeReserved=*/true), 1);
}

TEST_F(QueueTest, ReserveBlockedNonOverwritableConsumer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  // Fill the queue to the point of blocking the producer. Reserving a single
  // element after should fail.
  std::vector<int> data(mProducer->capacity());
  auto res = mProducer->push(data);
  EXPECT_EQ(res.status(), pw::OkStatus());
  EXPECT_EQ(res.value(), data.size());
  EXPECT_EQ(mProducer->size(), mProducer->capacity());
  EXPECT_EQ(mProducer->reserve(1).status(), pw::Status::ResourceExhausted());
}

TEST_F(QueueTest, ReserveBlockedOverwritableConsumer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  // Fill the queue to the point of overwriting the consumer. Reserving a single
  // element should succeed, overwriting the consumer. Since the consumer is
  // overwritten, it is ignored for calculation of the queue size.
  std::vector<int> data(mProducer->capacity());
  auto res = mProducer->push(data);
  EXPECT_EQ(res.status(), pw::OkStatus());
  EXPECT_EQ(res.value(), data.size());
  EXPECT_EQ(mProducer->size(), mProducer->capacity());
  EXPECT_EQ(mProducer->reserve(1).status(), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 0);
}

TEST_F(QueueTest, ReservationBrokenUpAcrossBlocks) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto total = mProducer->capacity();
  auto count = total;
  while (count > 0) {
    auto maybeRes = mProducer->reserve(count);
    ASSERT_EQ(maybeRes.status(), pw::OkStatus());
    EXPECT_EQ(maybeRes.value().size(), kBlockCapacity);
    count -= maybeRes.value().size();
  }
  EXPECT_EQ(mProducer->commit(total), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), mProducer->size(/*includeReserved=*/true));
}

}  // namespace
}  // namespace chre::shmem_spmc_queue
