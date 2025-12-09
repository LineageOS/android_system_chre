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

#include <optional>
#include <utility>
#include <vector>

#include "data_flow/host/remote_consumer.h"
#include "data_flow/internal/queue_internal.h"
#include "data_flow/queue.h"
#include "data_flow/queue_defs.h"
#include "data_flow/untyped_queue.h"
#include "gtest/gtest.h"
#include "pw_allocator/first_fit.h"
#include "pw_bytes/span.h"

namespace android::contexthub::data_flow {

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

std::array<std::byte, internal::kMaxIdSize> kZeroId = {std::byte(0)};

class QueueTest : public ::testing::Test {
 protected:
  static constexpr size_t kBlockCapacity = 32;
  static constexpr size_t kVarDataBlockCapacity = 128;
  static constexpr size_t kBaseMinBlockCount = 3;
  static constexpr size_t kBaseMaxBlockCount = 3;

  void SetUp() override {
    mStorage.resize(4096);
    mAllocator.Init(pw::ByteSpan(mStorage.data(), mStorage.size()));
    auto maybeQueue =
        android::contexthub::data_flow::createQueue<int, kBlockCapacity>(
            mAllocator, /*local=*/true);
    ASSERT_EQ(maybeQueue.status(), pw::OkStatus());
    mQueue = *maybeQueue;
    auto maybeVarDataQueue =
        android::contexthub::data_flow::createVariableDataQueue(
            mAllocator, kVarDataBlockCapacity, /*local=*/true);
    ASSERT_EQ(maybeVarDataQueue.status(), pw::OkStatus());
    mVarDataQueue = *maybeVarDataQueue;
  }

  void TearDown() override {
    mConsumers.clear();
    mProducer.reset();
    mVarDataConsumers.clear();
    mVarDataProducer.reset();
    mUntypedConsumers.clear();
    mUntypedProducer.reset();
    mAllocator.Deallocate(mQueue);
    mAllocator.Deallocate(mVarDataQueue);
  }

  void setRemote() {
    static_cast<internal::Queue *>(mQueue)->localNotify = false;
    static_cast<internal::Queue *>(mVarDataQueue)->localNotify = false;
  }

  uintptr_t base() {
    return reinterpret_cast<uintptr_t>(mStorage.data());
  }

  uint32_t size() {
    return mStorage.size();
  }

  uint32_t queueOffset() {
    return reinterpret_cast<uintptr_t>(mQueue) - base();
  }

  uint32_t varDataQueueOffset() {
    return reinterpret_cast<uintptr_t>(mVarDataQueue) - base();
  }

  pw::Result<Producer<int>> createLocalProducer(LocalNotifyArgs notifyArgs) {
    return Producer<int>::createLocal(
        {{.base = base(), .size = size()}, .allocator = &mAllocator}, mQueue,
        kBaseMaxBlockCount, kBaseMinBlockCount, mDataNotifier, notifyArgs);
  }

  pw::Result<VariableDataProducer> createLocalVarDataProducer(
      LocalNotifyArgs notifyArgs) {
    return VariableDataProducer::createLocal(
        {{.base = base(), .size = size()}, .allocator = &mAllocator},
        mVarDataQueue, kBaseMaxBlockCount, kBaseMinBlockCount, mDataNotifier,
        notifyArgs);
  }

  pw::Result<VariableDataProducer> createRemoteVarDataProducer(
      RemoteNotifyArgs notifyArgs) {
    return VariableDataProducer::createRemote(
        {{.base = base(), .size = size()}, .allocator = &mAllocator},
        mVarDataQueue, kBaseMaxBlockCount, kBaseMinBlockCount, mDataNotifier,
        std::move(notifyArgs));
  }

  pw::Result<Producer<int>> createRemoteProducer(RemoteNotifyArgs notifyArgs) {
    return Producer<int>::createRemote(
        {{.base = base(), .size = size()}, .allocator = &mAllocator}, mQueue,
        kBaseMaxBlockCount, kBaseMinBlockCount, mDataNotifier,
        std::move(notifyArgs));
  }

  pw::Result<Consumer<int>> createLocalConsumer(
      pw::ConstByteSpan id, LocalNotifyArgs notifyArgs,
      ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(
        uint32_t descOffset,
        mProducer->getConsumerManager().addConsumer(id, policyBuilder));
    return Consumer<int>::createLocal({.base = base(), .size = size()},
                                      queueOffset(), descOffset, notifyArgs);
  }

  pw::Result<VariableDataConsumer> createLocalVarDataConsumer(
      pw::ConstByteSpan id, LocalNotifyArgs notifyArgs,
      ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(
        uint32_t descOffset,
        mVarDataProducer->getConsumerManager().addConsumer(id, policyBuilder));
    return VariableDataConsumer::createLocal({.base = base(), .size = size()},
                                             varDataQueueOffset(), descOffset,
                                             notifyArgs);
  }

