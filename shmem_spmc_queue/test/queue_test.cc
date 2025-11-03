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
#include <utility>
#include <vector>

#include "chre/shmem_spmc_queue/internal/queue_internal.h"
#include "chre/shmem_spmc_queue/queue_defs.h"
#include "gtest/gtest.h"
#include "pw_allocator/first_fit.h"
#include "pw_bytes/span.h"

namespace chre::shmem_spmc_queue {

template <typename ElementType>
class ProducerPeer {
 public:
  explicit ProducerPeer(Producer<ElementType> &producer)
      : mProducer(producer) {}

  template <typename Fn, typename... Args>
  void forAllConsumers(uint16_t excludeMask, const Fn &fn, Args... args) {
    mProducer.forAllConsumers(excludeMask, fn, args...);
  }

 private:
  Producer<ElementType> &mProducer;
};

namespace {

// Checks that a Result<T> is OK and its value equals an expected one.
#define EXPECT_RESULT_EQ(res, expected)         \
  do {                                          \
    auto result = (res);                        \
    ASSERT_EQ(result.status(), pw::OkStatus()); \
    EXPECT_EQ(*result, (expected));             \
  } while (0)

constexpr LocalNotifyArgs kEmptyLocalNotifyArgs = {
    .fn = [](void * /*context*/) { return; }, .ctx = nullptr};

RemoteNotifyFn getEmptyRemoteNotifyFn() {
  return [](pw::ConstByteSpan /*id*/) { return; };
}

class QueueTest : public ::testing::Test {
 protected:
  static constexpr size_t kBlockCapacity = 32;
  static constexpr size_t kVarDataBlockCapacity = 128;
  static constexpr size_t kBaseMinBlockCount = 3;
  static constexpr size_t kBaseMaxBlockCount = 3;

  void SetUp() override {
    mStorage.resize(2048);
    mAllocator.Init(pw::ByteSpan(mStorage.data(), mStorage.size()));
    auto maybeQueue = chre::shmem_spmc_queue::createQueue<int, kBlockCapacity>(
        mAllocator, /*local=*/true);
    ASSERT_EQ(maybeQueue.status(), pw::OkStatus());
    mQueue = *maybeQueue;
    auto maybeVarDataQueue = chre::shmem_spmc_queue::createVariableDataQueue(
        mAllocator, kVarDataBlockCapacity, /*local=*/true);
    ASSERT_EQ(maybeVarDataQueue.status(), pw::OkStatus());
    mVarDataQueue = *maybeVarDataQueue;
  }

  void TearDown() override {
    mConsumers.clear();
    mProducer.reset();
    mVarDataConsumers.clear();
    mVarDataProducer.reset();
    mAllocator.Deallocate(mQueue);
    mAllocator.Deallocate(mVarDataQueue);
  }

  void setRemote() {
    static_cast<internal::Queue *>(mQueue)->localNotify = false;
  }

  uintptr_t base() {
    return reinterpret_cast<uintptr_t>(mStorage.data());
  }

  uint32_t size() {
    return mStorage.size();
  }

  pw::Result<Producer<int>> createLocalProducer(LocalNotifyArgs notifyArgs) {
    return Producer<int>::createLocal(
        {{.base = base(), .size = size()}, .allocator = &mAllocator}, mQueue,
        kBlockCapacity, kBaseMaxBlockCount, kBaseMinBlockCount, mDataNotifier,
        notifyArgs);
  }

  pw::Result<VariableDataProducer> createLocalVarDataProducer(
      LocalNotifyArgs notifyArgs) {
    return VariableDataProducer::createLocal(
        {{.base = base(), .size = size()}, .allocator = &mAllocator},
        mVarDataQueue, kVarDataBlockCapacity, kBaseMaxBlockCount,
        kBaseMinBlockCount, mDataNotifier, notifyArgs);
  }

  pw::Result<Producer<int>> createRemoteProducer(RemoteNotifyArgs notifyArgs) {
    return Producer<int>::createRemote(
        {{.base = base(), .size = size()}, .allocator = &mAllocator}, mQueue,
        kBlockCapacity, kBaseMaxBlockCount, kBaseMinBlockCount, mDataNotifier,
        std::move(notifyArgs));
  }

