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

#define PW_LOG_MODULE_NAME "DATA_FLOW.Queue"

#include "data_flow/queue.h"

#include <inttypes.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "chre/platform/atomic_ref.h"
#include "data_flow/internal/queue_internal.h"
#include "data_flow/queue_defs.h"
#include "data_flow/untyped_queue.h"
#include "pw_allocator/layout.h"
#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_log/log.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {

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
    block =
        fromOffset<BlockHeader>(region, block->nextBlockOffsetBytes, layout);
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
    chre::AtomicUint32Ref(block->baseIndex).store(0);
    chre::AtomicUint32Ref(block->skipIndex).store(blockCapacity);
  }
  if (variableData) {
    auto *variableDataBlock =
        reinterpret_cast<VariableDataBlockHeader *>(block);
    variableDataBlock->firstElementIndex = blockCapacity;
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
    PW_LOG_ERROR("allocateBlockRing: requested 0 blocks.");
    return pw::Status::InvalidArgument();
  }
  BlockHeader *head = nullptr, *prev = nullptr;
  for (size_t i = 0; i < count; ++i) {
    if (auto *block = allocateBlock(*region.allocator, layout, blockCapacity,
                                    variableData);
        block) {
      if (!head) {
        head = block;
      } else {
        prev->nextBlockOffsetBytes = toOffset(region.base, block);
      }
      prev = block;
    } else {
      deallocateBlockRing(region, layout, head);
      PW_LOG_ERROR("allocateBlockRing: Failed to allocate.");
      return pw::Status::ResourceExhausted();
    }
  }
  prev->nextBlockOffsetBytes = internal::toOffset(region.base, head);
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
  chre::AtomicUint32Ref(desc.writeIndex).store(writeIndex);
  desc.indexCorrection = correction;
  desc.tailBlockOffsetBytes = toOffset(shmemBase, tailBlock);
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
  auto baseIndex = chre::AtomicUint32Ref(curr->baseIndex).load();
  auto skipIndex = chre::AtomicUint32Ref(curr->skipIndex).load();
  uint32_t diffBase = skipIndex == capacity ? baseIndex : skipIndex;
  return ringDiff(chre::AtomicUint32Ref(next->baseIndex).load(), diffBase,
                  capacity);
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
  size_t elementAlignment =
      queue.elementConfig.get<ElementConfig::Tag::fixedSize>()
          .elementAlignmentBytes;
  return internal::alignTo(sizeof(internal::BlockHeader), elementAlignment);
}

/**
 * @return the offset of data within a block for a fixed-size element queue.
 * @param alignment The alignment of the element type.
 */
size_t getDataOffset(size_t elementAlignment) {
  return internal::alignTo(sizeof(internal::BlockHeader), elementAlignment);
}

/**
 * @return the base block layout for the given fixed-size element queue.
 * @param queue The queue metadata.
 */
pw::allocator::Layout getBlockBaseLayout(internal::Queue &queue) {
  size_t elementAlignment =
      queue.elementConfig.get<ElementConfig::Tag::fixedSize>()
          .elementAlignmentBytes;
  size_t blockAlignment =
      std::max(alignof(internal::BlockHeader), elementAlignment);
  return pw::allocator::Layout(getDataOffset(queue), blockAlignment);
}

/**
 * @return the block layout for the given fixed-size element queue.
 * @param blockCapacity The capacity of each block in elements.
 * @param elementSize The size of each element in bytes.
 * @param elementAlignment The alignment of the element type.
 */
pw::allocator::Layout getBlockLayout(size_t blockCapacity, size_t elementSize,
                                     size_t elementAlignment) {
  size_t blockAlignment =
      std::max(alignof(internal::BlockHeader), elementAlignment);
  return pw::allocator::Layout(
      getDataOffset(elementAlignment) + blockCapacity * elementSize,
      blockAlignment);
}

}  // namespace

pw::Status ProducerBase::checkArgs(const AllocatorRegion &region,
                                   size_t maxBlockCount, size_t minBlockCount) {
  if (region.size > UINT32_MAX || !region.allocator ||
      maxBlockCount < minBlockCount) {
    PW_LOG_ERROR("ProducerBase::checkArgs: Invalid arguments");
    return pw::Status::InvalidArgument();
  }
  return pw::OkStatus();
}

pw::Result<QueuePrivate *> ProducerBase::initQueue(
    const AllocatorRegion &region, size_t capacity, size_t elementSize,
    size_t elementAlignment, IdOrNotifyFn idOrNotifyFn, bool local) {
  if (!elementAlignment || (elementAlignment & (elementAlignment - 1)) != 0) {
    PW_LOG_ERROR("initQueue: elementAlignment %zu is not a power of 2",
                 elementAlignment);
    return pw::Status::InvalidArgument();
  }
  auto *queue = region.allocator->New<internal::QueuePrivate>();
  if (!queue) {
    return pw::Status::ResourceExhausted();
  }
  queue->queue.sourceMetadataOffsetBytes = internal::kOffsetInvalid;
  if (elementSize) {
    if (capacity % elementSize != 0) {
      PW_LOG_ERROR(
          "initQueue: capacity %zu is not a multiple of element size %zu",
          capacity, elementSize);
      region.allocator->Delete(queue);
      return pw::Status::InvalidArgument();
    }
    queue->queue.blockCapacityBytes = capacity;
    queue->queue.elementConfig.set<ElementConfig::Tag::fixedSize>(
        ElementConfig::FixedSize{
            .elementSizeBytes = static_cast<int32_t>(elementSize),
            .elementAlignmentBytes = static_cast<char16_t>(elementAlignment)});
  } else {
    if (elementAlignment > 1) {
      PW_LOG_ERROR(
          "initQueue: elementAlignment %zu > 1 not supported yet for variable "
          "size elements",
          elementAlignment);
      region.allocator->Delete(queue);
      return pw::Status::InvalidArgument();
    }
    queue->queue.blockCapacityBytes =
        internal::alignTo(capacity, alignof(internal::VariableElementHeader));
    queue->queue.elementConfig.set<ElementConfig::Tag::variableSize>(
        ElementConfig::VariableSize{
            .elementAlignmentBytes = static_cast<char16_t>(elementAlignment)});
  }
  std::memcpy(&queue->queue.sourceId, &idOrNotifyFn, sizeof(IdOrNotifyFn));
  queue->queue.localNotify = local;
  return queue;
}