  pw::Result<Consumer<int>> createRemoteConsumer(
      RemoteNotifyArgs notifyArgs, ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(uint32_t descOffset,
                  mProducer->getConsumerManager().addConsumer(notifyArgs.id,
                                                              policyBuilder));
    return Consumer<int>::createRemote({.base = base(), .size = size()},
                                       /*descRegion=*/{}, queueOffset(),
                                       descOffset, std::move(notifyArgs));
  }

  pw::Result<VariableDataConsumer> createRemoteVarDataConsumer(
      RemoteNotifyArgs notifyArgs, ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(uint32_t descOffset,
                  mVarDataProducer->getConsumerManager().addConsumer(
                      notifyArgs.id, policyBuilder));
    return VariableDataConsumer::createRemote(
        {.base = base(), .size = size()}, /*descRegion=*/{},
        varDataQueueOffset(), descOffset, std::move(notifyArgs));
  }

  pw::Result<VariableDataConsumer> createHostVarDataConsumer(
      RemoteNotifyArgs notifyArgs, ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(uint32_t descOffset,
                  mVarDataProducer->getConsumerManager().addConsumer(
                      notifyArgs.id, policyBuilder));
    PW_TRY_ASSIGN(auto consumer,
                  ::android::contexthub::data_flow::createRemoteConsumer(
                      {.base = base(), .size = size()}, /*descRegion=*/{},
                      varDataQueueOffset(), descOffset, std::move(notifyArgs)));
    bool isVarDataConsumer =
        std::holds_alternative<VariableDataConsumer>(consumer);
    EXPECT_TRUE(isVarDataConsumer);
    if (!isVarDataConsumer) {
      return pw::Status::Internal();
    }
    return std::get<VariableDataConsumer>(std::move(consumer));
  }