  pw::Result<Consumer<int>> createLocalConsumer(
      LocalNotifyArgs notifyArgs, ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(uint32_t descOffset,
                  mProducer->getConsumerManager().addConsumer());
    return Consumer<int>::createLocal(
        {.base = base(), .size = size()},
        reinterpret_cast<uintptr_t>(mQueue) - base(), descOffset, notifyArgs,
        policyBuilder);
  }

  pw::Result<VariableDataConsumer> createLocalVarDataConsumer(
      LocalNotifyArgs notifyArgs, ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(uint32_t descOffset,
                  mVarDataProducer->getConsumerManager().addConsumer());
    return VariableDataConsumer::createLocal(
        {.base = base(), .size = size()},
        reinterpret_cast<uintptr_t>(mVarDataQueue) - base(), descOffset,
        notifyArgs, policyBuilder);
  }

  pw::Result<Consumer<int>> createRemoteConsumer(
      RemoteNotifyArgs notifyArgs, ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(uint32_t descOffset,
                  mProducer->getConsumerManager().addConsumer());
    return Consumer<int>::createRemote(
        {.base = base(), .size = size()},
        reinterpret_cast<uintptr_t>(mQueue) - base(), descOffset,
        std::move(notifyArgs), policyBuilder);
  }

  void initLocalProducer(LocalNotifyArgs notifyArgs) {
    auto maybeProducer = createLocalProducer(notifyArgs);
    ASSERT_EQ(maybeProducer.status(), pw::OkStatus());
    mProducer.emplace(std::move(*maybeProducer));
  }

  void initRemoteProducer(RemoteNotifyArgs notifyArgs) {
    setRemote();
    auto maybeProducer = createRemoteProducer(std::move(notifyArgs));
    ASSERT_EQ(maybeProducer.status(), pw::OkStatus());
    mProducer.emplace(std::move(*maybeProducer));
  }

  void initLocalVarDataProducer(LocalNotifyArgs notifyArgs) {
    auto maybeProducer = createLocalVarDataProducer(notifyArgs);
    ASSERT_EQ(maybeProducer.status(), pw::OkStatus());
    mVarDataProducer.emplace(std::move(*maybeProducer));
  }

  void initRemoteConsumer(RemoteNotifyArgs notifyArgs,
                          ConsumerPolicyBuilder &policyBuilder) {
    auto maybeConsumer =
        createRemoteConsumer(std::move(notifyArgs), policyBuilder);
    ASSERT_EQ(maybeConsumer.status(), pw::OkStatus());
    mConsumers.emplace_back(std::move(*maybeConsumer));
  }

  void initLocalEndpoints(
      LocalNotifyArgs producerNotifyArgs,
      std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>>
          &consumerArgs) {
    initLocalProducer(producerNotifyArgs);
    for (auto &[consumerNotifyArgs, policyBuilder] : consumerArgs) {
      auto maybeConsumer =
          createLocalConsumer(consumerNotifyArgs, policyBuilder);
      ASSERT_EQ(maybeConsumer.status(), pw::OkStatus());
      mConsumers.emplace_back(std::move(*maybeConsumer));
    }
  }

  void initLocalVarDataEndpoints(
      LocalNotifyArgs producerNotifyArgs,
      std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>>
          &consumerArgs) {
    initLocalVarDataProducer(producerNotifyArgs);
    for (auto &[consumerNotifyArgs, policyBuilder] : consumerArgs) {
      auto maybeConsumer =
          createLocalVarDataConsumer(consumerNotifyArgs, policyBuilder);
      ASSERT_EQ(maybeConsumer.status(), pw::OkStatus());
      mVarDataConsumers.emplace_back(std::move(*maybeConsumer));
    }
  }

  // Initializes the producer with endpoint id 0 and consumers with endpoint
  // ids 1, 2, ...
  void initRemoteEndpoints(
      RemoteNotifyFn producerNotifyFn,
      std::vector<std::pair<RemoteNotifyFn, ConsumerPolicyBuilder>>
          &consumerArgs) {
    initRemoteProducer(
        {.fn = std::move(producerNotifyFn), .id = {std::byte(0)}});
    for (auto i = 0; i < consumerArgs.size(); ++i) {
      initRemoteConsumer(
          {.fn = std::move(consumerArgs[i].first), .id = {std::byte(i + 1)}},
          consumerArgs[i].second);
    }
  }

