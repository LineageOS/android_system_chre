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

#include "data_flow/queue.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "data_flow/internal/queue_internal.h"
#include "data_flow/queue_defs.h"
#include "data_flow/untyped_queue.h"
#include "pw_allocator/layout.h"
#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {

using DataConfigMode = internal::Queue::DataConfig::Mode;

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
void deallocateBlockRing(const AllocatorRegion &region,
                         pw::allocator::Layout layout, BlockHeader *head) {
  if (!head) {
    return;
  }
  for (BlockHeader *block = head; block;) {
    auto *tmp = block;
    block = fromOffset<BlockHeader>(region, block->nextBlockOffset, layout);
    region.allocator->Deallocate(tmp);
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
 * @param variableData True iff the queue holds variable-data elements.
 * @return The allocated block on success, nullptr on failure.
 */
BlockHeader *allocateBlock(pw::Allocator &allocator,
                           pw::allocator::Layout layout, uint32_t blockCapacity,
                           bool variableData) {
  auto *block = static_cast<BlockHeader *>(allocator.Allocate(layout));
  if (block) {
    block->baseIndex.store(0);
    block->skipIndex.store(blockCapacity);
  }
  if (variableData) {
    auto *variableDataBlock = reinterpret_cast<VariableDataBlock *>(block);
    variableDataBlock->header.firstElementIndex = blockCapacity;
  }
  return block;
}

/**
 * Allocates a block ring in shared memory.
 *
 * @param shmemBase The shared memory region in which to allocate the blocks.
 * @param layout The layout of the block ring.
 * @param blockCapacity The capacity of the block ring. This cannot be inferred
 * from the layout as it is dependent on element size and alignment.
 * @param count The number of blocks to allocate.
 * @param variableData True iff the queue holds variable-data elements.
 * @return Pointer to one block in the ring on success.
 */
pw::Result<BlockHeader *> allocateBlockRing(const AllocatorRegion &region,
                                            pw::allocator::Layout layout,
                                            uint32_t blockCapacity,
                                            size_t count, bool variableData) {
  if (count == 0) {
    return pw::Status::InvalidArgument();
  }
  BlockHeader *head = nullptr, *prev = nullptr;
  for (int i = 0; i < count; ++i) {
    if (auto *block = allocateBlock(*region.allocator, layout, blockCapacity,
                                    variableData);
        block) {
      if (!head) {
        head = block;
      } else {
        prev->nextBlockOffset = toOffset(region.base, block);
      }
      prev = block;
    } else {
      deallocateBlockRing(region, layout, head);
      return pw::Status::ResourceExhausted();
    }
  }
  prev->nextBlockOffset = internal::toOffset(region.base, head);
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
 * @param writeIndex The current write index.
 * @param correction The correction to the write index for calculating the index
 * within Block::data.
 * @param tailBlock The tail block.
 * @param shmemBase The base address of the shared memory.
 */
void initProducerDesc(ProducerDesc &desc, uint32_t writeIndex,
                      uint32_t correction, BlockHeader *tailBlock,
                      uintptr_t shmemBase) {
  desc.writeIndex.store(writeIndex);
  desc.indexCorrection = correction;
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

/**
 * @return the value aligned to the given alignment.
 * @param value The value to align.
 * @param alignment The alignment to use.
 */
constexpr size_t alignTo(size_t value, size_t alignment) {
  const auto kUnalignedBits = alignment - 1;
  return value & kUnalignedBits ? value + alignment - (value & kUnalignedBits)
                                : value;
}

/**
 * @return the offset of data within a block for a fixed-size element queue.
 * @param queue The queue metadata.
 */
size_t getDataOffset(internal::Queue &queue) {
  // Calculate the offset of data within the block based on the element
  // alignment. This must be equivalent to the offset of Block<T>.data for some
  // T of the same size and alignment.
  size_t elementAlignment = queue.config.fixedSize.elementAlignment;
  return internal::alignTo(sizeof(internal::BlockHeader), elementAlignment);
}

/**
 * @return the block layout for the given fixed-size element queue.
 * @param queue The queue metadata.
 */
pw::allocator::Layout getBlockLayout(internal::Queue &queue) {
  size_t elementAlignment = queue.config.fixedSize.elementAlignment;
  size_t blockAlignment =
      std::max(alignof(internal::BlockHeader), elementAlignment);
  return pw::allocator::Layout(getDataOffset(queue) + queue.blockCapacity,
                               blockAlignment);
}

}  // namespace

pw::Status ProducerBase::initialize(const AllocatorRegion &region,
                                    QueuePrivate *queue,
                                    pw::allocator::Layout layout,
                                    size_t maxBlockCount, size_t minBlockCount,
                                    IdOrNotifyFn idOrNotifyFn) {
  if (region.size > UINT32_MAX || !region.allocator ||
      maxBlockCount < minBlockCount) {
    return pw::Status::InvalidArgument();
  }
  bool variableData = queue->config.mode != DataConfigMode::kFixedSize;
  PW_TRY_ASSIGN(auto *tailBlock,
                allocateBlockRing(region, layout, queue->blockCapacity,
                                  minBlockCount, variableData));
  auto &desc = tailBlock->producerDesc;
  initProducerDesc(desc, /*writeIndex=*/0, /*correction=*/0, tailBlock,
                   region.base);
  queue->idOrNotifyFn = idOrNotifyFn;
  queue->blockListEpoch.store(getBlockListEpoch(minBlockCount, /*epoch=*/0));
  queue->producerOffset = toOffset(region.base, &desc);
  return pw::OkStatus();
}

ProducerBase::ProducerBase(const AllocatorRegion &region, QueuePrivate &queue,
                           pw::allocator::Layout blockLayout,
                           uint32_t dataOffset, size_t /*maxBlockCount*/,
                           size_t minBlockCount, DataNotifier &dataNotifier,
                           RemoteNotifyFn remoteNotifyFn,
                           MemoryAccess *memAccess)
    : mRegion(region),
      mRemoteNotifyFn(std::move(remoteNotifyFn)),
      mQueue(&queue),
      mDataNotifier(&dataNotifier),
      mMemAccess(memAccess),
      kBlockLayout(blockLayout),
      kDataOffset(dataOffset),
      kBlockCapacity(queue.blockCapacity),
      mDesc(fromOffset<ProducerDesc>(mRegion, queue.producerOffset)),
      mCurrBlock(fromOffset<BlockHeader>(mRegion, mDesc->tailBlockOffset,
                                         kBlockLayout)),
      mBlockCount(minBlockCount) {}

ProducerBase::~ProducerBase() {
  if (mState == State::kMovedFrom) {
    return;
  }
  if (mState == State::kActive) {
    stop();
  }
  // Deallocates all consumer descriptors. Consumers will have been notified in
  // stop() that the producer is torn down. The user may wait for the consumers
  // to signal that they have torn down before destroying the producer.
  // Otherwise, this does any remaining cleanup. Note that the memory remains on
  // the consumer side.
  for (auto node = mQueue->consumerList.begin();
       node != mQueue->consumerList.end();) {
    eraseConsumerNode(node);
  }
  // Release element storage back to the region allocator.
  deallocateBlockRing(
      mRegion, kBlockLayout,
      fromOffset<BlockHeader>(mRegion, mDesc->tailBlockOffset, kBlockLayout));
}

void ProducerBase::stop() {
  if (mState != State::kActive) {
    return;
  }
  mState = State::kStopped;
  mQueue->producerOffset.store(kOffsetInvalid);
  // Mark the producer as torn down and notify all consumers.
  forAllConsumers(
      /*excludeMask=*/0,
      [this](internal::ConsumerNode &node, uint32_t producerFlags) {
        setConsumerFlag(node, producerFlags, ProducerFlags::kFinished,
                        /*forceNotify=*/true);
      });
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
    enterNextBlock(mCurrBlock, /*correction=*/nullptr, mCurrBlockIndex,
                   /*convertSkipToBase=*/true);
  }
  mReserved += size;
  return pw::ByteSpan(begin, size);
}

pw::Status ProducerBase::truncate(size_t size) {
  // Check and update the size of the reservation.
  if (mReserved == 0) {
    return pw::Status::FailedPrecondition();
  } else if (size > mReserved) {
    return pw::Status::OutOfRange();
  } else if (size == mReserved) {
    return pw::OkStatus();
  }
  mReserved = size;
  // Sync the current block and index back to the write index.
  mCurrBlock =
      fromOffset<BlockHeader>(mRegion, mDesc->tailBlockOffset, kBlockLayout);
  mCurrBlockIndex =
      (mDesc->writeIndex.load() + mDesc->indexCorrection) % kBlockCapacity;
  // Advance to the new reservation size.
  advanceBlockIndexWithData(mCurrBlock, mCurrBlockIndex, /*correction=*/nullptr,
                            size, /*data=*/std::nullopt,
                            /*convertSkipToBase=*/false);
  return pw::OkStatus();
}

pw::Status ProducerBase::commit(size_t count) {
  if (mState != State::kActive) {
    return pw::Status::FailedPrecondition();
  }
  if (count > mReserved) {
    return pw::Status::OutOfRange();
  }
  mReserved -= count;
  advanceWriteIndex(count, /*data=*/std::nullopt);
  mDataNotifier->onWrite(*this);
  return pw::OkStatus();
}

pw::Result<size_t> ProducerBase::push(pw::ConstByteSpan data,
                                      bool allOrNothing) {
  if (mState != State::kActive) {
    return pw::Status::FailedPrecondition();
  }
  if (mReserved > 0) {  // push() is not allowed while a reservation is active.
    return pw::Status::FailedPrecondition();
  }
  PW_TRY_ASSIGN(auto count, checkAvailable(data.size(), allOrNothing));
  advanceWriteIndex(count, data);
  mDataNotifier->onWrite(*this);
  return count;
}

size_t ProducerBase::size(bool includeReserved) {
  if (mState != State::kActive) {
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
  if (mState != State::kActive) {
    return pw::Status::FailedPrecondition();
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
  auto *block =
      fromOffset<BlockHeader>(mRegion, mDesc->tailBlockOffset, kBlockLayout);
  uint32_t blockIndex = (writeIndex + correction) % kBlockCapacity;
  advanceBlockIndexWithData(block, blockIndex, &correction, count, data,
                            /*convertSkipToBase=*/data.has_value());
  // Update the write index in queue metadata.
  updateWriteIndex(block, writeIndex + count, correction);
}

void ProducerBase::advanceBlockIndexWithData(
    BlockHeader *&block, uint32_t &index, uint32_t *correction, uint32_t count,
    std::optional<pw::ConstByteSpan> data, bool convertSkipToBase) {
  auto pending = count;
  while (pending > 0) {
    uint32_t advance = pending;
    // Advance through the largest possible contiguous region from the current
    // index.
    auto *copyDst = blockData(block, kDataOffset) + index;
    bool toNextBlock =
        advanceContiguous(block->baseIndex.load(), block->skipIndex.load(),
                          kBlockCapacity, index, advance);
    if (data) {
      std::memcpy(copyDst, data->data(), advance);
      data = data->subspan(advance);
    }
    pending -= advance;
    if (toNextBlock) {
      // Only convert skip to base during a push(). If commit(), then the
      // conversion would already have occurred on reserve().
      enterNextBlock(block, correction, index, convertSkipToBase);
    }
  }
}

void ProducerBase::enterNextBlock(BlockHeader *&block, uint32_t *correction,
                                  uint32_t &index, bool convertSkipToBase) {
  auto *nextBlock = fromOffset<BlockHeader>(
      mRegion, block->nextBlockOffset.load(), kBlockLayout);
  // If the next block was skipped from on the last visit, set its base
  // index to that skip index and reset the skip index.
  auto nextSkipIndex = nextBlock->skipIndex.load();
  if (convertSkipToBase && nextSkipIndex != kBlockCapacity) {
    nextBlock->baseIndex.store(nextSkipIndex);
    nextBlock->skipIndex.store(kBlockCapacity);
  }
  if (correction) {
    // Update the index correction to be applied to the write index.
    correction += indexCorrectionIncrement(block, nextBlock, kBlockCapacity);
  }
  block = nextBlock;
  index = block->baseIndex;
}

void ProducerBase::updateWriteIndex(BlockHeader *tailBlock, uint32_t writeIndex,
                                    uint32_t correction) {
  if (tailBlock ==
      fromOffset<BlockHeader>(mRegion, mDesc->tailBlockOffset, kBlockLayout)) {
    // If the currently linked tail block is still the tail, just store the
    // new write index.
    mDesc->writeIndex.store(writeIndex);
  } else {
    // Initialize the descriptor in the new tail block, then link it.
    auto &newDesc = tailBlock->producerDesc;
    initProducerDesc(newDesc, writeIndex, correction, tailBlock, mRegion.base);
    mQueue->producerOffset.store(toOffset(mRegion.base, &newDesc));
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
  forAllConsumers(
      excludeMask,
      [this](internal::ConsumerNode &node, uint32_t producerFlags,
             uint32_t tail, uint32_t increment) {
        auto readIndex = node.desc->readIndex.load();
        auto diff = writeReadDiff(tail, readIndex);
        bool overwritable = node.policy.overwrite == OverwritePolicy::kAllowed;
        bool overwritten = false;
        if (overwritable && diff + increment > capacity()) {
          // If the consumer is behind by more than the current capacity or
          // would be if the increment is applied, mark it overwritten.
          // TODO(b/448384247): When the queue supports dynamic expansion,
          // this needs to be more conservative.
          setConsumerFlag(node, producerFlags, ProducerFlags::kOverwrite);
          overwritten = true;
        } else if (!overwritable && diff + increment >= capacity()) {
          // If the queue is at capacity or would be if the increment is
          // applied and the consumer cannot be overwritten, indicate that the
          // producer is blocked.
          setConsumerFlag(node, producerFlags, ProducerFlags::kBlocking);
        }
        // If this consumer is not overwritten, update the available space.
        if (!overwritten) {
          mAvailable = std::min(mAvailable, capacity() - diff);
        }
      },
      tail, increment);
}

void ProducerBase::setConsumerFlag(ConsumerNode &node, uint32_t current,
                                   ProducerFlags flag, bool forceNotify) {
  uint32_t flagCounter = getFlagsCounter(current) + kFlagCountInc;
  node.desc->producerFlags.store(static_cast<uint32_t>(flag) | flagCounter);
  // NOTE: If forceNotify, still check that the consumer has been initialized.
  if ((forceNotify &&
       getProducerFlags(current) != ProducerFlags::kPendingInit) ||
      node.policy.notification != NotificationPolicy::kNever) {
    notifyConsumer(*node.desc);
  }
}

void ProducerBase::notifyConsumer(ConsumerDesc &desc) {
  notify(desc.idOrNotifyFn, mRemoteNotifyFn);
}

pw::Result<uint32_t> ProducerBase::addConsumer(pw::ConstByteSpan id,
                                               const AllocatorRegion &region,
                                               ConsumerPolicy policy) {
  if (mState != State::kActive) {
    return pw::Status::FailedPrecondition();
  }
  // Attempt to allocate a ConsumerDesc in the given region.
  auto *desc = region.allocator->New<internal::ConsumerDesc>();
  if (!desc) {
    return pw::Status::ResourceExhausted();
  }
  // Attempt to allocate a ConsumerNode to track the descriptor from the queue's
  // primary region.
  auto *node =
      mRegion.allocator->New<internal::ConsumerNode>(id, region, desc, policy);
  if (!node) {
    region.allocator->Deallocate(desc);
    return pw::Status::ResourceExhausted();
  }
  PW_TRY(checkPolicy(policy));
  // Set the policy on the node.
  node->policy = policy;
  // Initialize the descriptor.
  std::memset(desc, 0, sizeof(internal::ConsumerDesc));
  // Let the consumer know if they are overwritable.
  desc->overwritePolicy = policy.overwrite;
  desc->consumerFlags.store(
      static_cast<uint32_t>(internal::ConsumerFlags::kFlagsCleared));
  desc->producerFlags.store(
      static_cast<uint32_t>(internal::ProducerFlags::kPendingInit) |
      internal::kFlagCountInc);
  // Link the node to the list of consumers.
  mQueue->consumerList.push_back(*node);
  // Return the offset of the descriptor in the region it was allocated from.
  return toOffset(region.base, desc);
}

pw::Status ProducerBase::updateConsumerPolicy(pw::ConstByteSpan id,
                                              ConsumerPolicy policy) {
  PW_TRY(checkPolicy(policy));
  for (auto node = mQueue->consumerList.begin();
       node != mQueue->consumerList.end();) {
    if (id.size() == node->id.size() &&
        !std::memcmp(node->id.data(), id.data(), id.size())) {
      node->policy = policy;
      node->desc->overwritePolicy = policy.overwrite;
      return pw::OkStatus();
    }
    ++node;
  }
  return pw::Status::NotFound();
}

pw::Status ProducerBase::pruneConsumers(
    const pw::Function<bool(pw::ConstByteSpan id)> &match) {
  if (mState == State::kMovedFrom || !mRemoteNotifyFn) {
    return pw::Status::FailedPrecondition();
  }
  for (auto node = mQueue->consumerList.begin();
       node != mQueue->consumerList.end();) {
    if (match(node->id)) {
      // If the consumer is matched, mark it disconnected and remove it.
      setConsumerFlag(*node, node->desc->producerFlags.load(),
                      ProducerFlags::kDisconnected);
      eraseConsumerNode(node);
    } else {
      ++node;
    }
  }
  return pw::OkStatus();
}

size_t ProducerBase::getNumConsumers() {
  if (mState == State::kMovedFrom) {
    return 0;
  }
  size_t count = 0;
  // Rather than just returning mQueue->consumerList.size(), iterate over all
  // consumers so that consumers that have set ConsumerFlags::kFinished are
  // pruned and not counted.
  forAllConsumers(
      /*excludeMask=*/0,
      [&count](internal::ConsumerNode &, uint32_t) { ++count; });
  return count;
}

bool ProducerBase::isFlagInMask(internal::ConsumerDesc &desc,
                                uint32_t producerFlags, uint32_t consumerFlags,
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

void ProducerBase::eraseConsumerNode(
    decltype(QueuePrivate::consumerList)::iterator &node) {
  // Remove the node from all containers.
  auto *nodePtr = &*node;
  node = mQueue->consumerList.erase(node);
  // TODO(b/449573761): Remove the consumer from any other containers it
  // may be present in.
  // Delete the descriptor and node.
  nodePtr->region.allocator->Delete(nodePtr->desc);
  mRegion.allocator->Delete(nodePtr);
}

pw::Status ProducerBase::checkPolicy(ConsumerPolicy policy) {
  if (policy.notification == NotificationPolicy::kHighWaterMark ||
      policy.notification == NotificationPolicy::kOpportunistic) {
    uint64_t threshold = policy.data;
    if (mQueue->config.mode == Queue::DataConfig::Mode::kFixedSize) {
      threshold *= mQueue->config.fixedSize.elementSize;
    }
    if (threshold > capacity()) {
      return pw::Status::InvalidArgument();
    }
  }
  return pw::OkStatus();
}

pw::Result<std::pair<Queue *, ConsumerDesc *>> ConsumerBase::checkArgs(
    const Region &region, const Region *descRegion, uint32_t queueOffset,
    uint32_t descOffset) {
  auto *queue = fromOffset<Queue>(region, queueOffset);
  auto *desc = descRegion ? fromOffset<ConsumerDesc>(*descRegion, descOffset)
                          : fromOffset<ConsumerDesc>(region, descOffset);
  if (!queue || !desc) {
    return pw::Status::InvalidArgument();
  }
  return std::make_pair(queue, desc);
}

ConsumerBase::ConsumerBase(const Region &region, Queue &queue,
                           ConsumerDesc &desc,
                           pw::allocator::Layout baseBlockLayout,
                           uint32_t dataOffset, RemoteNotifyFn remoteNotifyFn,
                           MemoryAccess *memAccess)
    : mRegion(region),
      mRemoteNotifyFn(std::move(remoteNotifyFn)),
      kBlockLayout{baseBlockLayout.size() + queue.blockCapacity,
                   baseBlockLayout.alignment()},
      mQueue(&queue),
      mDesc(&desc),
      mMemAccess(memAccess),
      kBlockCapacity(queue.blockCapacity),
      kDataOffset(dataOffset) {}

pw::Status ConsumerBase::initialize(
    IdOrNotifyFn idOrNotifyFn, std::optional<size_t> overwriteResetOffset) {
  if (!mRemoteNotifyFn != mQueue->localNotify) {
    return pw::Status::FailedPrecondition();
  }
  auto consumerFlags = mDesc->consumerFlags.load();
  mCurrentFlags = mDesc->producerFlags.load();
  if (getAndCheckProducerFlags(mCurrentFlags, consumerFlags) !=
      ProducerFlags::kPendingInit) {
    return pw::Status::FailedPrecondition();
  }
  mDesc->idOrNotifyFn = idOrNotifyFn;
  PW_TRY(syncToProducer());
  mOverwriteResetOffset = overwriteResetOffset.value_or(capacity() / 2);
  clearFlags();
  return pw::OkStatus();
}

ConsumerBase::~ConsumerBase() {
  if (!mActive) {
    return;
  }
  disableAndNotify();
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
    case ProducerFlags::kFinished:
      // Notify the producer that this consumer can be cleaned up.
      disableAndNotify();
      return pw::Status::Aborted();
    case ProducerFlags::kPendingInit:
      // This should not happen and may indicate that this instance has
      // outlived the producer that it was registered with. Handle it
      // accordingly.
      [[fallthrough]];
    case ProducerFlags::kDisconnected:
      mActive = false;
      return pw::Status::Aborted();
    case ProducerFlags::kOverwrite: {
      PW_TRY(handleOverwrite());
      clearFlags();
      return pw::Status::DataLoss();
    }
    case ProducerFlags::kBlocking:
      // This state is used to trigger a notification to the producer on a
      // read. Since we can't force the user to read, it isn't worth surfacing
      // to the user.
      [[fallthrough]];
    case ProducerFlags::kNone:
      // As long as we're in a good state, keep the epoch in sync.
      mBlockListEpoch = mQueue->blockListEpoch.load();
      return pw::OkStatus();
    default:  // Unexpected flag value. Clear it.
      clearFlags();
      return pw::OkStatus();
  }
}

pw::Result<pw::ConstByteSpan> ConsumerBase::peek(size_t count) {
  PW_TRY(checkAvailable(count));
  if (!mPeeked) {
    mCurrBlock = mHeadBlock;
    mCurrBlockIndex =
        (mDesc->readIndex.load() + mDesc->indexCorrection) % kBlockCapacity;
  }
  mPeeked += count;
  const auto *data = blockData(mCurrBlock, kDataOffset) + mCurrBlockIndex;
  uint32_t advance = count;
  if (advanceContiguous(mCurrBlock->baseIndex.load(),
                        mCurrBlock->skipIndex.load(), kBlockCapacity,
                        mCurrBlockIndex, advance)) {
    mCurrBlock = fromOffset<BlockHeader>(
        mRegion, mCurrBlock->nextBlockOffset.load(), kBlockLayout);
    mCurrBlockIndex = mCurrBlock->baseIndex.load();
  }
  PW_TRY(checkState());
  return pw::ConstByteSpan(data, count);
}

pw::Status ConsumerBase::release(size_t count) {
  PW_TRY(checkState());
  // It is valid to peek more than the available count. If so, clear mPeeked
  // and update mAvailable.
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

pw::Status ConsumerBase::resync(size_t offset) {
  PW_TRY(checkState());
  PW_TRY(updateAvailable());
  if (offset > mAvailable) {
    return pw::Status::OutOfRange();
  }
  advanceReadIndex(mAvailable - offset, /*buf=*/std::nullopt);
  mAvailable -= offset;
  mPeeked = 0;  // Reset the current block/index to the new head.
  return pw::OkStatus();
}

pw::Result<size_t> ConsumerBase::size() {
  PW_TRY(checkState());
  PW_TRY(updateAvailable());
  return mAvailable;
}

pw::Status ConsumerBase::checkAvailable(size_t count) {
  PW_TRY(checkState());
  if (count > capacity()) {
    // If the epoch has changed, check against the updated capacity.
    mBlockListEpoch = mQueue->blockListEpoch.load();
    if (count > capacity()) {
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
      auto *nextBlock = fromOffset<BlockHeader>(
          mRegion, mHeadBlock->nextBlockOffset.load(), kBlockLayout);
      correction +=
          indexCorrectionIncrement(mHeadBlock, nextBlock, kBlockCapacity);
      mHeadBlock = nextBlock;
      blockIndex = mHeadBlock->baseIndex.load();
    }
  }
  mDesc->readIndex.store(readIndex + count);
  mDesc->indexCorrection = correction;
  if (getProducerFlags(mCurrentFlags) == ProducerFlags::kBlocking) {
    notifyProducer();
    clearFlags();
  }
}

pw::Status ConsumerBase::handleOverwrite() {
  // If the epoch has changed, just sync to the producer.
  if (mQueue->blockListEpoch.load() != mBlockListEpoch) {
    return syncToProducer();
  }
  // Update mAvailable to determine how much to fast-forward.
  PW_TRY(updateAvailable());
  // Cap the offset from the write index to half the current queue capacity.
  // This avoids fast-forwarding by such a small amount that the consumer gets
  // overwritten again quickly.
  auto offset = std::min(mOverwriteResetOffset, capacity() / 2);
  // It should not be possible for mAvailable to be smaller than the offset,
  // as that would require the capacity to have increased (meaning the epoch
  // would have changed).
  PW_ASSERT(mAvailable >= offset);
  advanceReadIndex(mAvailable - offset, /*buf=*/std::nullopt);
  mAvailable -= offset;
  // If the epoch changed since we attempted to fast forward, the fast forward
  // is invalidated. Sync to the producer.
  if (mQueue->blockListEpoch.load() != mBlockListEpoch) {
    return syncToProducer();
  }
  return pw::OkStatus();
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
  mHeadBlock = fromOffset<BlockHeader>(mRegion, producerDesc->tailBlockOffset,
                                       kBlockLayout);
  mBlockListEpoch = mQueue->blockListEpoch.load();
  return pw::OkStatus();
}

pw::Result<ProducerDesc *> ConsumerBase::getProducerDesc() {
  auto *producerDesc =
      fromOffset<ProducerDesc>(mRegion, mQueue->producerOffset.load());
  if (!producerDesc) {
    disableAndNotify();
    return pw::Status::Aborted();
  }
  return producerDesc;
}

size_t ConsumerBase::capacity() {
  return mQueue->blockCapacity * blockCountForEpoch(mBlockListEpoch);
}

void ConsumerBase::disableAndNotify() {
  mActive = false;
  mDesc->consumerFlags.store(static_cast<uint32_t>(ConsumerFlags::kFinished));
  notifyProducer();
}

void ConsumerBase::notifyProducer() {
  notify(mDesc->idOrNotifyFn, mRemoteNotifyFn);
}

void ConsumerBase::clearFlags() {
  auto counter = getFlagsCounter(mCurrentFlags);
  mDesc->consumerFlags.store(
      static_cast<uint32_t>(ConsumerFlags::kFlagsCleared) | counter);
  mCurrentFlags = static_cast<uint32_t>(ProducerFlags::kNone) | counter;
}

}  // namespace internal

void DataNotifier::onWrite(internal::ProducerBase &producer) {
  // Only notify consumers that are in a good state to read (i.e. either no
  // flags or ProducerFlags::kBlocking).
  uint16_t excludeMask =
      ~(static_cast<uint16_t>(internal::ProducerFlags::kBlocking));
  uint32_t tail = producer.mDesc->writeIndex.load();
  producer.forAllConsumers(
      excludeMask,
      [&](internal::ConsumerNode &node, uint32_t /*producerFlags*/,
          uint32_t tail) {
        auto &desc = *node.desc;
        bool clearPeriod = true;
        auto policyData = node.policy.data;
        switch (node.policy.notification) {
          case NotificationPolicy::kNever:
            break;
          case NotificationPolicy::kOpportunistic:
            // Check that either the queue is local or the remote endpoint is
            // active. Then check that the low watermark has been reached.
            if ((!producer.mRemoteNotifyFn || isActive(node.id))) {
              notifyIfAtWatermark(producer, tail, policyData, desc);
            }
            break;
          case NotificationPolicy::kHighWaterMark:
            notifyIfAtWatermark(producer, tail, policyData, desc);
            break;
          case NotificationPolicy::kStreaming:
            producer.notifyConsumer(desc);
            break;
          case NotificationPolicy::kPeriodic:
            clearPeriod = false;
            updatePeriod(producer, node, policyData);
            break;
          default:
            // Invalid policy. Ignore.
            break;
        }
        if (clearPeriod) {
          // Disable any timers associated with this consumer.
          updatePeriod(producer, node, /*periodMs=*/std::nullopt);
        }
      },
      tail);
}

void DataNotifier::updatePeriod(internal::ProducerBase &producer,
                                internal::ConsumerNode &consumer,
                                std::optional<uint32_t> periodMs) {
  if (periodMs) {
    // The default implementation has no timer support, so just notify the
    // consumer.
    producer.notifyConsumer(*consumer.desc);
  }
}

void DataNotifier::notifyIfAtWatermark(internal::ProducerBase &producer,
                                       uint32_t writeIndex, uint32_t policyData,
                                       internal::ConsumerDesc &consumer) {
  // Calculate the threshold in bytes from the policy data and queue
  // configuration.
  uint32_t threshold = policyData;
  if (producer.mQueue->config.mode == DataConfigMode::kFixedSize) {
    threshold *= producer.mQueue->config.fixedSize.elementSize;
  }
  if (internal::writeReadDiff(writeIndex, consumer.readIndex.load()) >=
      threshold) {
    producer.notifyConsumer(consumer);
  }
}

pw::Result<VariableDataProducer> VariableDataProducer::createLocal(
    AllocatorRegion region, void *queue, size_t maxBlockCount,
    size_t minBlockCount, DataNotifier &dataNotifier,
    LocalNotifyArgs notifyArgs, MemoryAccess *memAccess) {
  if (!notifyArgs.fn || !queue) {
    return pw::Status::InvalidArgument();
  }
  auto queuePtr = static_cast<internal::QueuePrivate *>(queue);
  if (queuePtr->config.mode == DataConfigMode::kFixedSize ||
      !queuePtr->localNotify) {
    return pw::Status::FailedPrecondition();
  } else if (queuePtr->config.mode == DataConfigMode::kVariableSizeAligned) {
    return pw::Status::Unimplemented();
  }
  auto blockLayout = internal::variableDataBlockLayout(queuePtr->blockCapacity);
  PW_TRY(Base::initialize(region, queuePtr, blockLayout, maxBlockCount,
                          minBlockCount, {.localNotify = notifyArgs}));
  return VariableDataProducer(region, *queuePtr, blockLayout, maxBlockCount,
                              minBlockCount, dataNotifier,
                              /*remoteNotifyFn=*/{}, memAccess);
}

pw::Result<VariableDataProducer> VariableDataProducer::createRemote(
    AllocatorRegion region, void *queue, size_t maxBlockCount,
    size_t minBlockCount, DataNotifier &dataNotifier,
    RemoteNotifyArgs notifyArgs, MemoryAccess *memAccess) {
  if (!notifyArgs.fn || !queue) {
    return pw::Status::InvalidArgument();
  }
  auto queuePtr = static_cast<internal::QueuePrivate *>(queue);
  if (queuePtr->config.mode == DataConfigMode::kFixedSize ||
      queuePtr->localNotify) {
    return pw::Status::FailedPrecondition();
  } else if (queuePtr->config.mode == DataConfigMode::kVariableSizeAligned) {
    return pw::Status::Unimplemented();
  }
  auto blockLayout = internal::variableDataBlockLayout(queuePtr->blockCapacity);
  PW_TRY(Base::initialize(region, queuePtr, blockLayout, maxBlockCount,
                          minBlockCount, {.remoteId = notifyArgs.id}));
  return VariableDataProducer(region, *queuePtr, blockLayout, maxBlockCount,
                              minBlockCount, dataNotifier,
                              std::move(notifyArgs.fn), memAccess);
}

VariableDataProducer::VariableDataProducer(
    const AllocatorRegion &region, internal::QueuePrivate &queue,
    pw::allocator::Layout blockLayout, size_t maxBlockCount,
    size_t minBlockCount, DataNotifier &dataNotifier,
    RemoteNotifyFn remoteNotifyFn, MemoryAccess *memAccess)
    : ProducerBase(region, queue, blockLayout,
                   offsetof(internal::VariableDataBlock, data), maxBlockCount,
                   minBlockCount, dataNotifier, std::move(remoteNotifyFn),
                   memAccess) {}

pw::Result<pw::ByteSpan> VariableDataProducer::reserve(size_t count) {
  if (mCurrentHdrPtr) {
    PW_TRY_ASSIGN(auto reservation, Base::reserve(count));
    mCurrentHdrPtr->size += count;
    return reservation;
  }
  // Reserve space for the element size and data.
  PW_TRY_ASSIGN(auto reservation,
                Base::reserve(count + sizeof(internal::VariableDataHeader)));
  mCurrentHdrPtr =
      reinterpret_cast<internal::VariableDataHeader *>(reservation.data());
  mCurrentHdrPtr->size = count;
  reservation = reservation.subspan(sizeof(internal::VariableDataHeader));
  // If the reservation was at the end of a contiguous chunk, retrieve the next
  // contiguous chunk. This must succeed.
  if (reservation.empty()) {
    reservation = Base::reserve(count).value();
  }
  return reservation;
}

pw::Status VariableDataProducer::truncate(size_t size) {
  PW_TRY(Base::truncate(size + sizeof(internal::VariableDataHeader)));
  // Store the new size. The memory address of the element size has not changed.
  mCurrentHdrPtr->size = size;
  return pw::OkStatus();
}

pw::Status VariableDataProducer::commit() {
  if (!mCurrentHdrPtr) {
    return pw::Status::FailedPrecondition();
  }
  mCurrentHdrPtr = nullptr;
  updateFirstElementIndex();  // Enable consumers to seek to an element.
  // Commit the entire reservation. Notifies consumers as required.
  PW_TRY(Base::commit(mReserved));
  alignWriteIndex();  // Next element header should be aligned.
  return pw::OkStatus();
}

pw::Status VariableDataProducer::push(pw::ConstByteSpan element) {
  if (mReserved) {
    return pw::Status::FailedPrecondition();
  }
  PW_TRY(checkAvailable(element.size() + sizeof(internal::VariableDataHeader),
                        /*allOrNothing=*/true));
  updateFirstElementIndex();  // Enable consumers to seek to an element.
  internal::VariableDataHeader hdr{.size =
                                       static_cast<uint32_t>(element.size())};
  advanceWriteIndex(sizeof(hdr), pw::as_bytes(pw::span(&hdr, 1)));
  advanceWriteIndex(hdr.size, element);
  alignWriteIndex();  // Next element header should be aligned.
  // Notify consumers as required.
  mDataNotifier->onWrite(*this);
  return pw::OkStatus();
}

void VariableDataProducer::updateFirstElementIndex() {
  auto *tailBlock = internal::fromOffset<internal::VariableDataBlock>(
      mRegion, mDesc->tailBlockOffset, kBlockLayout);
  if (tailBlock->header.firstElementIndex == kBlockCapacity) {
    // Only set the first element index if this is the first variable size
    // element to be written into this block (on this pass through the block).
    tailBlock->header.firstElementIndex =
        (mDesc->writeIndex.load() + mDesc->indexCorrection) % kBlockCapacity;
  }
}

void VariableDataProducer::enterNextBlock(internal::BlockHeader *&block,
                                          uint32_t *correction, uint32_t &index,
                                          bool convertSkipToBase) {
  Base::enterNextBlock(block, correction, index, convertSkipToBase);
  auto *varDataBlock = reinterpret_cast<internal::VariableDataBlock *>(block);
  varDataBlock->header.firstElementIndex = kBlockCapacity;
}

void VariableDataProducer::alignWriteIndex() {
  constexpr size_t kAlignment = alignof(internal::VariableDataHeader);
  if (auto offset = mDesc->writeIndex.load() & (kAlignment - 1); offset) {
    advanceWriteIndex(kAlignment - offset, /*buf=*/std::nullopt);
  }
}

pw::Result<VariableDataConsumer> VariableDataConsumer::createLocal(
    Region region, uint32_t queueOffset, uint32_t descOffset,
    LocalNotifyArgs notifyArgs, MemoryAccess *memAccess,
    std::optional<size_t> overwriteResetOffset) {
  if (!notifyArgs.fn) {
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(auto queueAndDesc, checkArgs(region, /*descRegion=*/nullptr,
                                             queueOffset, descOffset));
  if (queueAndDesc.first->config.mode == DataConfigMode::kFixedSize) {
    return pw::Status::FailedPrecondition();
  } else if (queueAndDesc.first->config.mode ==
             DataConfigMode::kVariableSizeAligned) {
    return pw::Status::Unimplemented();
  }
  VariableDataConsumer consumer(region, *queueAndDesc.first,
                                *queueAndDesc.second,
                                /*remoteNotifyFn=*/{}, memAccess);
  PW_TRY(
      consumer.initialize({.localNotify = notifyArgs}, overwriteResetOffset));
  return consumer;
}

pw::Result<VariableDataConsumer> VariableDataConsumer::createRemote(
    Region region, std::optional<Region> descRegion, uint32_t queueOffset,
    uint32_t descOffset, RemoteNotifyArgs notifyArgs, MemoryAccess *memAccess,
    std::optional<size_t> overwriteResetOffset) {
  if (!notifyArgs.fn) {
    return pw::Status::InvalidArgument();
  }
  auto *descRegionPtr = descRegion ? &*descRegion : nullptr;
  PW_TRY_ASSIGN(auto queueAndDesc,
                checkArgs(region, descRegionPtr, queueOffset, descOffset));
  if (queueAndDesc.first->config.mode == DataConfigMode::kFixedSize) {
    return pw::Status::FailedPrecondition();
  } else if (queueAndDesc.first->config.mode ==
             DataConfigMode::kVariableSizeAligned) {
    return pw::Status::Unimplemented();
  }
  VariableDataConsumer consumer(region, *queueAndDesc.first,
                                *queueAndDesc.second, std::move(notifyArgs.fn),
                                memAccess);
  PW_TRY(
      consumer.initialize({.remoteId = notifyArgs.id}, overwriteResetOffset));
  return consumer;
}

VariableDataConsumer::VariableDataConsumer(const Region &region,
                                           internal::Queue &queue,
                                           internal::ConsumerDesc &desc,
                                           RemoteNotifyFn remoteNotifyFn,
                                           MemoryAccess *memAccess)
    : ConsumerBase(region, queue, desc, internal::variableDataBlockLayout(0),
                   offsetof(internal::VariableDataBlock, data),
                   std::move(remoteNotifyFn), memAccess) {}

pw::Result<size_t> VariableDataConsumer::getHeadSize() {
  // TODO(b/455007019): Implement
  return pw::Status::Unimplemented();
}

pw::Result<pw::ConstByteSpan> VariableDataConsumer::peek() {
  // TODO(b/455007019): Implement
  return pw::Status::Unimplemented();
}

pw::Status VariableDataConsumer::release() {
  // TODO(b/455007019): Implement
  return pw::Status::Unimplemented();
}

pw::Status VariableDataConsumer::pop(pw::ByteSpan & /*buffer*/) {
  // TODO(b/455007019): Implement
  return pw::Status::Unimplemented();
}

pw::Status VariableDataConsumer::resync(size_t /*offset*/) {
  // TODO(b/455007019): Implement
  return pw::Status::Unimplemented();
}

pw::Result<size_t> VariableDataConsumer::size() {
  // TODO(b/455007019): Implement
  return pw::Status::Unimplemented();
}

pw::Status initQueue(void *queue, size_t capacity, size_t elementSize,
                     size_t elementAlignment, bool local) {
  auto &queueRef = *static_cast<internal::Queue *>(queue);
  queueRef.producerOffset = internal::kOffsetInvalid;
  queueRef.blockCapacity = capacity;
  if (elementSize) {
    if (capacity % elementSize != 0) {
      return pw::Status::InvalidArgument();
    }
    queueRef.config.mode = DataConfigMode::kFixedSize;
    queueRef.config.fixedSize.elementSize = elementSize;
    queueRef.config.fixedSize.elementAlignment = elementAlignment;
  } else {
    queueRef.config.mode = DataConfigMode::kVariableSizeBasic;
  }
  queueRef.localNotify = local;
  return pw::OkStatus();
}

pw::Result<void *> createVariableDataQueue(pw::Allocator &allocator,
                                           size_t blockCapacity, bool local) {
  if (auto *queue = allocator.New<internal::QueuePrivate>(); queue) {
    PW_TRY(initQueue(queue, blockCapacity, /*elementSize=*/0,
                     /*elementAlignment=*/0, local));
    return queue;
  }
  return pw::Status::ResourceExhausted();
}

pw::Result<void *> createQueueUntyped(pw::Allocator &allocator,
                                      size_t blockCapacity, size_t elementSize,
                                      size_t elementAlignment, bool local) {
  if (auto *queue = allocator.New<internal::QueuePrivate>(); queue) {
    PW_TRY(initQueue(queue, blockCapacity * elementSize, elementSize,
                     elementAlignment, local));
    return queue;
  }
  return pw::Status::ResourceExhausted();
}

pw::Result<UntypedProducer> UntypedProducer::createLocal(
    AllocatorRegion region, void *queue, size_t maxBlockCount,
    size_t minBlockCount, DataNotifier &dataNotifier,
    LocalNotifyArgs notifyArgs, MemoryAccess *memAccess) {
  if (notifyArgs.fn == nullptr || !queue) {
    return pw::Status::InvalidArgument();
  }
  auto queuePtr = static_cast<internal::QueuePrivate *>(queue);
  if (queuePtr->config.mode != DataConfigMode::kFixedSize ||
      queuePtr->blockCapacity % queuePtr->config.fixedSize.elementSize != 0 ||
      !queuePtr->localNotify) {
    return pw::Status::FailedPrecondition();
  }
  auto blockLayout = internal::getBlockLayout(*queuePtr);
  PW_TRY(ProducerBase::initialize(region, queuePtr, blockLayout, maxBlockCount,
                                  minBlockCount, {.localNotify = notifyArgs}));
  return UntypedProducer(region, *queuePtr, blockLayout, maxBlockCount,
                         minBlockCount, dataNotifier, {}, memAccess);
}

pw::Result<UntypedProducer> UntypedProducer::createRemote(
    AllocatorRegion region, void *queue, size_t maxBlockCount,
    size_t minBlockCount, DataNotifier &dataNotifier,
    RemoteNotifyArgs notifyArgs, MemoryAccess *memAccess) {
  if (notifyArgs.fn == nullptr || !queue) {
    return pw::Status::InvalidArgument();
  }
  auto queuePtr = static_cast<internal::QueuePrivate *>(queue);
  if (queuePtr->config.mode != DataConfigMode::kFixedSize ||
      queuePtr->blockCapacity % queuePtr->config.fixedSize.elementSize != 0 ||
      queuePtr->localNotify) {
    return pw::Status::FailedPrecondition();
  }
  auto blockLayout = internal::getBlockLayout(*queuePtr);
  PW_TRY(ProducerBase::initialize(region, queuePtr, blockLayout, maxBlockCount,
                                  minBlockCount, {.remoteId = notifyArgs.id}));
  return UntypedProducer(region, *queuePtr, blockLayout, maxBlockCount,
                         minBlockCount, dataNotifier, std::move(notifyArgs.fn),
                         memAccess);
}

UntypedProducer::UntypedProducer(const AllocatorRegion &region,
                                 internal::QueuePrivate &queue,
                                 pw::allocator::Layout blockLayout,
                                 size_t maxBlockCount, size_t minBlockCount,
                                 DataNotifier &dataNotifier,
                                 RemoteNotifyFn remoteNotifyFn,
                                 MemoryAccess *memAccess)
    : ProducerBase(region, queue, blockLayout, internal::getDataOffset(queue),
                   maxBlockCount, minBlockCount, dataNotifier,
                   std::move(remoteNotifyFn), memAccess),
      mElementSize(queue.config.fixedSize.elementSize),
      mElementAlignment(queue.config.fixedSize.elementAlignment) {}

pw::Result<UntypedConsumer> UntypedConsumer::createLocal(
    Region region, uint32_t queueOffset, uint32_t descOffset,
    LocalNotifyArgs notifyArgs, MemoryAccess *memAccess,
    std::optional<size_t> overwriteResetOffset) {
  if (!notifyArgs.fn) {
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(auto queueAndDesc, checkArgs(region, /*descRegion=*/nullptr,
                                             queueOffset, descOffset));
  if (queueAndDesc.first->config.mode != DataConfigMode::kFixedSize) {
    return pw::Status::FailedPrecondition();
  }
  UntypedConsumer consumer(region, *queueAndDesc.first, *queueAndDesc.second,
                           /*remoteNotifyFn=*/{}, memAccess);
  PW_TRY(
      consumer.initialize({.localNotify = notifyArgs}, overwriteResetOffset));
  return consumer;
}

pw::Result<UntypedConsumer> UntypedConsumer::createRemote(
    Region region, std::optional<Region> descRegion, uint32_t queueOffset,
    uint32_t descOffset, RemoteNotifyArgs notifyArgs, MemoryAccess *memAccess,
    std::optional<size_t> overwriteResetOffset) {
  if (!notifyArgs.fn) {
    return pw::Status::InvalidArgument();
  }
  auto *descRegionPtr = descRegion ? &*descRegion : nullptr;
  PW_TRY_ASSIGN(auto queueAndDesc,
                checkArgs(region, descRegionPtr, queueOffset, descOffset));
  if (queueAndDesc.first->config.mode != DataConfigMode::kFixedSize) {
    return pw::Status::FailedPrecondition();
  }
  UntypedConsumer consumer(region, *queueAndDesc.first, *queueAndDesc.second,
                           std::move(notifyArgs.fn), memAccess);
  PW_TRY(
      consumer.initialize({.remoteId = notifyArgs.id}, overwriteResetOffset));
  return consumer;
}

UntypedConsumer::UntypedConsumer(const Region &region, internal::Queue &queue,
                                 internal::ConsumerDesc &desc,
                                 RemoteNotifyFn remoteNotifyFn,
                                 MemoryAccess *memAccess)
    : ConsumerBase(region, queue, desc, internal::getBlockLayout(queue),
                   internal::getDataOffset(queue), std::move(remoteNotifyFn),
                   memAccess),
      mElementSize(queue.config.fixedSize.elementSize),
      mElementAlignment(queue.config.fixedSize.elementAlignment) {}

}  // namespace android::contexthub::data_flow