  pw::Result<UntypedConsumer> createHostUntypedConsumer(
      RemoteNotifyArgs notifyArgs, ConsumerPolicyBuilder &policyBuilder) {
    PW_TRY_ASSIGN(uint32_t descOffset,
                  mProducer->getConsumerManager().addConsumer(notifyArgs.id,
                                                              policyBuilder));
    PW_TRY_ASSIGN(auto consumer,
                  ::android::contexthub::data_flow::createRemoteConsumer(
                      {.base = base(), .size = size()}, /*descRegion=*/{},
                      queueOffset(), descOffset, std::move(notifyArgs)));
    bool isUntypedConsumer = std::holds_alternative<UntypedConsumer>(consumer);
    EXPECT_TRUE(isUntypedConsumer);
    if (!isUntypedConsumer) {
      return pw::Status::Internal();
    }
    return std::get<UntypedConsumer>(std::move(consumer));
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

  void initRemoteVarDataProducer(RemoteNotifyArgs notifyArgs) {
    static_cast<internal::Queue *>(mVarDataQueue)->localNotify = false;
    auto maybeProducer = createRemoteVarDataProducer(std::move(notifyArgs));
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

  // Initializes the producer with endpoint id 0 and consumers with endpoint
  // ids 1, 2, ...
  void initLocalEndpoints(
      LocalNotifyArgs producerNotifyArgs,
      std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>>
          &consumerArgs) {
    initLocalProducer(producerNotifyArgs);
    for (auto i = 0; i < consumerArgs.size(); ++i) {
      auto consumerId = {std::byte(i + 1)};
      auto maybeConsumer = createLocalConsumer(
          consumerId, consumerArgs[i].first, consumerArgs[i].second);
      ASSERT_EQ(maybeConsumer.status(), pw::OkStatus());
      mConsumers.emplace_back(std::move(*maybeConsumer));
    }
  }

  // Initializes the producer with endpoint id 0 and consumers with endpoint
  // ids 1, 2, ...
  void initLocalVarDataEndpoints(
      LocalNotifyArgs producerNotifyArgs,
      std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>>
          &consumerArgs) {
    initLocalVarDataProducer(producerNotifyArgs);
    for (auto i = 0; i < consumerArgs.size(); ++i) {
      auto consumerId = {std::byte(i + 1)};
      auto maybeConsumer = createLocalVarDataConsumer(
          consumerId, consumerArgs[i].first, consumerArgs[i].second);
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
  std::optional<UntypedProducer> mUntypedProducer;
  std::vector<Consumer<int>> mConsumers;
  std::vector<VariableDataConsumer> mVarDataConsumers;
  std::vector<UntypedConsumer> mUntypedConsumers;
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

  ConsumerPolicyBuilder policyBuilder;
  pw::Result<uint32_t> result =
      consumerManager.addConsumer(kZeroId, policyBuilder);
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_NE(*result, internal::kOffsetInvalid);

  int consumerCount = 0;
  ProducerPeer(*mProducer)
      .forAllConsumers(
          /*excludeMask=*/0,
          [&](internal::ConsumerNode &, uint32_t) { consumerCount++; });
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
  ConsumerPolicyBuilder policyBuilder;
  pw::Result<uint32_t> result =
      consumerManager.addConsumer(kZeroId, policyBuilder);
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
  while (!tmps.empty()) {
    tmp = mAllocator.Allocate(
        pw::allocator::Layout::Of<internal::ConsumerNode>());
    if (tmp) {
      mAllocator.Deallocate(tmp);
      break;
    }
    mAllocator.Deallocate(tmps.back());
    tmps.pop_back();
  };

  std::vector<std::byte> storage(1024);
  pw::allocator::FirstFitAllocator<> allocator(storage);
  AllocatorRegion region = {
      {.base = reinterpret_cast<uintptr_t>(storage.data()),
       .size = static_cast<uint32_t>(storage.size())},
      .allocator = &allocator};
  ConsumerPolicyBuilder policyBuilder;
  pw::Result<uint32_t> result =
      consumerManager.addConsumer(kZeroId, policyBuilder, &region);
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_NE(*result, internal::kOffsetInvalid);

  int consumerCount = 0;
  ProducerPeer(*mProducer)
      .forAllConsumers(
          /*excludeMask=*/0,
          [&](internal::ConsumerNode &, uint32_t) { consumerCount++; });
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

  EXPECT_EQ(consumerManager.pruneConsumers([&](pw::ConstByteSpan id) {
    return std::memcmp(id.data(), consumerId.data(), consumerId.size()) == 0;
  }),
            pw::OkStatus());

  int consumerCount = 0;
  ProducerPeer(*mProducer)
      .forAllConsumers(
          /*excludeMask=*/0,
          [&](internal::ConsumerNode &, uint32_t) { consumerCount++; });
  EXPECT_EQ(consumerCount, 0);
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
  EXPECT_EQ(consumerManager.pruneConsumers([&](pw::ConstByteSpan id) {
    return std::memcmp(id.data(), consumerId.data(), consumerId.size()) != 0;
  }),
            pw::OkStatus());

  int consumerCount = 0;
  bool foundConsumer2 = false;
  ProducerPeer(*mProducer)
      .forAllConsumers(
          /*excludeMask=*/0, [&](internal::ConsumerNode &node, uint32_t) {
            consumerCount++;
            foundConsumer2 = node.id == consumerId;
          });
  EXPECT_EQ(consumerCount, 1);
  EXPECT_TRUE(foundConsumer2);
}

TEST_F(QueueTest, ConsumerManagerForAllConsumersExcludeMask) {
  initLocalProducer(kEmptyLocalNotifyArgs);
  auto consumerManager = mProducer->getConsumerManager();

  ConsumerPolicyBuilder policyBuilder;
  pw::Result<uint32_t> result =
      consumerManager.addConsumer(kZeroId, policyBuilder);
  ASSERT_EQ(result.status(), pw::OkStatus());

  int consumerCount = 0;
  uint32_t mask = static_cast<uint32_t>(internal::ProducerFlags::kPendingInit);
  ProducerPeer(*mProducer)
      .forAllConsumers(
          mask, [&](internal::ConsumerNode &, uint32_t) { consumerCount++; });
  EXPECT_EQ(consumerCount, 0);
}

TEST_F(QueueTest, ConsumerCreateLocalAndDestroy) {
  // First create a Producer so that on destruction it iterates through the
  // consumers and cleans up the consumer descriptor. This also tests that
  // ConsumerManager::forAllConsumers() cleans up gracefully removed Consumers
  // as expected.
  initLocalProducer(kEmptyLocalNotifyArgs);

  ConsumerPolicyBuilder policyBuilder;
  EXPECT_EQ(createLocalConsumer(kZeroId, kEmptyLocalNotifyArgs, policyBuilder)
                .status(),
            pw::OkStatus());
}

TEST_F(QueueTest, ConsumerCreateRemoteAndDestroy) {
  // First create a Producer so that on destruction it iterates through the
  // consumers and cleans up the consumer descriptor. This also tests that
  // ConsumerManager::forAllConsumers() cleans up gracefully removed Consumers
  // as expected.
  RemoteNotifyArgs args = {.fn = getEmptyRemoteNotifyFn(),
                           .id = {std::byte(0)}};
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
  // Test consumer ids start at 1.
  std::array<std::byte, 1> id = {std::byte(1)};

  // Fill the queue to the point of overwriting the consumer. Pushing a single
  // element should succeed, overwriting the consumer. Since the consumer is
  // overwritten, it is ignored for calculation of the queue size.
  std::vector<int> data(mProducer->capacity());
  EXPECT_RESULT_EQ(mProducer->push(data), data.size());
  EXPECT_EQ(mProducer->size(), mProducer->capacity());
  EXPECT_RESULT_EQ(mConsumers[0].size(), mProducer->capacity());
  EXPECT_EQ(mProducer->push(1), pw::Status::Unavailable());
  EXPECT_EQ(mProducer->getConsumerManager().updateConsumerPolicy(
                id, ConsumerPolicyBuilder().setOverwritable()),
            pw::OkStatus());
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(mConsumers[0].checkState(), pw::Status::DataLoss());
}

TEST_F(QueueTest, ConsumerChangeOverwritePolicyNotFound) {
  initLocalProducer(kEmptyLocalNotifyArgs);
  auto consumerManager = mProducer->getConsumerManager();
  EXPECT_EQ(consumerManager.updateConsumerPolicy(
                kZeroId, ConsumerPolicyBuilder().setOverwritable()),
            pw::Status::NotFound());
}

TEST_F(QueueTest, NewConsumerSyncsToProducer) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs;
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);
  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  auto consumer =
      createLocalConsumer(kZeroId, kEmptyLocalNotifyArgs,
                          ConsumerPolicyBuilder().setNonOverwritable());
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

TEST_F(QueueTest, ConsumerResyncDoesNothingIfOffsetTooLarge) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  ASSERT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_RESULT_EQ(mConsumers[0].size(), 1);
  EXPECT_EQ(mConsumers[0].resync(2), pw::OkStatus());
  EXPECT_RESULT_EQ(mConsumers[0].size(), 1);
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

TEST_F(QueueTest, VariableDataPushPop) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);
  EXPECT_EQ(mVarDataProducer->size(), 0);

  // Push variable-length data.
  std::vector<std::byte> data1 = {std::byte(1), std::byte(2)};
  EXPECT_EQ(mVarDataProducer->push(data1), pw::OkStatus());
  // size (4) + data (2) + padding (2) = 8
  EXPECT_EQ(mVarDataProducer->size(), 8);
  std::vector<std::byte> data2 = {std::byte(3), std::byte(4), std::byte(5)};
  EXPECT_EQ(mVarDataProducer->push(data2), pw::OkStatus());
  // 8 + size (4) + data (3) + padding (1) = 8 + 8 = 16
  EXPECT_EQ(mVarDataProducer->size(), 16);

  // Pop variable-length data.
  EXPECT_RESULT_EQ(mVarDataConsumers[0].size(), 16);
  EXPECT_RESULT_EQ(mVarDataConsumers[0].getHeadSize(), 2);
  std::vector<std::byte> popped(8);
  pw::ByteSpan poppedSpan(popped.data(), popped.size());
  EXPECT_EQ(mVarDataConsumers[0].pop(poppedSpan), pw::OkStatus());
  EXPECT_EQ(poppedSpan.size(), 2);
  EXPECT_EQ(std::memcmp(popped.data(), data1.data(), poppedSpan.size()), 0);
  EXPECT_EQ(mVarDataProducer->size(), 8);
  EXPECT_RESULT_EQ(mVarDataConsumers[0].getHeadSize(), 3);
  poppedSpan = pw::ByteSpan(popped.data(), popped.size());
  EXPECT_EQ(mVarDataConsumers[0].pop(poppedSpan), pw::OkStatus());
  EXPECT_EQ(poppedSpan.size(), 3);
  EXPECT_EQ(std::memcmp(popped.data(), data2.data(), poppedSpan.size()), 0);
  EXPECT_RESULT_EQ(mVarDataConsumers[0].empty(), true);
}

TEST_F(QueueTest, VariableDataReserveCommitPop) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto reservation = mVarDataProducer->reserve(5);
  ASSERT_EQ(reservation.status(), pw::OkStatus());
  EXPECT_EQ(reservation->size(), 5);
  // Reserved: 4 bytes for size + 5 for data. The size() method without
  // arguments does not include reserved elements.
  EXPECT_EQ(mVarDataProducer->size(), 0);
  EXPECT_GE(mVarDataProducer->size(/*includeReserved=*/true), 9);
  EXPECT_EQ(mVarDataProducer->commit(), pw::OkStatus());
  // Committed: 4 for size + 5 for data + 3 for padding.
  EXPECT_EQ(mVarDataProducer->size(), 12);