  std::vector<std::byte> mStorage;
  pw::allocator::FirstFitAllocator<> mAllocator;
  void *mQueue;
  void *mVarDataQueue;
  DataNotifier mDataNotifier;
  std::optional<Producer<int>> mProducer;
  std::optional<VariableDataProducer> mVarDataProducer;
  std::vector<Consumer<int>> mConsumers;
  std::vector<VariableDataConsumer> mVarDataConsumers;
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
  initLocalProducer(kEmptyLocalNotifyArgs);
  auto consumerManager = mProducer->getConsumerManager();

  pw::Result<uint32_t> result = consumerManager.addConsumer();
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_NE(*result, internal::kOffsetInvalid);

  int consumerCount = 0;
  ProducerPeer(*mProducer)
      .forAllConsumers(
          /*excludeMask=*/0,
          [&](internal::ConsumerDesc &, uint32_t) { consumerCount++; });
  EXPECT_EQ(consumerCount, 1);
}

TEST_F(QueueTest, ConsumerManagerAddConsumerFailureNoMemory) {
  initLocalProducer(kEmptyLocalNotifyArgs);
  auto consumerManager = mProducer->getConsumerManager();

  // Allocate all remaining memory.
  std::vector<void *> tmps;
  auto layout = pw::allocator::Layout::Of<internal::ConsumerDesc>();
  auto *tmp = mAllocator.Allocate(layout);
  while (tmp) {
    tmps.push_back(tmp);
    tmp = mAllocator.Allocate(layout);
  }
  pw::Result<uint32_t> result = consumerManager.addConsumer();
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
  for (auto *tmp : tmps) {
    mAllocator.Deallocate(tmp);
  }
}

TEST_F(QueueTest, ConsumerManagerAddConsumerSeparateRegionSuccess) {
  initLocalProducer(kEmptyLocalNotifyArgs);
  auto consumerManager = mProducer->getConsumerManager();

  // Ensure that no consumer descriptor can be allocated from the main region.
  std::vector<void *> tmps;
  void *tmp = nullptr;
  do {
    tmp = mAllocator.Allocate(
        pw::allocator::Layout::Of<internal::ConsumerDesc>());
    if (tmp) {
      tmps.push_back(tmp);
    }
  } while (tmp);
  // Make sure a consumer node can still be allocated.
  if ((tmp = mAllocator.Allocate(
           pw::allocator::Layout::Of<internal::ConsumerNode>()))) {
    mAllocator.Deallocate(tmp);
  } else {
    mAllocator.Deallocate(tmps.back());
    tmps.pop_back();
  }

  std::vector<std::byte> storage(1024);
  pw::allocator::FirstFitAllocator<> allocator(storage);
  AllocatorRegion region = {
      {.base = reinterpret_cast<uintptr_t>(storage.data()),
       .size = static_cast<uint32_t>(storage.size())},
      .allocator = &allocator};
  pw::Result<uint32_t> result = consumerManager.addConsumer(&region);
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_NE(*result, internal::kOffsetInvalid);

  int consumerCount = 0;
  ProducerPeer(*mProducer)
      .forAllConsumers(
          /*excludeMask=*/0,
          [&](internal::ConsumerDesc &, uint32_t) { consumerCount++; });
  EXPECT_EQ(consumerCount, 1);

  // Destroy the producer to ensure that the consumer is deallocated before the
  // allocator is destroyed.
  mProducer.reset();

  // Deallocate the allocated memory.
  for (auto *tmp : tmps) {
    mAllocator.Deallocate(tmp);
  }
}

TEST_F(QueueTest, ConsumerManagerPruneConsumersSuccess) {
  initRemoteProducer({.fn = getEmptyRemoteNotifyFn(), .id = {std::byte(0)}});
  std::array<std::byte, 16> consumerId = {std::byte(1)};
  initRemoteConsumer({.fn = getEmptyRemoteNotifyFn(), .id = consumerId},
                     ConsumerPolicyBuilder().setOverwritable());
  auto consumerManager = mProducer->getConsumerManager();

  EXPECT_EQ(consumerManager.pruneConsumers([&](pw::ConstByteSpan remoteId) {
    return std::memcmp(remoteId.data(), consumerId.data(), consumerId.size()) ==
           0;
  }),
            pw::OkStatus());

  int consumerCount = 0;
  ProducerPeer(*mProducer)
      .forAllConsumers(
          /*excludeMask=*/0,
          [&](internal::ConsumerDesc &, uint32_t) { consumerCount++; });
  EXPECT_EQ(consumerCount, 0);
}

TEST_F(QueueTest, ConsumerManagerPruneConsumersFailureNotRemote) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);
  auto consumerManager = mProducer->getConsumerManager();

