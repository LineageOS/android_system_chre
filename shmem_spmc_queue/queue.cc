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
#include "pw_assert/assert.h"

namespace chre::shmem_spmc_queue {
namespace internal {
namespace {

constexpr uint32_t kFlagCountInc = 0x10000;
constexpr uint32_t kFlagCountMask = 0xffff0000;

/**
 * Deallocates the block ring starting at the given head.
 *
 * @param shmemBase The base address of the shared memory.
 * @param shmemSize The size of the shared memory.
 * @param allocator The allocator used to allocate the block ring.
 * @param layout The layout of the block ring.
 * @param head The head of the block ring to deallocate. May be nullptr.
 */
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

/**
 * Allocate a block in shared memory.
 *
 * @param allocator The allocator used to allocate the block.
 * @param layout The layout of the block.
 * @param blockCapacity The capacity of the block. This cannot be inferred from
 * the layout as it is dependent on element size and alignment.
 * @return The allocated block on success, nullptr on failure.
 */
BlockHeader *allocateBlock(pw::Allocator &allocator,
                           pw::allocator::Layout layout,
                           uint32_t blockCapacity) {
  auto *block = static_cast<BlockHeader *>(allocator.Allocate(layout));
  if (block) {
    block->baseIndex.store(0);
    block->skipIndex.store(blockCapacity);
  }
  return block;
}

/**
 * Allocates a block ring in shared memory.
 *
 * @param shmemBase The base address of the shared memory.
 * @param shmemSize The size of the shared memory.
 * @param allocator The allocator used to allocate the block ring.
 * @param layout The layout of the block ring.
 * @param blockCapacity The capacity of the block ring. This cannot be inferred
 * from the layout as it is dependent on element size and alignment.
 * @param count The number of blocks to allocate.
 * @return Pointer to one block in the ring on success.
 */
pw::Result<BlockHeader *> allocateBlockRing(
    uintptr_t shmemBase, uint32_t shmemSize, pw::Allocator &allocator,
    pw::allocator::Layout layout, uint32_t blockCapacity, size_t count) {
  if (count == 0) {
    return pw::Status::InvalidArgument();
  }
  BlockHeader *head = nullptr, *prev = nullptr;
  for (int i = 0; i < count; ++i) {
    if (auto *block = allocateBlock(allocator, layout, blockCapacity); block) {
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

/**
 * Notifies an endpoint out-of-band.
 *
 * @param idOrNotifyFn Id for remote notification or local callback.
 * @param remoteNotifyFn Function for notifying Consumers out-of-band only for
 * remote queues.
 */
void notify(IdOrNotifyFn &idOrNotifyFn, const RemoteNotifyFn &remoteNotifyFn) {
  if (remoteNotifyFn) {
    remoteNotifyFn(pw::ConstByteSpan(idOrNotifyFn.remoteId));
  } else {
    idOrNotifyFn.localNotify.fn(idOrNotifyFn.localNotify.ctx);
  }
}

/** @return The counter value from the given flags. */
constexpr uint32_t getFlagsCounter(uint32_t flags) {
  return flags & kFlagCountMask;
}

/** @return The ProducerFlags value from the given flags. */
constexpr ProducerFlags getProducerFlags(uint32_t producerFlags) {
  return static_cast<ProducerFlags>(producerFlags & ~kFlagCountMask);
}

/**
 * Returns the current effective ProducerFlags state.
 *
 * @param producerFlags The raw producer flags value.
 * @param consumerFlags The raw consumer flags value. The counter is used to
 * determine if the consuemr has already cleared the ProducerFlags.
 * @return The effective ProducerFlags. This is kNone if the consumer has
 * already acked the flags.
 */
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
constexpr uint32_t writeReadDiff(uint32_t writeIndex, uint32_t readIndex) {
  return writeIndex >= readIndex ? writeIndex - readIndex
                                 : UINT32_MAX - readIndex + 1 + writeIndex;
}

/**
 * Calculates the difference between two ring buffer indices.
 *
 * @param end The ending index. May be less than the starting index.
 * @param begin The starting index.
 * @param size The size of the ring buffer.
 * @return The difference between the ending and starting index.
 */
constexpr uint32_t ringDiff(uint32_t end, uint32_t begin, uint32_t size) {
  return end > begin ? end - begin : size - begin + end;
}

/**
 * Initializes a ProducerDesc.
 *
 * @param desc The producer descriptor to initialize.
 * @param idOrNotifyFn Id for remote notification or local callback.
 * @param tailBlock The tail block.
 * @param shmemBase The base address of the shared memory.
 */
void initProducerDesc(ProducerDesc &desc, IdOrNotifyFn idOrNotifyFn,
                      uint32_t writeIndex, uint32_t correction, uint32_t epoch,
                      BlockHeader *tailBlock, uintptr_t shmemBase) {
  desc.idOrNotifyFn = idOrNotifyFn;
  desc.writeIndex.store(writeIndex);
  desc.indexCorrection = correction;
  desc.epoch.store(epoch);
  desc.tailBlockOffset = toOffset(shmemBase, tailBlock);
}

/**
 * @return The data pointer within the given block.
 *
 * @param block The block header.
 * @param dataOffset The offset of the data within the block.
 */
std::byte *blockData(BlockHeader *block, uint32_t dataOffset) {
  return reinterpret_cast<std::byte *>(reinterpret_cast<uintptr_t>(block) +
                                       dataOffset);
}

/**
 * Advances a read or write index within a block.
 *
 * The advancement done is over a single contiguous region of a block of size up
 * to count. This helps break up a larger chunk of data to be copied into
 * non-contiguous regions of one or more blocks.
 *
 * @param blockBaseIndex The base index of the block.
 * @param blockSkipIndex The skip index of the block.
 * @param blockCapacity The capacity of the block.
 * @param blockIndex [in/out] The index to advance.
 * @param count [in/out] The number of elements to advance. Stores the actual
 * advance.
 * @return true if the index should move on to the next block (note this may
 * actually be the same block).
 */
bool advanceContiguous(uint32_t blockBaseIndex, uint32_t blockSkipIndex,
                       uint32_t blockCapacity, uint32_t &blockIndex,
                       uint32_t &count) {
  PW_ASSERT(blockIndex < blockCapacity);
  PW_ASSERT(blockSkipIndex <= blockCapacity);
  PW_ASSERT(blockBaseIndex < blockCapacity);
  // Find the size of the next contiguous region (within count) by taking the
  // minimum with the distance to the end of the block, the distance to the skip
  // index (if set), and the distance to the base index of the block.
  auto diffToEnd = ringDiff(blockBaseIndex, blockIndex, blockCapacity);
  auto diffToSkip = ringDiff(blockSkipIndex, blockIndex, blockCapacity);
  auto diffToWrap = blockCapacity - blockIndex;
  count = std::min({count, diffToEnd, diffToSkip, diffToWrap});
  // Advance the index, wrapping around the block if necessary.
  PW_ASSERT(blockIndex + count <= blockCapacity);
  blockIndex = blockIndex + count == blockCapacity ? 0 : blockIndex + count;
  // The end of the block is reached if one of the following is true:
  // * The skip index is set and the index has reached it.
  // * The skip index is unset and the index has reached the block's base index.
  return (blockIndex == blockBaseIndex && blockSkipIndex == blockCapacity) ||
         blockIndex == blockSkipIndex;
}

/**
 * Combines block count and epoch into a single 32-bit value.
 *
 * @param blockCount The number of blocks in the queue.
 * @param epoch The block list epoch.
 */
uint32_t getBlockListEpoch(uint16_t blockCount, uint16_t epoch) {
  return (static_cast<uint32_t>(blockCount) << 16) |
         static_cast<uint32_t>(epoch);
}

/**
 * @return the block count for a producer epoch.
 * @param epoch The producer epoch.
 */
uint32_t blockCountForEpoch(uint32_t epoch) {
  return epoch >> 16;
}

/**
 * Calculates the increment to the index correction when entering a block.
 *
 * @param curr The current block.
 * @param next The next block.
 * @param capacity The capacity of a block.
 * @return The increase in the correction when entering the next block.
 */
uint32_t indexCorrectionIncrement(BlockHeader *curr, BlockHeader *next,
                                  uint32_t capacity) {
  auto baseIndex = curr->baseIndex.load();
  auto skipIndex = curr->skipIndex.load();
  uint32_t diffBase = skipIndex == capacity ? baseIndex : skipIndex;
  return ringDiff(next->baseIndex.load(), diffBase, capacity);
}

}  // namespace

pw::Status ProducerBase::initialize(uintptr_t shmemBase, uint32_t shmemSize,
                                    Queue *queue, pw::Allocator &allocator,
                                    pw::allocator::Layout layout,
                                    uint32_t blockCapacity,
                                    size_t maxBlockCount, size_t minBlockCount,
                                    IdOrNotifyFn idOrNotifyFn) {
  if (!queue || shmemSize > UINT32_MAX || maxBlockCount < minBlockCount) {
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(auto *tailBlock,
                allocateBlockRing(shmemBase, shmemSize, allocator, layout,
                                  blockCapacity, minBlockCount));
  auto &desc = tailBlock->producerDesc;
  initProducerDesc(desc, idOrNotifyFn, /*writeIndex=*/0, /*correction=*/0,
                   getBlockListEpoch(minBlockCount, /*epoch=*/0), tailBlock,
                   shmemBase);
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
      mDesc(fromOffset<ProducerDesc>(kShmemBase, kShmemSize,
                                     queue.producerOffset)),
      mCurrBlock(fromOffset<BlockHeader>(kShmemBase, kShmemSize,
                                         mDesc->tailBlockOffset, kBlockLayout)),
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

pw::Result<pw::ByteSpan> ProducerBase::reserve(size_t count) {
  PW_TRY_ASSIGN(uint32_t size, checkAvailable(count, /*allOrNothing=*/true));
  // Return a span over the next available contiguous region.
  auto *begin = blockData(mCurrBlock, kDataOffset) + mCurrBlockIndex;
  if (advanceContiguous(mCurrBlock->baseIndex, mCurrBlock->skipIndex,
                        kBlockCapacity, mCurrBlockIndex, size)) {
    uint32_t correction = 0;  // unused
    enterNextBlock(mCurrBlock, correction, mCurrBlockIndex,
                   /*convertSkipToBase=*/true);
  }
  mReserved += size;
  return pw::ByteSpan(begin, size);
}

pw::Status ProducerBase::commit(size_t count) {
  if (count > mReserved) {
    return pw::Status::OutOfRange();
  }
  mReserved -= count;
  advanceWriteIndex(count, /*data=*/std::nullopt);
  return pw::OkStatus();
}

pw::Result<size_t> ProducerBase::push(pw::ConstByteSpan data,
                                      bool allOrNothing) {
  if (mReserved > 0) {  // push() is not allowed while a reservation is active.
    return pw::Status::FailedPrecondition();
  }
  PW_TRY_ASSIGN(auto count, checkAvailable(data.size(), allOrNothing));
  advanceWriteIndex(count, data);
  return count;
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

pw::Result<size_t> ProducerBase::checkAvailable(size_t count,
                                                bool allOrNothing) {
  if (!mActive) {
    return pw::Status::NotFound();
  }
  // TODO(b/448384247): This should check against the maximum capacity.
  if (count > capacity()) {
    return pw::Status::OutOfRange();
  }
  if (count > mAvailable) {
    // Update the available space. If allOrNothing, update consumer flags taking
    // into account the size of the data.
    updateAvailable(allOrNothing ? count : 0);
    // TODO(b/448384247): Support dynamically resizing the queue.
    if (count > mAvailable) {
      if (allOrNothing) {
        return pw::Status::Unavailable();
      }
      count = mAvailable;
    }
  }
  mAvailable -= count;
  return count;
}

void ProducerBase::advanceWriteIndex(uint32_t count,
                                     std::optional<pw::ConstByteSpan> data) {
  uint32_t writeIndex = mDesc->writeIndex.load();
  uint32_t correction = mDesc->indexCorrection;
  auto *block = fromOffset<BlockHeader>(kShmemBase, kShmemSize,
                                        mDesc->tailBlockOffset, kBlockLayout);
  uint32_t blockIndex = (writeIndex + correction) % kBlockCapacity;
  auto pending = count;
  while (pending > 0) {
    uint32_t advance = pending;
    // Advance through the largest possible contiguous region from the current
    // index.
    auto *copyDst = blockData(block, kDataOffset) + blockIndex;
    bool toNextBlock =
        advanceContiguous(block->baseIndex.load(), block->skipIndex.load(),
                          kBlockCapacity, blockIndex, advance);
    if (data) {
      std::memcpy(copyDst, data->data(), advance);
      data = data->subspan(advance);
    }
    pending -= advance;
    if (toNextBlock) {
      // Only convert skip to base during a push(). If commit(), then the
      // conversion would already have occurred on reserve().
      enterNextBlock(block, correction, blockIndex,
                     /*convertSkipToBase=*/data.has_value());
    }
  }
  // Update the write index in queue metadata.
  updateWriteIndex(block, writeIndex + count, correction);
}

void ProducerBase::enterNextBlock(BlockHeader *&block, uint32_t &correction,
                                  uint32_t &index, bool convertSkipToBase) {
  auto *nextBlock = fromOffset<BlockHeader>(
      kShmemBase, kShmemSize, block->nextBlockOffset.load(), kBlockLayout);
  // If the next block was skipped from on the last visit, set its base
  // index to that skip index and reset the skip index.
  auto nextSkipIndex = nextBlock->skipIndex.load();
  if (convertSkipToBase && nextSkipIndex != kBlockCapacity) {
    nextBlock->baseIndex.store(nextSkipIndex);
    nextBlock->skipIndex.store(kBlockCapacity);
  }
  // Update the index correction to be applied to the write index.
  correction += indexCorrectionIncrement(block, nextBlock, kBlockCapacity);
  block = nextBlock;
  index = block->baseIndex;
}

void ProducerBase::updateWriteIndex(BlockHeader *tailBlock, uint32_t writeIndex,
                                    uint32_t correction) {
  if (tailBlock == fromOffset<BlockHeader>(kShmemBase, kShmemSize,
                                           mDesc->tailBlockOffset,
                                           kBlockLayout)) {
    // If the currently linked tail block is still the tail, just store the new
    // write index.
    mDesc->writeIndex.store(writeIndex);
  } else {
    // Initialize the descriptor in the new tail block, then link it.
    auto &newDesc = tailBlock->producerDesc;
    initProducerDesc(newDesc, mDesc->idOrNotifyFn, writeIndex, correction,
                     mDesc->epoch.load(), tailBlock, kShmemBase);
    mQueue->producerOffset.store(toOffset(kShmemBase, &newDesc));
    mDesc = &newDesc;
  }
}

void ProducerBase::updateAvailable(uint32_t increment) {
  auto tail = mDesc->writeIndex.load() + mReserved;
  mAvailable = capacity() - mReserved;  // Reset available counts.
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
  notify(desc.idOrNotifyFn, mRemoteNotifyFn);
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
                           pw::allocator::Layout baseBlockLayout,
                           uint32_t dataOffset, RemoteNotifyFn remoteNotifyFn,
                           MemoryAccess *memAccess,
                           std::optional<size_t> overwriteResetOffset)
    : mRemoteNotifyFn(std::move(remoteNotifyFn)),
      kBlockLayout{baseBlockLayout.size() + queue.blockCapacity,
                   baseBlockLayout.alignment()},
      kShmemBase(shmemBase),
      mQueue(&queue),
      mDesc(&desc),
      mMemAccess(memAccess),
      mOverwriteResetOffset(
          overwriteResetOffset.value_or(queue.blockCapacity / 2)),
      kShmemSize(shmemSize),
      kBlockCapacity(queue.blockCapacity),
      kDataOffset(dataOffset) {}

pw::Status ConsumerBase::initialize(IdOrNotifyFn idOrNotifyFn,
                                    ConsumerPolicyBuilder &policyBuilder) {
  auto consumerFlags = mDesc->consumerFlags.load();
  mCurrentFlags = mDesc->producerFlags.load();
  if (getAndCheckProducerFlags(mCurrentFlags, consumerFlags) !=
      ProducerFlags::kPendingInit) {
    return pw::Status::FailedPrecondition();
  }
  mDesc->idOrNotifyFn = idOrNotifyFn;
  mDesc->policy.store(policyBuilder.build().rawValue);
  PW_TRY(syncToProducer());
  clearFlags();
  return pw::OkStatus();
}

ConsumerBase::~ConsumerBase() {
  if (!mActive) {
    return;
  }
  mDesc->consumerFlags.store(static_cast<uint32_t>(ConsumerFlags::kFinished));
  if (auto maybeProducerDesc = getProducerDesc(); maybeProducerDesc.ok()) {
    notifyProducer(*maybeProducerDesc.value());
  }
}

pw::Status ConsumerBase::updatePolicy(ConsumerPolicyBuilder &policyBuilder) {
  PW_TRY(checkState());
  mDesc->policy.store(policyBuilder.build().rawValue);
  return pw::OkStatus();
}

void ConsumerBase::disable() {
  mActive = false;
}

pw::Status ConsumerBase::checkState() {
  if (!mActive) {
    return pw::Status::NotFound();
  }
  mCurrentFlags = mDesc->producerFlags.load();
  auto consumerFlags = mDesc->consumerFlags.load();
  auto flagValue = getAndCheckProducerFlags(mCurrentFlags, consumerFlags);
  switch (flagValue) {
    case ProducerFlags::kPendingInit:
      // This should not happen and may indicate that this instance has outlived
      // the producer that it was registered with. Handle it accordingly.
      [[fallthrough]];
    case ProducerFlags::kReset:
      mActive = false;
      return pw::Status::Aborted();
    case ProducerFlags::kOverwrite: {
      PW_TRY_ASSIGN(auto *producerDesc, getProducerDesc());
      // TODO(b/445967147): Only sync to the producer if the epoch has changed.
      // Otherwise try to catch up until mOverwriteResetOffset.
      mEpoch = producerDesc->epoch.load();
      mDesc->readIndex.store(producerDesc->writeIndex.load());
      mDesc->indexCorrection = producerDesc->indexCorrection;
      mAvailable = 0;
      mPeeked = 0;
      clearFlags();
      return pw::Status::DataLoss();
    }
    case ProducerFlags::kBlocking:
      // This state is used to trigger a notification to the producer on a read.
      // Since we can't force the user to read, it isn't worth surfacing to the
      // user.
      [[fallthrough]];
    case ProducerFlags::kNone:
      return pw::OkStatus();
    default:  // Unexpected flag value. Clear it.
      clearFlags();
      return pw::OkStatus();
  }
}

pw::Result<pw::ConstByteSpan> ConsumerBase::peek(size_t count) {
  PW_TRY(checkAvailable(count));
  mPeeked += count;
  const auto *data = blockData(mCurrBlock, kDataOffset) + mCurrBlockIndex;
  uint32_t advance = count;
  if (advanceContiguous(mCurrBlock->baseIndex.load(),
                        mCurrBlock->skipIndex.load(), kBlockCapacity,
                        mCurrBlockIndex, advance)) {
    mCurrBlock = fromOffset<BlockHeader>(kShmemBase, kShmemSize,
                                         mCurrBlock->nextBlockOffset.load(),
                                         kBlockLayout);
    mCurrBlockIndex = mCurrBlock->baseIndex.load();
  }
  PW_TRY(checkState());
  return pw::ConstByteSpan(data, count);
}

pw::Status ConsumerBase::release(size_t count) {
  PW_TRY(checkState());
  // It is valid to peek more than the available count. If so, clear mPeeked and
  // update mAvailable.
  if (count > mPeeked) {
    if (count > mAvailable + mPeeked) {
      PW_TRY(updateAvailable());
      // If count would exceed the queue size, just sync to the producer.
      if (count > mAvailable + mPeeked) {
        return syncToProducer();
      }
    }
    mAvailable -= count - mPeeked;
    mPeeked = 0;
  } else {
    mPeeked -= count;
  }
  advanceReadIndex(count, /*buf=*/std::nullopt);
  return pw::OkStatus();
}

pw::Status ConsumerBase::pop(pw::ByteSpan data) {
  if (mPeeked) {  // pop() is not allowed when there is un-release()d data.
    return pw::Status::FailedPrecondition();
  }
  PW_TRY(checkAvailable(data.size()));
  advanceReadIndex(data.size(), data);
  return pw::OkStatus();
}

pw::Status ConsumerBase::resync(size_t /*offset*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

pw::Result<size_t> ConsumerBase::size() {
  PW_TRY(checkState());
  PW_TRY(updateAvailable());
  return mAvailable;
}

pw::Status ConsumerBase::checkAvailable(size_t count) {
  PW_TRY(checkState());
  if (count > mQueue->blockCapacity * blockCountForEpoch(mEpoch)) {
    // If the epoch has changed, check against the updated capacity.
    PW_TRY_ASSIGN(auto *producerDesc, getProducerDesc());
    mEpoch = producerDesc->epoch.load();
    if (count > mQueue->blockCapacity * blockCountForEpoch(mEpoch)) {
      return pw::Status::OutOfRange();
    }
  }
  if (count > mAvailable) {
    // Check if the producer has pushed new data.
    PW_TRY(updateAvailable());
    if (count > mAvailable) {
      return pw::Status::Unavailable();
    }
  }
  mAvailable -= count;
  return pw::OkStatus();
}

void ConsumerBase::advanceReadIndex(size_t count,
                                    std::optional<pw::ByteSpan> buf) {
  auto pending = count;
  auto readIndex = mDesc->readIndex.load();
  uint32_t blockIndex = (readIndex + mDesc->indexCorrection) % kBlockCapacity;
  auto correction = mDesc->indexCorrection;
  // Loop through the contiguous regions, copying out data and tracking index
  // corrections as the read index moves between blocks.
  while (pending > 0) {
    uint32_t advance = pending;
    const auto *dataPtr = blockData(mHeadBlock, kDataOffset) + blockIndex;
    bool toNextBlock = advanceContiguous(mHeadBlock->baseIndex.load(),
                                         mHeadBlock->skipIndex.load(),
                                         kBlockCapacity, blockIndex, advance);
    if (buf) {
      std::memcpy(buf->data(), dataPtr, advance);
      buf = buf->subspan(advance);
    }
    pending -= advance;
    if (toNextBlock) {
      mHeadBlock = fromOffset<BlockHeader>(kShmemBase, kShmemSize,
                                           mHeadBlock->nextBlockOffset.load(),
                                           kBlockLayout);
      blockIndex = mHeadBlock->baseIndex.load();
      correction +=
          indexCorrectionIncrement(mHeadBlock, mHeadBlock, kBlockCapacity);
    }
  }
  mDesc->readIndex.store(readIndex + count);
  mDesc->indexCorrection = correction;
  if (getProducerFlags(mCurrentFlags) == ProducerFlags::kBlocking) {
    if (auto producerDesc = getProducerDesc(); producerDesc.ok()) {
      notifyProducer(*producerDesc.value());
    }
    clearFlags();
  }
}

pw::Status ConsumerBase::updateAvailable() {
  PW_TRY_ASSIGN(auto *producerDesc, getProducerDesc());
  mAvailable =
      writeReadDiff(producerDesc->writeIndex.load(), mDesc->readIndex.load());
  return pw::OkStatus();
}

pw::Status ConsumerBase::syncToProducer() {
  PW_TRY_ASSIGN(auto *producerDesc, getProducerDesc());
  auto readIndex = producerDesc->writeIndex.load();
  mDesc->readIndex.store(readIndex);
  mDesc->indexCorrection = producerDesc->indexCorrection;
  mHeadBlock = fromOffset<BlockHeader>(
      kShmemBase, kShmemSize, producerDesc->tailBlockOffset, kBlockLayout);
  mCurrBlock = mHeadBlock;
  mCurrBlockIndex = (readIndex + mDesc->indexCorrection) % kBlockCapacity;
  mEpoch = producerDesc->epoch.load();
  return pw::OkStatus();
}

pw::Result<ProducerDesc *> ConsumerBase::getProducerDesc() {
  auto *producerDesc = fromOffset<ProducerDesc>(kShmemBase, kShmemSize,
                                                mQueue->producerOffset.load());
  if (!producerDesc) {
    mActive = false;
    return pw::Status::Aborted();
  }
  return producerDesc;
}

void ConsumerBase::notifyProducer(ProducerDesc &producerDesc) {
  notify(producerDesc.idOrNotifyFn, mRemoteNotifyFn);
}

void ConsumerBase::clearFlags() {
  auto counter = getFlagsCounter(mCurrentFlags);
  mDesc->consumerFlags.store(
      static_cast<uint32_t>(ConsumerFlags::kFlagsCleared) | counter);
  mCurrentFlags = static_cast<uint32_t>(ProducerFlags::kNone) | counter;
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
