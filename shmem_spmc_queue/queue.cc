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
                           size_t maxBlockCount, size_t minBlockCount,
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
      mMaxBlockCount(maxBlockCount),
      mMinBlockCount(minBlockCount) {}

ProducerBase::~ProducerBase() {
  if (!mActive) {
    return;
  }
  mActive = false;
  mQueue->producerOffset = kOffsetInvalid;
  // TODO(b/445482700): Notify Consumers.
  deallocateBlockRing(
      kShmemBase, kShmemSize, *mAllocator, kBlockLayout,
      fromOffset<BlockHeader>(kShmemBase, kShmemSize, mDesc->tailBlockOffset,
                              kBlockLayout));
}

pw::Status ProducerBase::setMaxBlockCountTarget(size_t /*count*/,
                                                bool /*force*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

size_t ProducerBase::getMaxBlockCountTarget() const {
  // TODO(b/445482700): Implement.
  return 0;
}

pw::Status ProducerBase::setMinBlockCountTarget(size_t /*count*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

size_t ProducerBase::getMinBlockCountTarget() const {
  // TODO(b/445482700): Implement.
  return 0;
}

size_t ProducerBase::getBlockCount() const {
  // TODO(b/445482700): Implement.
  return 0;
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

bool ProducerBase::full() const {
  // TODO(b/445482700): Implement.
  return true;
}

size_t ProducerBase::size(bool /*includeReserved*/,
                          bool /*includeOverwritable*/) const {
  // TODO(b/445482700): Implement.
  return 0;
}

size_t ProducerBase::capacity() const {
  // TODO(b/445482700): Implement.
  return 0;
}

pw::Status ConsumerBase::initialize(uintptr_t /*shmemBase*/,
                                    uint32_t /*shmemSize*/, Queue * /*queue*/,
                                    ConsumerDesc * /*desc*/,
                                    IdOrNotifyFn /*idOrNotifyFn*/,
                                    ConsumerPolicyBuilder & /*policyBuilder*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

ConsumerBase::ConsumerBase(uintptr_t /*shmemBase*/, uint32_t /*shmemSize*/,
                           Queue & /*queue*/, ConsumerDesc & /*desc*/,
                           uint32_t /*dataOffset*/,
                           RemoteNotifyFn /*remoteNotifyFn*/,
                           MemoryAccess * /*memAccess*/,
                           std::optional<size_t> /*overwriteResetOffset*/) {
  // TODO(b/445967147): Implement.
}

ConsumerBase::~ConsumerBase() {
  // TODO(b/445967147): Implement.
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
  auto &desc = *static_cast<internal::ConsumerDesc *>(descRaw);
  desc.nextConsumerOffset =
      queueRef.dynamicConsumersHeadOffset != internal::kOffsetInvalid
          ? queueRef.dynamicConsumersHeadOffset
          : internal::kOffsetInvalid;
  queueRef.dynamicConsumersHeadOffset = toOffset(kShmemBase, &desc);
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

}  // namespace chre::shmem_spmc_queue