  EXPECT_EQ(consumerManager.pruneConsumers(
                [&](pw::ConstByteSpan /*remoteId*/) { return false; }),
            pw::Status::FailedPrecondition());
}

TEST_F(QueueTest, ConsumerManagerPruneConsumersSuccessMultiple) {
  std::vector<std::pair<RemoteNotifyFn, ConsumerPolicyBuilder>> consumerArgs;
  for (int i = 0; i < 3; ++i) {
    consumerArgs.emplace_back(getEmptyRemoteNotifyFn(),
                              ConsumerPolicyBuilder().setOverwritable());
  }
  initRemoteEndpoints(getEmptyRemoteNotifyFn(), consumerArgs);
  auto consumerManager = mProducer->getConsumerManager();

  // Prune the consumers with id 1 and 3.
  std::array<std::byte, 16> consumerId = {std::byte(2)};
  EXPECT_EQ(consumerManager.pruneConsumers([&](pw::ConstByteSpan remoteId) {
    return std::memcmp(remoteId.data(), consumerId.data(), consumerId.size()) !=
           0;
  }),
            pw::OkStatus());

  int consumerCount = 0;
  bool foundConsumer2 = false;
  ProducerPeer(*mProducer)
      .forAllConsumers(
          /*excludeMask=*/0, [&](internal::ConsumerDesc &desc, uint32_t) {
            consumerCount++;
            foundConsumer2 = desc.idOrNotifyFn.remoteId == consumerId;
          });
  EXPECT_EQ(consumerCount, 1);
  EXPECT_TRUE(foundConsumer2);
}

TEST_F(QueueTest, ConsumerManagerForAllConsumersExcludeMask) {
  initLocalProducer(kEmptyLocalNotifyArgs);
  auto consumerManager = mProducer->getConsumerManager();

  pw::Result<uint32_t> result = consumerManager.addConsumer();
  ASSERT_EQ(result.status(), pw::OkStatus());

  int consumerCount = 0;
  uint32_t mask = static_cast<uint32_t>(internal::ProducerFlags::kPendingInit);
  ProducerPeer(*mProducer)
      .forAllConsumers(
          mask, [&](internal::ConsumerDesc &, uint32_t) { consumerCount++; });
  EXPECT_EQ(consumerCount, 0);
}

TEST_F(QueueTest, ConsumerCreateLocalAndDestroy) {
  // First create a Producer so that on destruction it iterates through the
  // consumers and cleans up the consumer descriptor. This also tests that
  // ConsumerManager::forAllConsumers() cleans up gracefully removed Consumers
  // as expected.
  initLocalProducer(kEmptyLocalNotifyArgs);

  ConsumerPolicyBuilder policyBuilder;
  EXPECT_EQ(createLocalConsumer(kEmptyLocalNotifyArgs, policyBuilder).status(),
            pw::OkStatus());
}

TEST_F(QueueTest, ConsumerCreateRemoteAndDestroy) {
  // First create a Producer so that on destruction it iterates through the
  // consumers and cleans up the consumer descriptor. This also tests that
  // ConsumerManager::forAllConsumers() cleans up gracefully removed Consumers
  // as expected.
  RemoteNotifyArgs args = {.fn = getEmptyRemoteNotifyFn(), .id = {std::byte(0)}};
  initRemoteProducer(std::move(args));

  ConsumerPolicyBuilder policyBuilder;
  args = {.fn = getEmptyRemoteNotifyFn(), .id = {std::byte(1)}};
  EXPECT_EQ(createRemoteConsumer(std::move(args), policyBuilder).status(),
            pw::OkStatus());
}

TEST_F(QueueTest, PushPopNonoverwritableConsumer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 1);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 1);
  EXPECT_RESULT_EQ(mConsumers[0].pop(), 1);
  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
}

TEST_F(QueueTest, PushPopOverwritableConsumer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 1);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 1);
  EXPECT_RESULT_EQ(mConsumers[0].pop(), 1);
  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
}