  // Pop variable-length data.
  EXPECT_RESULT_EQ(mVarDataConsumers[0].size(), 12);
  EXPECT_RESULT_EQ(mVarDataConsumers[0].getHeadSize(), 5);
  std::vector<std::byte> popped(8);
  pw::ByteSpan poppedSpan(popped);
  EXPECT_EQ(mVarDataConsumers[0].pop(poppedSpan), pw::OkStatus());
  EXPECT_EQ(poppedSpan.size(), 5);
  EXPECT_EQ(std::memcmp(popped.data(), reservation->data(), poppedSpan.size()),
            0);
  EXPECT_RESULT_EQ(mVarDataConsumers[0].empty(), true);
}

TEST_F(QueueTest, VariableDataPushPeekRelease) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);
  EXPECT_EQ(mVarDataProducer->size(), 0);

  // Push variable-length data.
  std::vector<std::byte> data1 = {std::byte(1), std::byte(2)};
  EXPECT_EQ(mVarDataProducer->push(data1), pw::OkStatus());
  // size (4) + data (2) + padding (2) = 8
  EXPECT_EQ(mVarDataProducer->size(), 8);
  std::vector<std::byte> data2 = {std::byte(3), std::byte(4), std::byte(5)};
  EXPECT_EQ(mVarDataProducer->push(data2), pw::OkStatus());
  // 8 + size (4) + data (3) + padding (1) = 8 + 8 = 16
  EXPECT_EQ(mVarDataProducer->size(), 16);

  // Peek and release variable-length data.
  EXPECT_RESULT_EQ(mVarDataConsumers[0].size(), 16);

  // First element (data1)
  size_t head_size = 0;
  EXPECT_RESULT_EQ(mVarDataConsumers[0].getHeadSize(), data1.size());
  head_size = mVarDataConsumers[0].getHeadSize().value();
  std::vector<std::byte> peeked1;
  peeked1.reserve(head_size);
  size_t remaining1 = head_size;
  while (remaining1 > 0) {
    auto maybePeeked = mVarDataConsumers[0].peek();
    ASSERT_EQ(maybePeeked.status(), pw::OkStatus());
    peeked1.insert(peeked1.end(), maybePeeked->begin(), maybePeeked->end());
    remaining1 -= maybePeeked->size();
  }
  EXPECT_EQ(peeked1.size(), data1.size());
  EXPECT_EQ(std::memcmp(peeked1.data(), data1.data(), data1.size()), 0);
  EXPECT_EQ(mVarDataConsumers[0].release(), pw::OkStatus());
  EXPECT_EQ(mVarDataProducer->size(), 8);

  // Second element (data2)
  EXPECT_RESULT_EQ(mVarDataConsumers[0].getHeadSize(), data2.size());
  head_size = mVarDataConsumers[0].getHeadSize().value();
  std::vector<std::byte> peeked2;
  peeked2.reserve(head_size);
  size_t remaining2 = head_size;
  while (remaining2 > 0) {
    auto maybePeeked = mVarDataConsumers[0].peek();
    ASSERT_EQ(maybePeeked.status(), pw::OkStatus());
    peeked2.insert(peeked2.end(), maybePeeked->begin(), maybePeeked->end());
    remaining2 -= maybePeeked->size();
  }
  EXPECT_EQ(peeked2.size(), data2.size());
  EXPECT_EQ(std::memcmp(peeked2.data(), data2.data(), data2.size()), 0);
  EXPECT_EQ(mVarDataConsumers[0].release(), pw::OkStatus());

  EXPECT_RESULT_EQ(mVarDataConsumers[0].empty(), true);
}

