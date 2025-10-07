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

#include "chre/shmem_spmc_queue/internal/queue_internal.h"
#include "chre/shmem_spmc_queue/queue_defs.h"
#include "pw_allocator/layout.h"

namespace chre::shmem_spmc_queue {
namespace internal {
namespace {

constexpr uint32_t kFlagCountInc = 0x10000;
constexpr uint32_t kFlagCountMask = 0xffff0000;

void deallocateBlockRing(uintptr_t shmemBase, uint32_t shmemSize,
                         pw::Allocator &allocator, pw::allocator::Layout layout,
                         BlockHeader *head) {
  if (!head) {
    return;
  }
  for (BlockHeader *block = head; block;) {
    auto *tmp = block;
    block = fromOffset<BlockHeader>(shmemBase, shmemSize,
                                    block->nextBlockOffset, layout);
    allocator.Deallocate(tmp);
    if (block == head) {
      return;
    }
  }
}

pw::Result<BlockHeader *> allocateBlockRing(uintptr_t shmemBase,
                                            uint32_t shmemSize,
                                            pw::Allocator &allocator,
                                            pw::allocator::Layout layout,
                                            size_t count) {
  if (count == 0) {
    return pw::Status::InvalidArgument();
  }
  BlockHeader *head = nullptr, *prev = nullptr;
  for (int i = 0; i < count; ++i) {
    if (auto *blockRaw = allocator.Allocate(layout); blockRaw) {
      auto *block = static_cast<BlockHeader *>(blockRaw);
      if (!head) {
        head = block;
      } else {
        prev->nextBlockOffset = toOffset(shmemBase, block);
      }
      prev = block;
    } else {
      deallocateBlockRing(shmemBase, shmemSize, allocator, layout, head);
      return pw::Status::ResourceExhausted();
    }
  }
  prev->nextBlockOffset = internal::toOffset(shmemBase, head);
  return head;
}

void notify(IdOrNotifyFn &idOrNotifyFn, const RemoteNotifyFn &remoteNotifyFn,
            bool local) {
  if (local) {
    idOrNotifyFn.localNotify.fn(idOrNotifyFn.localNotify.ctx);
  } else {
    remoteNotifyFn(pw::ConstByteSpan(idOrNotifyFn.remoteId));
  }
}

constexpr uint32_t getFlagsCounter(uint32_t flags) {
  return flags & kFlagCountMask;
}

constexpr ProducerFlags getProducerFlags(uint32_t producerFlags) {
  return static_cast<ProducerFlags>(producerFlags & ~kFlagCountMask);
}

constexpr ProducerFlags getAndCheckProducerFlags(uint32_t producerFlags,
                                                 uint32_t consumerFlags) {
  auto value = getProducerFlags(producerFlags);
  if (value == ProducerFlags::kNone ||
      getFlagsCounter(producerFlags) == getFlagsCounter(consumerFlags)) {
    return internal::ProducerFlags::kNone;
  }
  return value;
}

/** @return The NotificationPolicy given the raw ConsumerPolicy value. */
NotificationPolicy notificationPolicy(uint32_t policyRaw) {
  return static_cast<NotificationPolicy>(
      reinterpret_cast<ConsumerPolicy *>(&policyRaw)->policy &
      static_cast<uint8_t>(NotificationPolicy::kMask));
}

/** @return The OverwritePolicy given the raw ConsumerPolicy value. */
OverwritePolicy overwritePolicy(uint32_t policyRaw) {
  return static_cast<OverwritePolicy>(
      reinterpret_cast<ConsumerPolicy *>(&policyRaw)->policy &
      static_cast<uint8_t>(OverwritePolicy::kMask));
}

/**
 * @return writeIndex - readIndex handling UINT32_MAX overflow.
 *
 * writeIndex is always at or ahead of readIndex.
 */
uint32_t writeReadDiff(uint32_t writeIndex, uint32_t readIndex) {
  return writeIndex >= readIndex ? writeIndex - readIndex
                                 : UINT32_MAX - readIndex + 1 + writeIndex;
}

}  // namespace

pw::Status ProducerBase::initialize(uintptr_t shmemBase, size_t shmemSize,
                                    Queue *queue, pw::Allocator &allocator,
                                    pw::allocator::Layout layout,
                                    size_t maxBlockCount, size_t minBlockCount,
                                    IdOrNotifyFn idOrNotifyFn) {
  if (!queue || shmemSize > UINT32_MAX || maxBlockCount < minBlockCount) {
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(auto *tailBlock,
                allocateBlockRing(shmemBase, shmemSize, allocator, layout,
                                  minBlockCount));
  auto &desc = tailBlock->producerDesc;
  desc.idOrNotifyFn = idOrNotifyFn;
  desc.writeIndex = 0;
  desc.indexCorrection = 0;
  desc.epoch = 0;
  desc.tailBlockOffset = toOffset(shmemBase, tailBlock);
  queue->producerOffset = toOffset(shmemBase, &desc);
  return pw::OkStatus();
}

ProducerBase::ProducerBase(uintptr_t shmemBase, uint32_t shmemSize,
                           Queue &queue, pw::Allocator &allocator,
                           pw::allocator::Layout blockLayout,
                           size_t /*maxBlockCount*/, size_t minBlockCount,
                           uint32_t dataOffset, DataNotifier &dataNotifier,
                           ConsumerManager &consumerManager,
                           RemoteNotifyFn remoteNotifyFn,
                           MemoryAccess *memAccess)
    : mRemoteNotifyFn(std::move(remoteNotifyFn)),
      kShmemBase(shmemBase),
      mQueue(&queue),
      mAllocator(&allocator),
      mDataNotifier(&dataNotifier),
      mConsumerManager(&consumerManager),
      mMemAccess(memAccess),
      kBlockLayout(blockLayout),
      kShmemSize(shmemSize),
      kDataOffset(dataOffset),
      kBlockCapacity(queue.blockCapacity),
      kLocal(queue.localNotify),
      mDesc(fromOffset<ProducerDesc>(kShmemBase, kShmemSize,
                                     queue.producerOffset)),
      mBlockCount(minBlockCount) {}

ProducerBase::~ProducerBase() {
  if (!mActive) {
    return;
  }
  mActive = false;
  mQueue->producerOffset = kOffsetInvalid;
  mConsumerManager->forAllConsumers(
      *mQueue, /*excludeMask=*/0,
      [this](internal::ConsumerDesc &desc, uint32_t producerFlags) {
        setConsumerFlag(desc, producerFlags, ProducerFlags::kReset,
                        /*forceNotify=*/true);
      });
  deallocateBlockRing(
      kShmemBase, kShmemSize, *mAllocator, kBlockLayout,
      fromOffset<BlockHeader>(kShmemBase, kShmemSize, mDesc->tailBlockOffset,
                              kBlockLayout));
}

pw::Status ProducerBase::setMaxBlockCountTarget(size_t /*count*/,
                                                bool /*force*/) {
  // TODO(b/448384247): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ProducerBase::setMinBlockCountTarget(size_t /*count*/) {
  // TODO(b/448384247): Implement.
  return pw::Status::Unimplemented();
}

pw::Result<pw::ByteSpan> ProducerBase::reserve(size_t /*count*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ProducerBase::commit(size_t /*count*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

pw::Result<size_t> ProducerBase::push(pw::ConstByteSpan /*data*/,
                                      bool /*allOrNothing*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

size_t ProducerBase::size(bool includeReserved) {
  if (!mActive) {
    return 0;
  }
  // Recalculate the available space to capture updates from consumers in shared
  // state.
  updateAvailable();
  auto size = capacity() - mAvailable;
  // If requested, exclude reserved elements from the size.
  return includeReserved ? size : size - mReserved;
}

void ProducerBase::updateAvailable(uint32_t increment) {
  auto tail = mDesc->writeIndex.load(std::memory_order_relaxed) + mReserved;
  mAvailable = capacity();  // Reset available counts.
  // Consumers that have been overwritten, are not yet initialized, or would
  // otherwise need to sync back to the producer position should not block
  // writes to the queue. Add them to the exclude mask.
  // NOTE: This effectively means all ProducerFlags states except kBlocking.
  auto excludeMask = ~(static_cast<uint16_t>(ProducerFlags::kBlocking));
  mConsumerManager->forAllConsumers(
      *mQueue, excludeMask,
      [this](internal::ConsumerDesc &desc, uint32_t producerFlags,
             uint32_t tail, uint32_t increment) {
        auto readIndex = desc.readIndex.load();
        auto diff = writeReadDiff(tail, readIndex);
        bool overwritable =
            overwritePolicy(desc.policy.load()) == OverwritePolicy::kAllowed;
        bool overwritten = false;
        if (overwritable && diff + increment > capacity()) {
          // If the consumer is behind by more than the current capacity or
          // would be if the increment is applied, mark it overwritten.
          // TODO(b/448384247): When the queue supports dynamic expansion, this
          // needs to be more conservative.
          setConsumerFlag(desc, producerFlags, ProducerFlags::kOverwrite);
          overwritten = true;
        } else if (!overwritable && diff + increment >= capacity()) {
          // If the queue is at capacity or would be if the increment is applied
          // and the consumer cannot be overwritten, indicate that the producer
          // is blocked.
          setConsumerFlag(desc, producerFlags, ProducerFlags::kBlocking);
        }
        // If this consumer is not overwritten, update the available space.
        if (!overwritten) {
          mAvailable = std::min(mAvailable, capacity() - diff);
        }
      },
      tail, increment);
}

void ProducerBase::setConsumerFlag(ConsumerDesc &desc, uint32_t current,
                                   ProducerFlags flag, bool forceNotify) {
  uint32_t flagCounter = getFlagsCounter(current) + kFlagCountInc;
  desc.producerFlags.store(static_cast<uint32_t>(flag) | flagCounter);
  if (forceNotify ||
      notificationPolicy(desc.policy.load()) != NotificationPolicy::kNever) {
    notifyConsumer(desc);
  }
}

void ProducerBase::notifyConsumer(ConsumerDesc &desc) {
  notify(desc.idOrNotifyFn, mRemoteNotifyFn, kLocal);
}

pw::Result<std::pair<Queue *, ConsumerDesc *>> ConsumerBase::checkArgs(
    uintptr_t shmemBase, uint32_t shmemSize, uint32_t queueOffset,
    uint32_t descOffset) {
  auto *queue = fromOffset<Queue>(shmemBase, shmemSize, queueOffset);
  auto *desc = fromOffset<ConsumerDesc>(shmemBase, shmemSize, descOffset);
  if (!queue || !desc) {
    return pw::Status::InvalidArgument();
  }
  return std::make_pair(queue, desc);
}

ConsumerBase::ConsumerBase(uintptr_t shmemBase, uint32_t shmemSize,
                           Queue &queue, ConsumerDesc &desc,
                           uint32_t dataOffset, RemoteNotifyFn remoteNotifyFn,
                           MemoryAccess *memAccess,
                           std::optional<size_t> overwriteResetOffset)
    : mRemoteNotifyFn(std::move(remoteNotifyFn)),
      kShmemBase(shmemBase),
      mQueue(&queue),
      mDesc(&desc),
      mMemAccess(memAccess),
      mOverwriteResetOffset(
          overwriteResetOffset.value_or(queue.blockCapacity / 2)),
      kShmemSize(shmemSize),
      kBlockCapacity(queue.blockCapacity),
      kDataOffset(dataOffset),
      kLocal(queue.localNotify) {}

pw::Status ConsumerBase::initialize(IdOrNotifyFn idOrNotifyFn,
                                    ConsumerPolicyBuilder &policyBuilder) {
  auto consumerFlags = mDesc->consumerFlags.load();
  auto producerFlags = mDesc->producerFlags.load();
  if (getAndCheckProducerFlags(producerFlags, consumerFlags) !=
      ProducerFlags::kPendingInit) {
    return pw::Status::FailedPrecondition();
  }
  mDesc->idOrNotifyFn = idOrNotifyFn;
  mDesc->policy.store(policyBuilder.build().rawValue);
  // Sync the consumer to the producer and store the current epoch.
  PW_TRY_ASSIGN(auto *producerDesc, getProducerDesc());
  mDesc->readIndex.store(producerDesc->writeIndex.load());
  mDesc->indexCorrection = producerDesc->indexCorrection;
  mEpoch = producerDesc->epoch.load();
  clearFlag(producerFlags);
  return pw::OkStatus();
}

ConsumerBase::~ConsumerBase() {
  if (!mStatus.ok()) {
    return;
  }
  mStatus = pw::Status::NotFound();
  mDesc->consumerFlags.store(static_cast<uint32_t>(ConsumerFlags::kFinished));
  if (auto maybeProducerDesc = getProducerDesc(); maybeProducerDesc.ok()) {
    notifyProducer(*maybeProducerDesc.value());
  }
}

pw::Status ConsumerBase::updatePolicy(
    ConsumerPolicyBuilder & /*policyBuilder*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

void ConsumerBase::disable() {
  // TODO(b/445967147): Implement.
}

pw::Status ConsumerBase::checkState() {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

pw::Result<pw::ConstByteSpan> ConsumerBase::peek(size_t /*count*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ConsumerBase::release(size_t /*count*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ConsumerBase::pop(pw::ByteSpan /*data*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ConsumerBase::resync(size_t /*offset*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

size_t ConsumerBase::size() {
  // TODO(b/445967147): Implement.
  return 0;
}

bool ConsumerBase::empty() {
  // TODO(b/445967147): Implement.
  return true;
}

pw::Result<ProducerDesc *> ConsumerBase::getProducerDesc() {
  auto *producerDesc = fromOffset<ProducerDesc>(kShmemBase, kShmemSize,
                                                mQueue->producerOffset.load());
  if (!producerDesc) {
    mStatus = pw::Status::Aborted();
    return mStatus;
  }
  return producerDesc;
}

void ConsumerBase::notifyProducer(ProducerDesc &producerDesc) {
  notify(producerDesc.idOrNotifyFn, mRemoteNotifyFn, kLocal);
}

void ConsumerBase::clearFlag(uint32_t flag) {
  mDesc->consumerFlags.store(
      static_cast<uint32_t>(ConsumerFlags::kFlagsCleared) |
      getFlagsCounter(flag));
}

}  // namespace internal

void DataNotifier::onWrite(internal::ProducerBase & /*producer*/) {
  // TODO(b/445482700): Implement.
}

void DataNotifier::updatePeriod(internal::ProducerBase & /*producer*/,
                                pw::span<const uint8_t, 16> /*consumerId*/,
                                std::optional<uint32_t> /*periodMs*/) {
  // TODO(b/445482700): Implement.
}

pw::Result<uint32_t> ConsumerManager::addConsumer(void *queue) {
  if (!queue) {
    return pw::Status::InvalidArgument();
  }
  auto &queueRef = *static_cast<internal::Queue *>(queue);
  auto descRaw =
      mAllocator->Allocate(pw::allocator::Layout::Of<internal::ConsumerDesc>());
  if (!descRaw) {
    return pw::Status::ResourceExhausted();
  }
  std::memset(descRaw, 0, sizeof(internal::ConsumerDesc));
  auto &desc = *static_cast<internal::ConsumerDesc *>(descRaw);
  desc.consumerFlags.store(
      static_cast<uint32_t>(internal::ConsumerFlags::kFlagsCleared));
  desc.producerFlags.store(
      static_cast<uint32_t>(internal::ProducerFlags::kPendingInit) |
      internal::kFlagCountInc);
  desc.nextConsumerOffset =
      queueRef.dynamicConsumersHeadOffset != internal::kOffsetInvalid
          ? queueRef.dynamicConsumersHeadOffset
          : internal::kOffsetInvalid;
  queueRef.dynamicConsumersHeadOffset = internal::toOffset(kShmemBase, &desc);
  return queueRef.dynamicConsumersHeadOffset;
}

pw::Status ConsumerManager::removeConsumer(void *queue, uint32_t offset) {
  if (!queue || offset == internal::kOffsetInvalid) {
    return pw::Status::InvalidArgument();
  }
  uint32_t *descOffsetPtr =
      &static_cast<internal::Queue *>(queue)->dynamicConsumersHeadOffset;
  auto *desc = internal::fromOffset<internal::ConsumerDesc>(
      kShmemBase, kShmemSize, *descOffsetPtr);
  while (desc) {
    if (*descOffsetPtr == offset) {
      *descOffsetPtr = desc->nextConsumerOffset;
      mAllocator->Deallocate(desc);
      return pw::OkStatus();
    }
    descOffsetPtr = &desc->nextConsumerOffset;
    desc = internal::fromOffset<internal::ConsumerDesc>(kShmemBase, kShmemSize,
                                                        *descOffsetPtr);
  }
  return pw::Status::NotFound();
}

bool ConsumerManager::isFlagInMask(internal::ConsumerDesc &desc,
                                   uint32_t producerFlags,
                                   uint32_t consumerFlags,
                                   uint16_t producerMask) {
  // Check whether the consumer has acked the latest flag value.
  if (internal::getAndCheckProducerFlags(producerFlags, consumerFlags) ==
      internal::ProducerFlags::kNone) {
    // If the flag hasn't been cleared, clear it now.
    if (internal::getProducerFlags(producerFlags) !=
        internal::ProducerFlags::kNone) {
      desc.producerFlags.store(
          internal::getFlagsCounter(producerFlags) |
          static_cast<uint32_t>(internal::ProducerFlags::kNone));
    }
    return false;
  }
  // Return true if the current flag value is in the mask.
  return !!(static_cast<uint16_t>(producerFlags) & producerMask);
}

}  // namespace chre::shmem_spmc_queue