TEST_F(QueueTest, PushBlockedNonOverwritableConsumer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  // Fill the queue to the point of blocking the producer. Pushing a single
  // element after should fail.
  std::vector<int> data(mProducer->capacity());
  for (int i = 0; i < data.size(); ++i) {
    data[i] = i;
  }
  auto res = mProducer->push(data);
  EXPECT_EQ(res.status(), pw::OkStatus());
  EXPECT_EQ(res.value(), data.size());
  EXPECT_EQ(mProducer->size(), mProducer->capacity());
  EXPECT_EQ(mProducer->push(1), pw::Status::Unavailable());
  EXPECT_RESULT_EQ(mConsumers[0].size(), mProducer->capacity());
  std::vector<int> output(data.size());
  EXPECT_EQ(mConsumers[0].pop(output), pw::OkStatus());
  EXPECT_EQ(output, data);
  EXPECT_EQ(mProducer->size(), 0);
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
  EXPECT_RESULT_EQ(mConsumers[0].size(), mProducer->capacity());
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 0);
  EXPECT_EQ(mConsumers[0].size().status(), pw::Status::DataLoss());
  // Check that the consumer catches up to the queue capacity / 2.
  EXPECT_RESULT_EQ(mConsumers[0].size(), mProducer->capacity() / 2);
  EXPECT_EQ(mProducer->size(), mProducer->capacity() / 2);
}

TEST_F(QueueTest, ConsumerChangeOverwritePolicy) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  // Fill the queue to the point of overwriting the consumer. Pushing a single
  // element should succeed, overwriting the consumer. Since the consumer is
  // overwritten, it is ignored for calculation of the queue size.
  std::vector<int> data(mProducer->capacity());
  EXPECT_RESULT_EQ(mProducer->push(data), data.size());
  EXPECT_EQ(mProducer->size(), mProducer->capacity());
  EXPECT_RESULT_EQ(mConsumers[0].size(), mProducer->capacity());
  EXPECT_EQ(mProducer->push(1), pw::Status::Unavailable());
  EXPECT_EQ(
      mConsumers[0].updatePolicy(ConsumerPolicyBuilder().setOverwritable()),
      pw::OkStatus());
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(mConsumers[0].checkState(), pw::Status::DataLoss());
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
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
  EXPECT_EQ(mProducer->size(/*includeReserved=*/true), 1);
  EXPECT_EQ(mProducer->commit(1), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), 1);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 1);
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
  EXPECT_EQ(mProducer->reserve(1).status(), pw::Status::Unavailable());
  EXPECT_RESULT_EQ(mConsumers[0].size(), mProducer->capacity());
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
  EXPECT_EQ(mConsumers[0].size().status(), pw::Status::DataLoss());
  // Check that the consumer catches up to the queue capacity / 2.
  EXPECT_RESULT_EQ(mConsumers[0].size(), mProducer->capacity() / 2);
  EXPECT_EQ(mProducer->size(), mProducer->capacity() / 2);
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
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
  EXPECT_EQ(mProducer->commit(total), pw::OkStatus());
  EXPECT_EQ(mProducer->size(), mProducer->size(/*includeReserved=*/true));
  EXPECT_RESULT_EQ(mConsumers[0].size(), mProducer->size());
}

TEST_F(QueueTest, PopLessThanAvailable) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  ASSERT_EQ(mProducer->push(1), pw::OkStatus());
  ASSERT_EQ(mProducer->push(2), pw::OkStatus());
  EXPECT_RESULT_EQ(mConsumers[0].pop(), 1);
  EXPECT_EQ(mProducer->size(), 1);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 1);
}

TEST_F(QueueTest, PopFailsNoData) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  std::vector<int> data(1);
  EXPECT_EQ(mConsumers[0].pop(data), pw::Status::Unavailable());
}

TEST_F(QueueTest, PopFailsWithPeekedData) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  ASSERT_EQ(mProducer->push(1), pw::OkStatus());
  ASSERT_EQ(mProducer->push(2), pw::OkStatus());
  ASSERT_EQ(mConsumers[0].peek(1).status(), pw::OkStatus());
  std::vector<int> data(1);
  EXPECT_EQ(mConsumers[0].pop(data), pw::Status::FailedPrecondition());
}