TEST_F(QueueTest, VariableDataConsumerReleaseWithoutPeek) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);
  EXPECT_EQ(mVarDataProducer->size(), 0);

  // Push variable-length data.
  std::vector<std::byte> data = {std::byte(1), std::byte(2), std::byte(3)};
  EXPECT_EQ(mVarDataProducer->push(data), pw::OkStatus());
  EXPECT_EQ(mVarDataProducer->size(), 8);

  // Release without peeking.
  EXPECT_EQ(mVarDataConsumers[0].release(), pw::OkStatus());
  EXPECT_RESULT_EQ(mVarDataConsumers[0].empty(), true);
}

TEST_F(QueueTest, VariableDataPushPopIncreasingSize) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  // Fill the queue with elements of increasing size.
  std::vector<std::vector<std::byte>> pushedData;
  size_t totalSize = 0;
  for (size_t i = 1;; ++i) {
    std::vector<std::byte> data(i);
    std::fill(data.begin(), data.end(), std::byte(i));
    if (auto status = mVarDataProducer->push(data); !status.ok()) {
      EXPECT_EQ(status, pw::Status::Unavailable());
      break;
    }
    pushedData.push_back(data);
    totalSize += data.size();
    EXPECT_GE(mVarDataProducer->size(), totalSize);
  }

  // Verify the elements from the consumer side.
  for (const auto &expectedData : pushedData) {
    EXPECT_RESULT_EQ(mVarDataConsumers[0].getHeadSize(), expectedData.size());
    std::vector<std::byte> poppedData(expectedData.size());
    pw::ByteSpan poppedSpan(poppedData);
    ASSERT_EQ(mVarDataConsumers[0].pop(poppedSpan), pw::OkStatus());

    EXPECT_EQ(poppedSpan.size(), expectedData.size());
    EXPECT_EQ(std::memcmp(poppedSpan.data(), expectedData.data(),
                          expectedData.size()),
              0);
  }

  EXPECT_RESULT_EQ(mVarDataConsumers[0].empty(), true);
}