ProducerBase::ProducerBase(const AllocatorRegion &region, QueuePrivate &queue,
                           pw::allocator::Layout blockLayout,
                           uint32_t blockCapacity, uint32_t dataOffset,
                           size_t /*maxBlockCount*/, size_t minBlockCount,
                           DataNotifier &dataNotifier,
                           RemoteNotifyFn remoteNotifyFn,
                           MemoryAccess *memAccess)
    : mRegion(region),
      mRemoteNotifyFn(std::move(remoteNotifyFn)),
      mQueue(&queue),
      mDataNotifier(&dataNotifier),
      mMemAccess(memAccess),
      kBlockLayout(blockLayout),
      kDataOffset(dataOffset),
      kBlockCapacity(blockCapacity),
      mCurrBlock(nullptr),  // Indicate that it is uninitialized.
      mBlockCount(minBlockCount) {}

pw::Status ProducerBase::initialize(bool variableData) {
  PW_TRY_ASSIGN(mCurrBlock,
                allocateBlockRing(mRegion, kBlockLayout, kBlockCapacity,
                                  mBlockCount, variableData));
  mDesc = &mCurrBlock->sourceMetadata;
  initProducerDesc(*mDesc, /*writeIndex=*/0, /*correction=*/0, mCurrBlock,
                   mRegion.base);
  chre::AtomicUint32Ref(mQueue->queue.blockListEpoch)
      .store(getBlockListEpoch(mBlockCount, /*epoch=*/0));
  chre::AtomicUint32Ref(mQueue->queue.sourceMetadataOffsetBytes)
      .store(toOffset(mRegion.base, mDesc));
  return pw::OkStatus();
}

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
  deallocateBlockRing(mRegion, kBlockLayout, mCurrBlock);
  mRegion.allocator->Deallocate(mQueue);
}

uint32_t ProducerBase::getQueueOffset() const {
  return toOffset(mRegion.base, mQueue);
}

void ProducerBase::stop() {
  if (mState != State::kActive) {
    return;
  }
  mState = State::kStopped;
  chre::AtomicUint32Ref(mQueue->queue.sourceMetadataOffsetBytes)
      .store(kOffsetInvalid);
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
  PW_TRY(checkActive());
  PW_TRY_ASSIGN(uint32_t size, checkAvailable(count, /*allOrNothing=*/true));
  // Return a span over the next available contiguous region.
  auto *begin = blockData(mCurrBlock, kDataOffset) + mCurrBlockIndex;
  if (advanceContiguous(chre::AtomicUint32Ref(mCurrBlock->baseIndex).load(),
                        chre::AtomicUint32Ref(mCurrBlock->skipIndex).load(),
                        kBlockCapacity, mCurrBlockIndex, size)) {
    enterNextBlock(mCurrBlock, /*correction=*/nullptr, mCurrBlockIndex,
                   /*convertSkipToBase=*/true);
  }
  mReserved += size;
  return pw::ByteSpan(begin, size);
}

pw::Status ProducerBase::truncate(size_t size) {
  PW_TRY(checkActive());
  // Check and update the size of the reservation.
  if (mReserved == 0) {
    PW_LOG_ERROR("ProducerBase::truncate: No reservation to truncate");
    return pw::Status::FailedPrecondition();
  } else if (size > mReserved) {
    PW_LOG_ERROR("ProducerBase::truncate: Size %zu exceeds reservation %zu",
                 size, mReserved);
    return pw::Status::OutOfRange();
  } else if (size == mReserved) {
    return pw::OkStatus();
  }
  mReserved = size;
  // Sync the current block and index back to the write index.
  mCurrBlock = fromOffset<BlockHeader>(mRegion, mDesc->tailBlockOffsetBytes,
                                       kBlockLayout);
  mCurrBlockIndex = (chre::AtomicUint32Ref(mDesc->writeIndex).load() +
                     mDesc->indexCorrection) %
                    kBlockCapacity;
  // Advance to the new reservation size.
  advanceBlockIndexWithData(mCurrBlock, mCurrBlockIndex, /*correction=*/nullptr,
                            size, /*data=*/std::nullopt,
                            /*convertSkipToBase=*/false);
  return pw::OkStatus();
}

pw::Status ProducerBase::commit(size_t count) {
  PW_TRY(checkActive());
  if (count > mReserved) {
    PW_LOG_ERROR("ProducerBase::commit: Count %zu exceeds reservation %zu",
                 count, mReserved);
    return pw::Status::OutOfRange();
  }
  mReserved -= count;
  advanceWriteIndex(count, /*data=*/std::nullopt);
  mDataNotifier->onWrite(*this);
  return pw::OkStatus();
}