TEST_F(QueueTest, ReserveCommitPeekRelease) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto maybeReserve = mProducer->reserve(1);
  ASSERT_EQ(maybeReserve.status(), pw::OkStatus());
  EXPECT_EQ(maybeReserve.value().size(), 1);
  maybeReserve.value().data()[0] = 1;
  EXPECT_EQ(mProducer->commit(1), pw::OkStatus());
  auto maybePeeked = mConsumers[0].peek(1);
  ASSERT_EQ(maybePeeked.status(), pw::OkStatus());
  EXPECT_EQ(maybeReserve.value().data(), maybePeeked.value().data());
  EXPECT_EQ(maybePeeked.value().size(), 1);
  EXPECT_EQ(maybePeeked.value()[0], 1);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 1);
  EXPECT_EQ(mConsumers[0].release(1), pw::OkStatus());
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
}

TEST_F(QueueTest, PushPeekMultipleReleaseSingle) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  ASSERT_EQ(mProducer->push(1), pw::OkStatus());
  ASSERT_EQ(mProducer->push(2), pw::OkStatus());
  auto maybePeeked = mConsumers[0].peek(1);
  ASSERT_EQ(maybePeeked.status(), pw::OkStatus());
  EXPECT_EQ(maybePeeked.value().size(), 1);
  EXPECT_EQ(maybePeeked.value()[0], 1);
  maybePeeked = mConsumers[0].peek(1);
  ASSERT_EQ(maybePeeked.status(), pw::OkStatus());
  EXPECT_EQ(maybePeeked.value().size(), 1);
  EXPECT_EQ(maybePeeked.value()[0], 2);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 2);
  EXPECT_EQ(mConsumers[0].release(2), pw::OkStatus());
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
}

TEST_F(QueueTest, PushPeekSingleReleaseMultiple) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  ASSERT_EQ(mProducer->push(1), pw::OkStatus());
  ASSERT_EQ(mProducer->push(2), pw::OkStatus());
  auto maybePeeked = mConsumers[0].peek(2);
  ASSERT_EQ(maybePeeked.status(), pw::OkStatus());
  EXPECT_EQ(maybePeeked.value().size(), 2);
  EXPECT_EQ(maybePeeked.value()[0], 1);
  EXPECT_EQ(maybePeeked.value()[1], 2);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 2);
  EXPECT_EQ(mConsumers[0].release(1), pw::OkStatus());
  EXPECT_RESULT_EQ(mConsumers[0].size(), 1);
  EXPECT_EQ(mConsumers[0].release(1), pw::OkStatus());
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
}

TEST_F(QueueTest, PeekAcrossBlocks) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  // Fill the queue to the point of blocking the producer. Pushing a single
  // element after should fail.
  std::vector<int> data(mProducer->capacity());
  for (int i = 0; i < data.size(); ++i) {
    data[i] = i;
  }
  EXPECT_RESULT_EQ(mProducer->push(data), data.size());
  EXPECT_RESULT_EQ(mConsumers[0].size(), data.size());
  auto remainder = data.size();
  while (remainder) {
    auto maybePeeked = mConsumers[0].peek(remainder);
    ASSERT_EQ(maybePeeked.status(), pw::OkStatus());
    EXPECT_EQ(std::memcmp(maybePeeked.value().data(),
                          data.data() + data.size() - remainder,
                          maybePeeked.value().size()),
              0);
    remainder -= maybePeeked.value().size();
  }
}

TEST_F(QueueTest, ConsumerResyncSuccess) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  std::vector<int> data(mProducer->capacity());
  for (int i = 0; i < data.size(); ++i) {
    data[i] = i;
  }
  EXPECT_RESULT_EQ(mProducer->push(data), data.size());
  EXPECT_EQ(mConsumers[0].resync(data.size() / 2), pw::OkStatus());
  EXPECT_RESULT_EQ(mConsumers[0].size(), data.size() / 2);
}

TEST_F(QueueTest, ConsumerResyncFailsOffsetTooLarge) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  EXPECT_EQ(mConsumers[0].resync(1), pw::Status::OutOfRange());
}