TEST_F(QueueTest, VariableDataConsumerResyncAcrossBlocks) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  // Push a large element that leaves just enough space in the first block to
  // prevent the next element header from fitting, forcing a block wrap.
  constexpr size_t kRemainingSpace = sizeof(internal::VariableDataHeader) - 1;
  size_t data1_size = kVarDataBlockCapacity -
                      sizeof(internal::VariableDataHeader) - kRemainingSpace;
  std::vector<std::byte> data1(data1_size, std::byte(0xAA));
  ASSERT_EQ(mVarDataProducer->push(data1), pw::OkStatus());

  // Push a second, smaller element that will start in the next block.
  std::vector<std::byte> data2(16, std::byte(0xBB));
  ASSERT_EQ(mVarDataProducer->push(data2), pw::OkStatus());

  // Resync to keep only the second element.
  ASSERT_EQ(mVarDataConsumers[0].resync(data2.size() +
                                        sizeof(internal::VariableDataHeader)),
            pw::OkStatus());

  // Verify that the consumer now only sees the second element.
  EXPECT_RESULT_EQ(mVarDataConsumers[0].getHeadSize(), data2.size());
}

TEST_F(QueueTest, VariableDataProducerReserveTruncateCommit) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto reservation = mVarDataProducer->reserve(10);
  ASSERT_EQ(reservation.status(), pw::OkStatus());
  // Reserved: 4 for size + 10 for data.
  EXPECT_EQ(mVarDataProducer->size(/*includeReserved=*/true), 14);
  EXPECT_EQ(mVarDataProducer->truncate(5), pw::OkStatus());
  // Reserved: 4 for size + 5 for data.
  EXPECT_EQ(mVarDataProducer->size(/*includeReserved=*/true), 9);
  EXPECT_EQ(mVarDataProducer->commit(), pw::OkStatus());
  // Committed: 4 for size + 5 for data + 3 for padding.
  EXPECT_EQ(mVarDataProducer->size(), 12);
  EXPECT_RESULT_EQ(mVarDataConsumers[0].size(), 12);
}