pw::Result<size_t> ProducerBase::push(pw::ConstByteSpan data,
                                      bool allOrNothing) {
  PW_TRY(checkActive());
  if (mReserved > 0) {  // push() is not allowed while a reservation is active.
    PW_LOG_ERROR("ProducerBase::push: Active reservation");
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
  // TODO(b/448384247): This should check against the maximum capacity.
  if (count > capacity()) {
    PW_LOG_ERROR("ProducerBase::checkAvailable: Count %zu exceeds capacity %zu",
                 count, capacity());
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
  uint32_t writeIndex = chre::AtomicUint32Ref(mDesc->writeIndex).load();
  uint32_t correction = mDesc->indexCorrection;
  auto *block = fromOffset<BlockHeader>(mRegion, mDesc->tailBlockOffsetBytes,
                                        kBlockLayout);
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
        advanceContiguous(chre::AtomicUint32Ref(block->baseIndex).load(),
                          chre::AtomicUint32Ref(block->skipIndex).load(),
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
      mRegion, chre::AtomicUint32Ref(block->nextBlockOffsetBytes).load(),
      kBlockLayout);
  // If the next block was skipped from on the last visit, set its base
  // index to that skip index and reset the skip index.
  auto nextSkipIndex = chre::AtomicUint32Ref(nextBlock->skipIndex).load();
  if (convertSkipToBase && nextSkipIndex != kBlockCapacity) {
    chre::AtomicUint32Ref(nextBlock->baseIndex).store(nextSkipIndex);
    chre::AtomicUint32Ref(nextBlock->skipIndex).store(kBlockCapacity);
  }
  if (correction) {
    // Update the index correction to be applied to the write index.
    *correction += indexCorrectionIncrement(block, nextBlock, kBlockCapacity);
  }
  block = nextBlock;
  index = chre::AtomicUint32Ref(block->baseIndex).load();
}

void ProducerBase::updateWriteIndex(BlockHeader *tailBlock, uint32_t writeIndex,
                                    uint32_t correction) {
  if (tailBlock == fromOffset<BlockHeader>(mRegion, mDesc->tailBlockOffsetBytes,
                                           kBlockLayout)) {
    // If the currently linked tail block is still the tail, just store the
    // new write index.
    chre::AtomicUint32Ref(mDesc->writeIndex).store(writeIndex);
  } else {
    // Initialize the descriptor in the new tail block, then link it.
    auto &newDesc = tailBlock->sourceMetadata;
    initProducerDesc(newDesc, writeIndex, correction, tailBlock, mRegion.base);
    chre::AtomicUint32Ref(mQueue->queue.sourceMetadataOffsetBytes)
        .store(toOffset(mRegion.base, &newDesc));
    mDesc = &newDesc;
  }
}

void ProducerBase::updateAvailable(uint32_t increment) {
  auto tail = chre::AtomicUint32Ref(mDesc->writeIndex).load() + mReserved;
  mAvailable = capacity() - mReserved;  // Reset available counts.
  // Consumers that have been overwritten or would otherwise need to sync back
  // to the producer position should not block writes to the queue, as well as
  // consumers that are no longer in a valid state. Add them to the exclude
  // mask. This effectively means all ProducerFlags states except kBlocking and
  // kPendingInit.
  auto excludeMask = ~(static_cast<uint16_t>(ProducerFlags::kBlocking) |
                       static_cast<uint16_t>(ProducerFlags::kPendingInit));
  forAllConsumers(
      excludeMask,
      [this](internal::ConsumerNode &node, uint32_t producerFlags,
             uint32_t tail, uint32_t increment) {
        auto readIndex = chre::AtomicUint32Ref(node.desc->readIndex).load();
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
  chre::AtomicUint32Ref(node.desc->sourceFlags)
      .store(static_cast<uint32_t>(flag) | flagCounter);
  // NOTE: If forceNotify, still check that the consumer has been initialized.
  if ((forceNotify &&
       getProducerFlags(current) != ProducerFlags::kPendingInit) ||
      node.policy.notification != NotificationPolicy::kNever) {
    notifyConsumer(*node.desc);
  }
}

void ProducerBase::notifyConsumer(ConsumerDesc &desc) {
  notify(*reinterpret_cast<IdOrNotifyFn *>(&desc.id), mRemoteNotifyFn);
}

pw::Result<uint32_t> ProducerBase::addConsumer(pw::ConstByteSpan id,
                                               const AllocatorRegion &region,
                                               ConsumerPolicy policy) {
  PW_TRY(checkActive());
  // Attempt to allocate a ConsumerDesc in the given region.
  auto *desc = region.allocator->New<internal::ConsumerDesc>();
  if (!desc) {
    PW_LOG_ERROR("ProducerBase::addConsumer: Failed to allocate ConsumerDesc");
    return pw::Status::ResourceExhausted();
  }
  // Attempt to allocate a ConsumerNode to track the descriptor from the queue's
  // primary region.
  auto *node =
      mRegion.allocator->New<internal::ConsumerNode>(id, region, desc, policy);
  if (!node) {
    region.allocator->Deallocate(desc);
    PW_LOG_ERROR("ProducerBase::addConsumer: Failed to allocate ConsumerNode");
    return pw::Status::ResourceExhausted();
  }
  PW_TRY(checkPolicy(policy));
  // Set the policy on the node.
  node->policy = policy;
  // Initialize the descriptor.
  std::memset(desc, 0, sizeof(internal::ConsumerDesc));
  std::memcpy(&desc->id, id.data(), id.size());
  // Let the consumer know if they are overwritable.
  desc->isOverwritable = policy.overwrite == OverwritePolicy::kAllowed;
  chre::AtomicUint32Ref(desc->sinkFlags)
      .store(static_cast<uint32_t>(internal::ConsumerFlags::kFlagsCleared));
  chre::AtomicUint32Ref(desc->sourceFlags)
      .store(static_cast<uint32_t>(internal::ProducerFlags::kPendingInit) |
             internal::kFlagCountInc);
  // Sync the consumer to the producer.
  desc->indexCorrection = mDesc->indexCorrection;
  chre::AtomicUint32Ref(desc->readIndex)
      .store(chre::AtomicUint32Ref(mDesc->writeIndex).load());
  desc->initialHeadBlockOffsetBytes = mDesc->tailBlockOffsetBytes;
  desc->initialBlockListEpoch =
      chre::AtomicUint32Ref(mQueue->queue.blockListEpoch).load();
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
      node->desc->isOverwritable =
          policy.overwrite == OverwritePolicy::kAllowed;
      return pw::OkStatus();
    }
    ++node;
  }
  PW_LOG_ERROR("ProducerBase::updateConsumerPolicy: Consumer not found");
  return pw::Status::NotFound();
}

pw::Status ProducerBase::pruneConsumers(
    const pw::Function<bool(pw::ConstByteSpan id)> &match) {
  if (mState == State::kMovedFrom) {
    PW_LOG_ERROR("ProducerBase::pruneConsumers: Moved-from instance");
    return pw::Status::FailedPrecondition();
  }
  for (auto node = mQueue->consumerList.begin();
       node != mQueue->consumerList.end();) {
    if (match(node->id)) {
      // If the consumer is matched, mark it disconnected and remove it.
      setConsumerFlag(*node,
                      chre::AtomicUint32Ref(node->desc->sourceFlags).load(),
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
    PW_LOG_ERROR("ProducerBase::getNumConsumers: Moved-from instance");
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
      chre::AtomicUint32Ref(desc.sourceFlags)
          .store(internal::getFlagsCounter(producerFlags) |
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
    if (mQueue->queue.elementConfig.getTag() == ElementConfig::Tag::fixedSize) {
      threshold *=
          mQueue->queue.elementConfig.get<ElementConfig::Tag::fixedSize>()
              .elementSizeBytes;
    }
    if (threshold > capacity()) {
      PW_LOG_ERROR("ProducerBase::checkPolicy: watermark of %" PRIu64
                   " bytes exceeds capacity of %zu bytes",
                   threshold, capacity());
      return pw::Status::InvalidArgument();
    }
  }
  return pw::OkStatus();
}

pw::Status ProducerBase::checkActive() const {
  if (mState != State::kActive) {
    PW_LOG_ERROR("ProducerBase: API call on inactive producer");
    return pw::Status::FailedPrecondition();
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
    PW_LOG_ERROR("ConsumerBase::checkArgs: Invalid queue or desc offset");
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
      kBlockLayout{baseBlockLayout.size() + queue.blockCapacityBytes,
                   baseBlockLayout.alignment()},
      mQueue(&queue),
      mDesc(&desc),
      mMemAccess(memAccess),
      kBlockCapacity(queue.blockCapacityBytes),
      kDataOffset(dataOffset) {}

pw::Status ConsumerBase::initialize(
    IdOrNotifyFn idOrNotifyFn, std::optional<size_t> overwriteResetOffset) {
  if (!mRemoteNotifyFn != mQueue->localNotify) {
    PW_LOG_ERROR(
        "ConsumerBase::initialize: Got local notify function for remote queue "
        "or vice versa");
    return pw::Status::FailedPrecondition();
  }
  auto consumerFlags = chre::AtomicUint32Ref(mDesc->sinkFlags).load();
  mCurrentFlags = chre::AtomicUint32Ref(mDesc->sourceFlags).load();
  auto flagValue = getAndCheckProducerFlags(mCurrentFlags, consumerFlags);
  if (!(flagValue == ProducerFlags::kPendingInit ||
        flagValue == ProducerFlags::kOverwrite ||
        flagValue == ProducerFlags::kBlocking)) {
    PW_LOG_ERROR("ConsumerBase::initialize: descriptor state not one of "
                 "{kPendingInit, kOverwrite, kBlocking}");
    return flagValue == ProducerFlags::kFinished ||
                   flagValue == ProducerFlags::kDisconnected
               ? pw::Status::Aborted()
               : pw::Status::FailedPrecondition();
  }
  std::memcpy(&mDesc->id, &idOrNotifyFn, sizeof(IdOrNotifyFn));
  // mBlockListEpoch must be set before capacity() is called when setting a
  // default mOverwriteResetOffset. This is subsequently used if the consumer
  // has already been overwritten.
  mBlockListEpoch = mDesc->initialBlockListEpoch;
  mOverwriteResetOffset = overwriteResetOffset.value_or(capacity() / 2);
  mHeadBlock = fromOffset<BlockHeader>(
      mRegion, mDesc->initialHeadBlockOffsetBytes, kBlockLayout);
  if (flagValue == ProducerFlags::kOverwrite) {
    PW_TRY(handleOverwrite());
  }
  clearFlags();
  return checkState();
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
    PW_LOG_ERROR("ConsumerBase::checkState: instance is disabled");
    return pw::Status::FailedPrecondition();
  }
  mCurrentFlags = chre::AtomicUint32Ref(mDesc->sourceFlags).load();
  auto consumerFlags = chre::AtomicUint32Ref(mDesc->sinkFlags).load();
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
      PW_LOG_ERROR("ConsumerBase::checkState: producer gone or disconnected");
      return pw::Status::Aborted();
    case ProducerFlags::kOverwrite: {
      PW_LOG_INFO("ConsumerBase::checkState: read position overwritten");
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
      mBlockListEpoch = chre::AtomicUint32Ref(mQueue->blockListEpoch).load();
      return pw::OkStatus();
    default:  // Unexpected flag value. Clear it.
      PW_LOG_WARN("ConsumerBase::checkState: unexpected flag value %" PRIu16,
                  static_cast<uint16_t>(flagValue));
      clearFlags();
      return pw::OkStatus();
  }
}

pw::Result<pw::ConstByteSpan> ConsumerBase::peek(size_t count) {
  PW_TRY(checkState());
  PW_TRY(checkAvailable(count));
  if (!mPeeked) {
    mCurrBlock = mHeadBlock;
    mCurrBlockIndex = (chre::AtomicUint32Ref(mDesc->readIndex).load() +
                       mDesc->indexCorrection) %
                      kBlockCapacity;
  }
  mPeeked += count;
  const auto *data = blockData(mCurrBlock, kDataOffset) + mCurrBlockIndex;
  uint32_t advance = count;
  if (advanceContiguous(chre::AtomicUint32Ref(mCurrBlock->baseIndex).load(),
                        chre::AtomicUint32Ref(mCurrBlock->skipIndex).load(),
                        kBlockCapacity, mCurrBlockIndex, advance)) {
    mCurrBlock = fromOffset<BlockHeader>(
        mRegion, chre::AtomicUint32Ref(mCurrBlock->nextBlockOffsetBytes).load(),
        kBlockLayout);
    mCurrBlockIndex = chre::AtomicUint32Ref(mCurrBlock->baseIndex).load();
  }
  PW_TRY(checkState());
  return pw::ConstByteSpan(data, count);
}

pw::Status ConsumerBase::releaseNoNotify(size_t count) {
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

pw::Status ConsumerBase::popNoNotify(pw::ByteSpan data) {
  if (mPeeked) {  // pop() is not allowed when there is un-release()d data.
    PW_LOG_ERROR("ConsumerBase::pop: Can't pop with unreleased data");
    return pw::Status::FailedPrecondition();
  }
  PW_TRY(checkAvailable(data.size()));
  advanceReadIndex(data.size(), data);
  return pw::OkStatus();
}

pw::Status ConsumerBase::resync(size_t offset) {
  PW_TRY(checkState());
  PW_TRY(updateAvailable());
  mPeeked = 0;  // Reset the current block/index to the new head.
  if (offset > mAvailable) {
    PW_LOG_WARN(
        "ConsumerBase::resync: offset %zu exceeds available data %zu. Leaving "
        "read index unmodified",
        offset, mAvailable);
  } else if (offset < mAvailable) {
    advanceReadIndex(mAvailable - offset, /*buf=*/std::nullopt);
    maybeNotifyOnRead();
    mAvailable -= offset;
  }
  return pw::OkStatus();
}

pw::Result<size_t> ConsumerBase::size() {
  PW_TRY(checkState());
  PW_TRY(updateAvailable());
  return mAvailable;
}

pw::Result<bool> ConsumerBase::isOverwritable() {
  PW_TRY(checkState());
  return mDesc->isOverwritable;
}

pw::Status ConsumerBase::checkAvailable(size_t count) {
  PW_TRY(checkState());
  if (count > capacity()) {
    // If the epoch has changed, check against the updated capacity.
    mBlockListEpoch = chre::AtomicUint32Ref(mQueue->blockListEpoch).load();
    if (count > capacity()) {
      PW_LOG_ERROR(
          "ConsumerBase::checkAvailable: count %zu exceeds capacity %zu", count,
          capacity());
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

size_t ConsumerBase::advanceReadIndex(size_t count,
                                      std::optional<pw::ByteSpan> buf,
                                      bool stopOnNextBlock) {
  auto pending = count;
  auto readIndex = chre::AtomicUint32Ref(mDesc->readIndex).load();
  uint32_t blockIndex = (readIndex + mDesc->indexCorrection) % kBlockCapacity;
  auto correction = mDesc->indexCorrection;
  // Loop through the contiguous regions, copying out data and tracking index
  // corrections as the read index moves between blocks.
  while (pending > 0) {
    uint32_t advance = pending;
    const auto *dataPtr = blockData(mHeadBlock, kDataOffset) + blockIndex;
    bool toNextBlock =
        advanceContiguous(chre::AtomicUint32Ref(mHeadBlock->baseIndex).load(),
                          chre::AtomicUint32Ref(mHeadBlock->skipIndex).load(),
                          kBlockCapacity, blockIndex, advance);
    if (buf) {
      std::memcpy(buf->data(), dataPtr, advance);
      buf = buf->subspan(advance);
    }
    pending -= advance;
    if (toNextBlock) {
      auto *nextBlock = fromOffset<BlockHeader>(
          mRegion,
          chre::AtomicUint32Ref(mHeadBlock->nextBlockOffsetBytes).load(),
          kBlockLayout);
      correction +=
          indexCorrectionIncrement(mHeadBlock, nextBlock, kBlockCapacity);
      mHeadBlock = nextBlock;
      blockIndex = chre::AtomicUint32Ref(mHeadBlock->baseIndex).load();
      if (stopOnNextBlock) {
        break;
      }
    }
  }
  chre::AtomicUint32Ref(mDesc->readIndex).store(readIndex + count - pending);
  mDesc->indexCorrection = correction;
  return count - pending;
}

void ConsumerBase::maybeNotifyOnRead() {
  if (getProducerFlags(mCurrentFlags) == ProducerFlags::kBlocking) {
    notifyProducer();
    clearFlags();
  }
}

pw::Status ConsumerBase::handleOverwrite() {
  // If the epoch has changed, just sync to the producer.
  if (chre::AtomicUint32Ref(mQueue->blockListEpoch).load() != mBlockListEpoch) {
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
  PW_TRY(overwriteFastForward(offset));
  // If the epoch changed since we attempted to fast forward, the fast forward
  // is invalidated. Sync to the producer.
  if (chre::AtomicUint32Ref(mQueue->blockListEpoch).load() != mBlockListEpoch) {
    return syncToProducer();
  }
  return pw::OkStatus();
}

pw::Status ConsumerBase::updateAvailable() {
  PW_TRY_ASSIGN(auto *producerDesc, getProducerDesc());
  mAvailable =
      writeReadDiff(chre::AtomicUint32Ref(producerDesc->writeIndex).load(),
                    chre::AtomicUint32Ref(mDesc->readIndex).load());
  return pw::OkStatus();
}

pw::Status ConsumerBase::overwriteFastForward(size_t offset) {
  advanceReadIndex(mAvailable - offset, /*buf=*/std::nullopt);
  mAvailable = offset;
  return pw::OkStatus();
}

pw::Status ConsumerBase::syncToProducer() {
  PW_TRY_ASSIGN(auto *producerDesc, getProducerDesc());
  auto readIndex = chre::AtomicUint32Ref(producerDesc->writeIndex).load();
  chre::AtomicUint32Ref(mDesc->readIndex).store(readIndex);
  mDesc->indexCorrection = producerDesc->indexCorrection;
  mHeadBlock = fromOffset<BlockHeader>(
      mRegion, producerDesc->tailBlockOffsetBytes, kBlockLayout);
  mBlockListEpoch = chre::AtomicUint32Ref(mQueue->blockListEpoch).load();
  return pw::OkStatus();
}

pw::Result<ProducerDesc *> ConsumerBase::getProducerDesc() {
  auto *producerDesc = fromOffset<ProducerDesc>(
      mRegion, chre::AtomicUint32Ref(mQueue->sourceMetadataOffsetBytes).load());
  if (!producerDesc) {
    disableAndNotify();
    PW_LOG_ERROR("ConsumerBase::getProducerDesc: Producer gone");
    return pw::Status::Aborted();
  }
  return producerDesc;
}

size_t ConsumerBase::capacity() {
  return mQueue->blockCapacityBytes * blockCountForEpoch(mBlockListEpoch);
}

void ConsumerBase::disableAndNotify() {
  mActive = false;
  chre::AtomicUint32Ref(mDesc->sinkFlags)
      .store(static_cast<uint32_t>(ConsumerFlags::kFinished));
  notifyProducer();
}

void ConsumerBase::notifyProducer() {
  notify(*reinterpret_cast<IdOrNotifyFn *>(&mQueue->sourceId), mRemoteNotifyFn);
}

void ConsumerBase::clearFlags() {
  auto counter = getFlagsCounter(mCurrentFlags);
  chre::AtomicUint32Ref(mDesc->sinkFlags)
      .store(static_cast<uint32_t>(ConsumerFlags::kFlagsCleared) | counter);
  mCurrentFlags = static_cast<uint32_t>(ProducerFlags::kNone) | counter;
}

}  // namespace internal

void DataNotifier::onWrite(internal::ProducerBase &producer) {
  // Only notify consumers that are in a good state to read (i.e. either no
  // flags or ProducerFlags::kBlocking).
  uint16_t excludeMask =
      ~(static_cast<uint16_t>(internal::ProducerFlags::kBlocking));
  uint32_t tail = chre::AtomicUint32Ref(producer.mDesc->writeIndex).load();
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
  if (producer.mQueue->queue.elementConfig.getTag() ==
      internal::ElementConfig::Tag::fixedSize) {
    threshold *= producer.mQueue->queue.elementConfig
                     .get<internal::ElementConfig::Tag::fixedSize>()
                     .elementSizeBytes;
  }
  if (internal::writeReadDiff(
          writeIndex, chre::AtomicUint32Ref(consumer.readIndex).load()) >=
      threshold) {
    producer.notifyConsumer(consumer);
  }
}

pw::Result<VariableDataProducer> VariableDataProducer::createLocal(
    AllocatorRegion region, size_t blockCapacity, size_t maxBlockCount,
    size_t minBlockCount, DataNotifier &dataNotifier,
    LocalNotifyArgs notifyArgs, MemoryAccess *memAccess) {
  if (!notifyArgs.fn) {
    PW_LOG_ERROR(
        "VariableDataProducer::createLocal: Invalid notifyArgs or queue");
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(internal::QueuePrivate * queuePtr,
                Base::initQueue(region, blockCapacity, /*elementSize=*/0,
                                /*elementAlignment=*/1,
                                {.localNotify = notifyArgs}, /*local=*/true));
  VariableDataProducer producer(region, *queuePtr, blockCapacity, maxBlockCount,
                                minBlockCount, dataNotifier,
                                /*remoteNotifyFn=*/{}, memAccess);
  PW_TRY(producer.initialize(/*variableData=*/true));
  return producer;
}

pw::Result<VariableDataProducer> VariableDataProducer::createRemote(
    AllocatorRegion region, size_t blockCapacity, size_t maxBlockCount,
    size_t minBlockCount, DataNotifier &dataNotifier,
    RemoteNotifyArgs notifyArgs, MemoryAccess *memAccess) {
  if (!notifyArgs.fn) {
    PW_LOG_ERROR(
        "VariableDataProducer::createRemote: Invalid notifyArgs or queue");
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(internal::QueuePrivate * queuePtr,
                Base::initQueue(region, blockCapacity, /*elementSize=*/0,
                                /*elementAlignment=*/1,
                                {.remoteId = notifyArgs.id}, /*local=*/false));
  VariableDataProducer producer(region, *queuePtr, blockCapacity, maxBlockCount,
                                minBlockCount, dataNotifier,
                                std::move(notifyArgs.fn), memAccess);
  PW_TRY(producer.initialize(/*variableData=*/true));
  return producer;
}

VariableDataProducer::VariableDataProducer(
    const AllocatorRegion &region, internal::QueuePrivate &queue,
    size_t blockCapacity, size_t maxBlockCount, size_t minBlockCount,
    DataNotifier &dataNotifier, RemoteNotifyFn remoteNotifyFn,
    MemoryAccess *memAccess)
    : ProducerBase(region, queue,
                   internal::variableDataBlockLayout(blockCapacity),
                   blockCapacity, offsetof(internal::VariableDataBlock, data),
                   maxBlockCount, minBlockCount, dataNotifier,
                   std::move(remoteNotifyFn), memAccess) {}

pw::Result<pw::ByteSpan> VariableDataProducer::reserve(size_t count) {
  if (mCurrentHdrPtr) {
    PW_TRY_ASSIGN(auto reservation, Base::reserve(count));
    mCurrentHdrPtr->sizeBytes += count;
    return reservation;
  }
  // Reserve space for the element size and data.
  PW_TRY_ASSIGN(auto reservation,
                Base::reserve(count + sizeof(internal::VariableElementHeader)));
  mCurrentHdrPtr =
      reinterpret_cast<internal::VariableElementHeader *>(reservation.data());
  mCurrentHdrPtr->sizeBytes = count;
  reservation = reservation.subspan(sizeof(internal::VariableElementHeader));
  // If the reservation was at the end of a contiguous chunk, retrieve the next
  // contiguous chunk. This must succeed.
  if (reservation.empty()) {
    reservation = Base::reserve(count).value();
  }
  return reservation;
}

pw::Status VariableDataProducer::truncate(size_t size) {
  PW_TRY(Base::truncate(size + sizeof(internal::VariableElementHeader)));
  // Store the new size. The memory address of the element size has not changed.
  mCurrentHdrPtr->sizeBytes = size;
  return pw::OkStatus();
}

pw::Status VariableDataProducer::commit() {
  if (!mCurrentHdrPtr) {
    PW_LOG_ERROR("VariableDataProducer::commit: No active reservation");
    return pw::Status::FailedPrecondition();
  }
  mCurrentHdrPtr = nullptr;
  updateFirstElementIndex();  // Enable consumers to seek to an element.
  // Commit the entire reservation. Notifies consumers as required. Round up the
  // reservation size to the header alignment. This should always be possible as
  // the reservation is header aligned and the block capacity is a multiple of
  // the header size.
  mReserved =
      internal::alignTo(mReserved, alignof(internal::VariableElementHeader));
  PW_TRY(Base::commit(mReserved));
  return pw::OkStatus();
}

pw::Status VariableDataProducer::push(pw::ConstByteSpan element) {
  if (mReserved) {
    PW_LOG_ERROR(
        "VariableDataProducer::push: Can't push with active reservation");
    return pw::Status::FailedPrecondition();
  }
  // Calculate the total size of the element and header, rounding up to align
  // the next header.
  const auto kTotalSize = internal::alignTo(
      element.size() + sizeof(internal::VariableElementHeader),
      alignof(internal::VariableElementHeader));
  PW_TRY(checkAvailable(kTotalSize, /*allOrNothing=*/true));
  updateFirstElementIndex();  // Enable consumers to seek to an element.
  internal::VariableElementHeader hdr{.sizeBytes =
                                          static_cast<int32_t>(element.size())};
  advanceWriteIndex(sizeof(hdr), pw::as_bytes(pw::span(&hdr, 1)));
  advanceWriteIndex(hdr.sizeBytes, element);
  advanceWriteIndex(kTotalSize - element.size() - sizeof(hdr),
                    /*data=*/std::nullopt);
  // Notify consumers as required.
  mDataNotifier->onWrite(*this);
  return pw::OkStatus();
}

void VariableDataProducer::updateFirstElementIndex() {
  auto *tailBlock = internal::fromOffset<internal::VariableDataBlock>(
      mRegion, mDesc->tailBlockOffsetBytes, kBlockLayout);
  if (tailBlock->header.firstElementIndex == kBlockCapacity) {
    // Only set the first element index if this is the first variable size
    // element to be written into this block (on this pass through the block).
    tailBlock->header.firstElementIndex =
        (chre::AtomicUint32Ref(mDesc->writeIndex).load() +
         mDesc->indexCorrection) %
        kBlockCapacity;
  }
}

void VariableDataProducer::enterNextBlock(internal::BlockHeader *&block,
                                          uint32_t *correction, uint32_t &index,
                                          bool convertSkipToBase) {
  Base::enterNextBlock(block, correction, index, convertSkipToBase);
  auto *varDataBlock = reinterpret_cast<internal::VariableDataBlock *>(block);
  varDataBlock->header.firstElementIndex = kBlockCapacity;
}

pw::Result<VariableDataConsumer> VariableDataConsumer::createLocal(
    Region region, uint32_t queueOffset, uint32_t descOffset,
    LocalNotifyArgs notifyArgs, MemoryAccess *memAccess,
    std::optional<size_t> overwriteResetOffset) {
  if (!notifyArgs.fn) {
    PW_LOG_ERROR("Received null notify function");
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(auto queueAndDesc, checkArgs(region, /*descRegion=*/nullptr,
                                             queueOffset, descOffset));
  if (queueAndDesc.first->elementConfig.getTag() ==
      internal::ElementConfig::Tag::fixedSize) {
    PW_LOG_ERROR("VariableDataConsumer::createLocal: Fixed size queue");
    return pw::Status::FailedPrecondition();
  } else if (queueAndDesc.first->elementConfig.getTag() !=
             internal::ElementConfig::Tag::variableSize) {
    PW_LOG_ERROR(
        "VariableDataConsumer::createLocal: Aligned variable-size data not "
        "supported");
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
    PW_LOG_ERROR("Received null notify function");
    return pw::Status::InvalidArgument();
  }
  auto *descRegionPtr = descRegion ? &*descRegion : nullptr;
  PW_TRY_ASSIGN(auto queueAndDesc,
                checkArgs(region, descRegionPtr, queueOffset, descOffset));
  if (queueAndDesc.first->elementConfig.getTag() ==
      internal::ElementConfig::Tag::fixedSize) {
    PW_LOG_ERROR("VariableDataConsumer::createRemote: Fixed size queue");
    return pw::Status::FailedPrecondition();
  } else if (queueAndDesc.first->elementConfig.getTag() !=
             internal::ElementConfig::Tag::variableSize) {
    PW_LOG_ERROR(
        "VariableDataConsumer::createRemote: Aligned variable-size data not "
        "supported");
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
  if (mCurrentHdr) {
    return mCurrentHdr->sizeBytes;
  }
  internal::VariableElementHeader hdr;
  PW_TRY(Base::popNoNotify(pw::as_writable_bytes(pw::span(&hdr, 1))));
  mCurrentHdr = hdr;
  return mCurrentHdr->sizeBytes;
}

pw::Result<pw::ConstByteSpan> VariableDataConsumer::peek() {
  if (!mCurrentHdr) {
    PW_TRY(getHeadSize());
  }
  // Peek from the remaining bytes of the current head element.
  return Base::peek(mCurrentHdr->sizeBytes - mPeeked);
}

pw::Status VariableDataConsumer::releaseNoNotify() {
  // If an element hasn't yet been peeked, get the size of the next element.
  if (!mCurrentHdr) {
    PW_TRY(getHeadSize());
  }
  // Get the total size including alignment adjustment.
  auto totalSize = internal::alignTo(mCurrentHdr->sizeBytes,
                                     alignof(internal::VariableElementHeader));
  PW_TRY(Base::releaseNoNotify(totalSize));
  mCurrentHdr.reset();
  return pw::OkStatus();
}

pw::Status VariableDataConsumer::pop(pw::ByteSpan &buffer) {
  if (!mCurrentHdr) {
    PW_TRY(getHeadSize());
  }
  buffer = buffer.subspan(0, mCurrentHdr->sizeBytes);
  PW_TRY(Base::popNoNotify(buffer));
  // Move the read index to the start of the next element, notifying the
  // producer if required.
  constexpr size_t kAlignment = alignof(internal::VariableElementHeader);
  auto offset = mCurrentHdr->sizeBytes & (kAlignment - 1);
  auto adjustment = offset ? kAlignment - offset : 0;
  PW_TRY(Base::release(adjustment));
  mCurrentHdr.reset();
  return pw::OkStatus();
}

pw::Status VariableDataConsumer::resync(size_t offset) {
  mCurrentHdr.reset();
  PW_TRY(checkState());
  PW_TRY(updateAvailable());
  mCurrentHdr.reset();
  if (offset > mAvailable) {
    PW_LOG_WARN(
        "VariableDataConsumer::resync: offset %zu exceeds available data %zu. "
        "Leaving read index unmodified",
        offset, mAvailable);
  }
  // Fast-forward through elements until the offset is reached.
  while (mAvailable > offset) {
    PW_TRY(releaseNoNotify());
    maybeNotifyOnRead();
  }
  return pw::OkStatus();
}

pw::Status VariableDataConsumer::overwriteFastForward(size_t offset) {
  // Fast-forward through blocks until we reach an identifiable element boundary
  // past where the producer has overwritten.
  while (mAvailable > offset) {
    auto advance = advanceReadIndex(mAvailable, /*buf=*/std::nullopt,
                                    /*stopOnNextBlock=*/true);
    mAvailable -= advance;
    uint32_t firstElementIndex =
        reinterpret_cast<internal::VariableDataBlock *>(mHeadBlock)
            ->header.firstElementIndex;
    if (firstElementIndex != kBlockCapacity && mAvailable < capacity()) {
      auto diff = internal::ringDiff(
          firstElementIndex,
          chre::AtomicUint32Ref(mHeadBlock->baseIndex).load(), kBlockCapacity);
      mAvailable -= advanceReadIndex(diff, /*buf=*/std::nullopt);
      break;
    }
  }
  // If the epoch changed since we attempted to fast forward, the fast forward
  // is invalidated. Sync to the producer.
  if (chre::AtomicUint32Ref(mQueue->blockListEpoch).load() != mBlockListEpoch) {
    return syncToProducer();
  }
  clearFlags();
  // We're in a good state now. Just resync().
  return resync(offset);
}

pw::Result<UntypedProducer> UntypedProducer::createLocal(
    AllocatorRegion region, size_t blockCapacity, size_t elementSize,
    size_t elementAlignment, size_t maxBlockCount, size_t minBlockCount,
    DataNotifier &dataNotifier, LocalNotifyArgs notifyArgs,
    MemoryAccess *memAccess) {
  if (notifyArgs.fn == nullptr) {
    PW_LOG_ERROR(
        "UntypedProducer::createLocal: Received null notify function or "
        "metadata");
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(
      internal::QueuePrivate * queuePtr,
      ProducerBase::initQueue(region, blockCapacity * elementSize, elementSize,
                              elementAlignment, {.localNotify = notifyArgs},
                              /*local=*/true));
  UntypedProducer producer(region, *queuePtr, blockCapacity, elementSize,
                           elementAlignment, maxBlockCount, minBlockCount,
                           dataNotifier, /*remoteNotifyFn=*/{}, memAccess);
  PW_TRY(producer.initialize(/*variableData=*/false));
  return producer;
}

pw::Result<UntypedProducer> UntypedProducer::createRemote(
    AllocatorRegion region, size_t blockCapacity, size_t elementSize,
    size_t elementAlignment, size_t maxBlockCount, size_t minBlockCount,
    DataNotifier &dataNotifier, RemoteNotifyArgs notifyArgs,
    MemoryAccess *memAccess) {
  if (notifyArgs.fn == nullptr) {
    PW_LOG_ERROR(
        "UntypedProducer::createRemote: Received null notify function or "
        "metadata");
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(
      internal::QueuePrivate * queuePtr,
      ProducerBase::initQueue(region, blockCapacity * elementSize, elementSize,
                              elementAlignment, {.remoteId = notifyArgs.id},
                              /*local=*/false));
  UntypedProducer producer(region, *queuePtr, blockCapacity, elementSize,
                           elementAlignment, maxBlockCount, minBlockCount,
                           dataNotifier, std::move(notifyArgs.fn), memAccess);
  PW_TRY(producer.initialize(/*variableData=*/false));
  return producer;
}

UntypedProducer::UntypedProducer(
    const AllocatorRegion &region, internal::QueuePrivate &queue,
    size_t blockCapacity, size_t elementSize, size_t elementAlignment,
    size_t maxBlockCount, size_t minBlockCount, DataNotifier &dataNotifier,
    RemoteNotifyFn remoteNotifyFn, MemoryAccess *memAccess)
    : ProducerBase(region, queue,
                   internal::getBlockLayout(blockCapacity, elementSize,
                                            elementAlignment),
                   blockCapacity * elementSize,
                   internal::getDataOffset(elementAlignment), maxBlockCount,
                   minBlockCount, dataNotifier, std::move(remoteNotifyFn),
                   memAccess),
      mElementSize(elementSize),
      mElementAlignment(elementAlignment) {}

pw::Result<UntypedConsumer> UntypedConsumer::createLocal(
    Region region, uint32_t queueOffset, uint32_t descOffset,
    LocalNotifyArgs notifyArgs, MemoryAccess *memAccess,
    std::optional<size_t> overwriteResetOffset) {
  if (!notifyArgs.fn) {
    PW_LOG_ERROR("UntypedConsumer::createLocal: Received null notify function");
    return pw::Status::InvalidArgument();
  }
  PW_TRY_ASSIGN(auto queueAndDesc, checkArgs(region, /*descRegion=*/nullptr,
                                             queueOffset, descOffset));
  if (queueAndDesc.first->elementConfig.getTag() !=
      internal::ElementConfig::Tag::fixedSize) {
    PW_LOG_ERROR(
        "UntypedConsumer::createLocal: Unexpected queue config. Must be fixed "
        "size.");
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
    PW_LOG_ERROR(
        "UntypedConsumer::createRemote: Received null notify function");
    return pw::Status::InvalidArgument();
  }
  auto *descRegionPtr = descRegion ? &*descRegion : nullptr;
  PW_TRY_ASSIGN(auto queueAndDesc,
                checkArgs(region, descRegionPtr, queueOffset, descOffset));
  if (queueAndDesc.first->elementConfig.getTag() !=
      internal::ElementConfig::Tag::fixedSize) {
    PW_LOG_ERROR(
        "UntypedConsumer::createRemote: Unexpected queue config. Must be fixed "
        "size.");
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
    : ConsumerBase(region, queue, desc, internal::getBlockBaseLayout(queue),
                   internal::getDataOffset(queue), std::move(remoteNotifyFn),
                   memAccess),
      mElementSize(
          queue.elementConfig.get<internal::ElementConfig::Tag::fixedSize>()
              .elementSizeBytes),
      mElementAlignment(
          queue.elementConfig.get<internal::ElementConfig::Tag::fixedSize>()
              .elementAlignmentBytes) {}

}  // namespace android::contexthub::data_flow