TEST_F(QueueTest, ProducerStop) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);
  auto consumerManager = mProducer->getConsumerManager();

  EXPECT_EQ(consumerManager.getNumConsumers(), 1);
  mProducer->stop();
  EXPECT_EQ(consumerManager.getNumConsumers(), 1);
  EXPECT_EQ(mConsumers[0].checkState(), pw::Status::Aborted());
  EXPECT_EQ(consumerManager.getNumConsumers(), 0);
}

TEST_F(QueueTest, VariableDataProducerPush) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);
  EXPECT_EQ(mVarDataProducer->size(), 0);

  std::vector<std::byte> data1 = {std::byte(1), std::byte(2)};
  EXPECT_EQ(mVarDataProducer->push(data1), pw::OkStatus());
  // size (16) + data (2) + padding (6) = 24
  EXPECT_EQ(mVarDataProducer->size(), 24);
  std::vector<std::byte> data2 = {std::byte(3), std::byte(4), std::byte(5)};
  EXPECT_EQ(mVarDataProducer->push(data2), pw::OkStatus());
  // 24 + size (16) + data (3) + padding (5) = 24 + 24 = 48
  EXPECT_EQ(mVarDataProducer->size(), 48);
}

TEST_F(QueueTest, VariableDataProducerReserveCommit) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto reservation = mVarDataProducer->reserve(5);
  ASSERT_EQ(reservation.status(), pw::OkStatus());
  EXPECT_EQ(reservation->size(), 5);
  // Reserved: 16 bytes for size + 5 for data. The size() method without
  // arguments does not include reserved elements.
  EXPECT_EQ(mVarDataProducer->size(), 0);
  EXPECT_EQ(mVarDataProducer->size(/*includeReserved=*/true), 21);
  EXPECT_EQ(mVarDataProducer->commit(), pw::OkStatus());
  // Committed: 16 for size + 5 for data + 3 for padding.
  EXPECT_EQ(mVarDataProducer->size(), 24);
}

TEST_F(QueueTest, VariableDataProducerReserveTruncateCommit) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto reservation = mVarDataProducer->reserve(10);
  ASSERT_EQ(reservation.status(), pw::OkStatus());
  // Reserved: 16 for size + 10 for data.
  EXPECT_EQ(mVarDataProducer->size(/*includeReserved=*/true), 26);
  EXPECT_EQ(mVarDataProducer->truncate(5), pw::OkStatus());
  // Reserved: 16 for size + 5 for data.
  EXPECT_EQ(mVarDataProducer->size(/*includeReserved=*/true), 21);
  EXPECT_EQ(mVarDataProducer->commit(), pw::OkStatus());
  // Committed: 16 for size + 5 for data + 3 for padding.
  EXPECT_EQ(mVarDataProducer->size(), 24);
}

TEST_F(QueueTest, VariableDataProducerTruncateToReservedSizeDoesNothing) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto reservation = mVarDataProducer->reserve(10);
  ASSERT_EQ(reservation.status(), pw::OkStatus());
  // Reserved: 16 for size + 10 for data.
  EXPECT_EQ(mVarDataProducer->size(/*includeReserved=*/true), 26);
  EXPECT_EQ(mVarDataProducer->truncate(10), pw::OkStatus());
  // Reserved: 16 for size + 10 for data.
  EXPECT_EQ(mVarDataProducer->size(/*includeReserved=*/true), 26);
}

TEST_F(QueueTest, VariableDataProducerPushFailsWithReservation) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  ASSERT_EQ(mVarDataProducer->reserve(1).status(), pw::OkStatus());
  std::vector<std::byte> data = {std::byte(1)};
  EXPECT_EQ(mVarDataProducer->push(data), pw::Status::FailedPrecondition());
}

TEST_F(QueueTest, VariableDataProducerCommitFailsWithoutReservation) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  EXPECT_EQ(mVarDataProducer->commit(), pw::Status::FailedPrecondition());
}

TEST_F(QueueTest, VariableDataProducerTruncateFailsWithoutReservation) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  EXPECT_EQ(mVarDataProducer->truncate(1), pw::Status::FailedPrecondition());
}

TEST_F(QueueTest, VariableDataProducerTruncateFailsSizeTooLarge) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  ASSERT_EQ(mVarDataProducer->reserve(5).status(), pw::OkStatus());
  EXPECT_EQ(mVarDataProducer->truncate(10), pw::Status::OutOfRange());
}

}  // namespace
}  // namespace chre::shmem_spmc_queue