TEST_F(QueueTest, VariableDataProducerTruncateToReservedSizeDoesNothing) {
  std::vector<std::pair<LocalNotifyArgs, ConsumerPolicyBuilder>> consumerArgs =
      {{kEmptyLocalNotifyArgs, ConsumerPolicyBuilder().setNonOverwritable()}};
  initLocalVarDataEndpoints(kEmptyLocalNotifyArgs, consumerArgs);

  auto reservation = mVarDataProducer->reserve(10);
  ASSERT_EQ(reservation.status(), pw::OkStatus());
  // Reserved: 4 for size + 10 for data.
  EXPECT_EQ(mVarDataProducer->size(/*includeReserved=*/true), 14);
  EXPECT_EQ(mVarDataProducer->truncate(10), pw::OkStatus());
  // Reserved: 4 for size + 10 for data.
  EXPECT_EQ(mVarDataProducer->size(/*includeReserved=*/true), 14);
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

TEST_F(QueueTest, UntypedProducerAndTypedConsumer) {
  auto maybeProducer = UntypedProducer::createLocal(
      {{.base = base(), .size = size()}, .allocator = &mAllocator}, mQueue,
      kBaseMaxBlockCount, kBaseMinBlockCount, mDataNotifier,
      kEmptyLocalNotifyArgs);
  ASSERT_EQ(maybeProducer.status(), pw::OkStatus());
  mUntypedProducer.emplace(std::move(*maybeProducer));
  ASSERT_EQ(mUntypedProducer->getElementSize(), sizeof(int));
  ASSERT_EQ(mUntypedProducer->getElementAlignment(), alignof(int));

  pw::Result<uint32_t> descOffsetResult =
      mUntypedProducer->getConsumerManager().addConsumer(
          kZeroId, ConsumerPolicyBuilder().setNonOverwritable());
  ASSERT_EQ(descOffsetResult.status(), pw::OkStatus());

  auto maybeConsumer =
      Consumer<int>::createLocal({.base = base(), .size = size()},
                                 reinterpret_cast<uintptr_t>(mQueue) - base(),
                                 *descOffsetResult, kEmptyLocalNotifyArgs);
  ASSERT_EQ(maybeConsumer.status(), pw::OkStatus());
  mConsumers.emplace_back(std::move(*maybeConsumer));

  int pushedValue = 123;
  pw::ConstByteSpan data = pw::as_bytes(pw::span(&pushedValue, 1));
  EXPECT_RESULT_EQ(mUntypedProducer->push(data), 1);

  EXPECT_RESULT_EQ(mConsumers[0].size(), 1);
  EXPECT_RESULT_EQ(mConsumers[0].pop(), pushedValue);
  EXPECT_RESULT_EQ(mConsumers[0].size(), 0);
}

TEST_F(QueueTest, TypedProducerAndUntypedConsumer) {
  initLocalProducer(kEmptyLocalNotifyArgs);

  pw::Result<uint32_t> descOffsetResult =
      mProducer->getConsumerManager().addConsumer(
          kZeroId, ConsumerPolicyBuilder().setNonOverwritable());
  ASSERT_EQ(descOffsetResult.status(), pw::OkStatus());

  auto maybeConsumer =
      UntypedConsumer::createLocal({.base = base(), .size = size()},
                                   reinterpret_cast<uintptr_t>(mQueue) - base(),
                                   *descOffsetResult, kEmptyLocalNotifyArgs);
  ASSERT_EQ(maybeConsumer.status(), pw::OkStatus());
  mUntypedConsumers.emplace_back(std::move(*maybeConsumer));
  ASSERT_EQ(mUntypedConsumers[0].getElementSize(), sizeof(int));
  ASSERT_EQ(mUntypedConsumers[0].getElementAlignment(), alignof(int));

  int pushedValue = 456;
  EXPECT_EQ(mProducer->push(pushedValue), pw::OkStatus());

  EXPECT_RESULT_EQ(mUntypedConsumers[0].size(), 1);
  int poppedValue;
  pw::ByteSpan data = pw::as_writable_bytes(pw::span(&poppedValue, 1));
  EXPECT_EQ(mUntypedConsumers[0].pop(data), pw::OkStatus());
  EXPECT_EQ(poppedValue, pushedValue);
  EXPECT_RESULT_EQ(mUntypedConsumers[0].size(), 0);
}

TEST_F(QueueTest, UntypedProducerAndUntypedConsumer) {
  auto maybeProducer = UntypedProducer::createLocal(
      {{.base = base(), .size = size()}, .allocator = &mAllocator}, mQueue,
      kBaseMaxBlockCount, kBaseMinBlockCount, mDataNotifier,
      kEmptyLocalNotifyArgs);
  ASSERT_EQ(maybeProducer.status(), pw::OkStatus());
  mUntypedProducer.emplace(std::move(*maybeProducer));

  pw::Result<uint32_t> descOffsetResult =
      mUntypedProducer->getConsumerManager().addConsumer(
          kZeroId, ConsumerPolicyBuilder().setNonOverwritable());
  ASSERT_EQ(descOffsetResult.status(), pw::OkStatus());

  auto maybeConsumer =
      UntypedConsumer::createLocal({.base = base(), .size = size()},
                                   reinterpret_cast<uintptr_t>(mQueue) - base(),
                                   *descOffsetResult, kEmptyLocalNotifyArgs);
  ASSERT_EQ(maybeConsumer.status(), pw::OkStatus());
  mUntypedConsumers.emplace_back(std::move(*maybeConsumer));

  ASSERT_EQ(mUntypedProducer->getElementSize(), sizeof(int));
  ASSERT_EQ(mUntypedProducer->getElementAlignment(), alignof(int));
  ASSERT_EQ(mUntypedConsumers[0].getElementSize(), sizeof(int));
  ASSERT_EQ(mUntypedConsumers[0].getElementAlignment(), alignof(int));

  int pushedValue = 789;
  pw::ConstByteSpan pushData = pw::as_bytes(pw::span(&pushedValue, 1));
  EXPECT_RESULT_EQ(mUntypedProducer->push(pushData), 1);

  EXPECT_RESULT_EQ(mUntypedConsumers[0].size(), 1);
  int poppedValue;
  pw::ByteSpan popData = pw::as_writable_bytes(pw::span(&poppedValue, 1));
  EXPECT_EQ(mUntypedConsumers[0].pop(popData), pw::OkStatus());
  EXPECT_EQ(poppedValue, pushedValue);
  EXPECT_RESULT_EQ(mUntypedConsumers[0].size(), 0);
}

TEST_F(QueueTest, CreateQueueUntypedIsSameAsCreateQueue) {
  auto maybeTypedQueue = createQueue<int, 4>(mAllocator, /*local=*/true);
  ASSERT_EQ(maybeTypedQueue.status(), pw::OkStatus());
  auto maybeUntypedQueue =
      createQueueUntyped(mAllocator, /*blockCapacity=*/4, sizeof(int),
                         alignof(int), /*local=*/true);
  ASSERT_EQ(maybeUntypedQueue.status(), pw::OkStatus());
  EXPECT_EQ(std::memcmp(*maybeTypedQueue, *maybeUntypedQueue,
                        sizeof(internal::Queue)),
            0);
  mAllocator.Deallocate(*maybeTypedQueue);
  mAllocator.Deallocate(*maybeUntypedQueue);
}

TEST_F(QueueTest, RemoteNotificationNever) {
  static int notificationCount = 0;
  RemoteNotifyFn notifyFn = [](pw::ConstByteSpan) { notificationCount++; };
  std::vector<std::pair<RemoteNotifyFn, ConsumerPolicyBuilder>> consumerArgs;
  consumerArgs.emplace_back(
      getEmptyRemoteNotifyFn(),
      ConsumerPolicyBuilder().setNonOverwritable().setNeverNotify());
  initRemoteEndpoints(std::move(notifyFn), consumerArgs);

  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(notificationCount, 0);
}

TEST_F(QueueTest, RemoteNotificationStreaming) {
  static int notificationCount = 0;
  RemoteNotifyFn notifyFn = [](pw::ConstByteSpan) { notificationCount++; };
  std::vector<std::pair<RemoteNotifyFn, ConsumerPolicyBuilder>> consumerArgs;
  consumerArgs.emplace_back(
      getEmptyRemoteNotifyFn(),
      ConsumerPolicyBuilder().setNonOverwritable().setStreaming());
  initRemoteEndpoints(std::move(notifyFn), consumerArgs);

  EXPECT_EQ(mProducer->push(1), pw::OkStatus());
  EXPECT_EQ(notificationCount, 1);
  EXPECT_EQ(mProducer->push(2), pw::OkStatus());
  EXPECT_EQ(notificationCount, 2);
}

TEST_F(QueueTest, RemoteNotificationHighWaterMark) {
  static int notificationCount = 0;
  RemoteNotifyFn notifyFn = [](pw::ConstByteSpan) { notificationCount++; };
  constexpr size_t kHighWatermark = 4;
  std::vector<std::pair<RemoteNotifyFn, ConsumerPolicyBuilder>> consumerArgs;
  consumerArgs.emplace_back(
      getEmptyRemoteNotifyFn(),
      ConsumerPolicyBuilder().setNonOverwritable().setHighWaterMark(
          kHighWatermark));
  initRemoteEndpoints(std::move(notifyFn), consumerArgs);

  for (size_t i = 0; i < kHighWatermark - 1; ++i) {
    EXPECT_EQ(mProducer->push(0), pw::OkStatus());
    EXPECT_EQ(notificationCount, 0);
  }

  // Crossing the watermark should trigger a notification.
  EXPECT_EQ(mProducer->push(0), pw::OkStatus());
  EXPECT_EQ(notificationCount, 1);

  // Every subsequent push should also trigger a notification.
  EXPECT_EQ(mProducer->push(0), pw::OkStatus());
  EXPECT_EQ(notificationCount, 2);

  // Pop three elements so that a subsequent push will not cross the watermark.
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(mConsumers[0].pop().status(), pw::OkStatus());
  }
  EXPECT_EQ(mProducer->push(0), pw::OkStatus());
  EXPECT_EQ(notificationCount, 2);

  // Push another element, which should trigger a notification.
  EXPECT_EQ(mProducer->push(0), pw::OkStatus());
  EXPECT_EQ(notificationCount, 3);
}

TEST_F(QueueTest, CreateRemoteConsumerForFixedSizeQueue) {
  RemoteNotifyArgs producerArgs = {.fn = getEmptyRemoteNotifyFn(),
                                   .id = {std::byte(0)}};
  initRemoteProducer(std::move(producerArgs));

  ConsumerPolicyBuilder policyBuilder;
  RemoteNotifyArgs consumerArgs = {.fn = getEmptyRemoteNotifyFn(),
                                   .id = {std::byte(1)}};
  auto consumer =
      createHostUntypedConsumer(std::move(consumerArgs), policyBuilder);
  ASSERT_EQ(consumer.status(), pw::OkStatus());
  EXPECT_EQ(consumer->getElementSize(), sizeof(int));
  EXPECT_EQ(consumer->getElementAlignment(), alignof(int));
}

TEST_F(QueueTest, CreateRemoteConsumerForVariableSizeQueue) {
  RemoteNotifyArgs producerArgs = {.fn = getEmptyRemoteNotifyFn(),
                                   .id = {std::byte(0)}};
  initRemoteVarDataProducer(std::move(producerArgs));

  ConsumerPolicyBuilder policyBuilder;
  RemoteNotifyArgs consumerArgs = {.fn = getEmptyRemoteNotifyFn(),
                                   .id = {std::byte(1)}};
  EXPECT_EQ(createHostVarDataConsumer(std::move(consumerArgs), policyBuilder)
                .status(),
            pw::OkStatus());
}

}  // namespace
}  // namespace android::contexthub::data_flow
