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

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "data_flow/queue_defs.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"
#include "pw_bytes/span.h"
#include "pw_containers/intrusive_list.h"
#include "pw_function/function.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {

// Forward declarations.
class ConsumerManager;
class ConsumerPolicyBuilder;
class DataNotifier;
class MemoryAccess;

namespace internal {

// TODO(b/444261568): Replace std::atomic<uint32_t> with chre::AtomicUint32 to
// allow for platforms that don't have <atomic> support. This will require a way
// to report something like is_always_lock_free for a given platform's
// implementation.
static_assert(std::atomic<uint32_t>::is_always_lock_free);

//! Analog to nullptr for offsets in shared memory.
constexpr uint32_t kOffsetInvalid = UINT32_MAX;

/** Consumer notification and overwrite policy. */
union ConsumerPolicy {
  struct {
    uint8_t policy;   // NotificationPolicy | OverwritePolicy.
    uint8_t data[3];  // Interpreted based on NotificationPolicy.
  };
  uint32_t rawValue;
};
static_assert(sizeof(ConsumerPolicy) == 4);

//! Endpoint id for remote notifications or local callback.
union alignas(8) IdOrNotifyFn {
  LocalNotifyArgs localNotify;
  std::array<std::byte, 16> remoteId;
} __attribute__((packed));
static_assert(sizeof(IdOrNotifyFn) == 16);

/** Producer metadata in shared memory. */
struct alignas(8) ProducerDesc {
  // Current write index. Updated by the producer.
  std::atomic<uint32_t> writeIndex;
  // Correction to index for calculating index within Block::data.
  uint32_t indexCorrection;
  // Offset of the block containing the current write index in shared memory.
  uint32_t tailBlockOffset;
  // Reserved for future use.
  uint8_t reserved[12];
} __attribute__((packed));
static_assert(sizeof(ProducerDesc) == 24);

/**
 * Flags used by the Producer to indicate exceptional state.
 *
 * Flags are mutually exclusive and the latest value takes precedence over
 * previous ones. The values are a bitmask to make it easy to compare against a
 * set of values.
 */
enum class ProducerFlags : uint16_t {
  kNone = 0x0,            // No flags set. Consumer does not need to ack this.
  kPendingInit = 0x1,     // Consumer state allocated, pending Consumer().
  kBlocking = 0x1 << 1,   // Producer cannot write until this Consumer reads.
  kOverwrite = 0x1 << 2,  // Producer overwrote this Consumer.
  kFinished = 0x1 << 3,   // Producer torn down.
  kDisconnected = 0x1 << 4,  // The consumer endpoint disconnected.
};

/** Flags used by the Consumer to acknowledge ProducerFlags or tear down. */
enum class ConsumerFlags : uint16_t {
  kFlagsCleared = 0,  // Producer flags have been handled.
  kFinished,          // Consumer torn down and ready for deallocation.
};

/**
 * Queue implementation version. Uses the same numbering scheme as the CHRE API.
 *
 * Minor version changes require that the following are maintained:
 * - The form and meaning of any existing struct fields
 * - The size and alignment of ProducerDesc and BlockHeader
 *
 * Major version changes will use new struct definitions, however, the first
 * field of Queue and ConsumerDesc must be a Version.
 */
struct Version {
  uint8_t major;
  uint8_t minor;
  uint16_t patch;
};
static_assert(sizeof(Version) == 4);

/** Consumer metadata in shared memory. */
struct ConsumerDesc {
  // Consumer version.
  Version version;
  // Current read index. Updated by the consumer.
  std::atomic<uint32_t> readIndex;
  // Correction to index for calculating index within Block::data.
  uint32_t indexCorrection;
  // The following two fields are a way to emulate a single flag set by
  // the producer and atomically read and cleared by the consumer. The producer
  // only writes to producerFlags while the consumer only writes to
  // consumerFlags. The producer maintains a local counter whose value is
  // incremented on every write and included in producerFlags. The consumer
  // copies the latest read counter value into consumerFlags to indicate that it
  // has handled the producer flags up to that counter value, effectively
  // clearing it.
  // { 0-15: ProducerFlags | 16-31: counter incremented on write }
  std::atomic<uint32_t> producerFlags;
  // { 0-15: ConsumerFlags | 16-31: latest value of producerFlags counter }
  std::atomic<uint32_t> consumerFlags;
  // Consumer policy.
  std::atomic<uint32_t> policy;
  // Id for remote notification or local callback.
  IdOrNotifyFn idOrNotifyFn;
} __attribute__((packed));
static_assert(sizeof(ConsumerDesc) == 40);

/** Queue metadata in shared memory. */
struct Queue {
  // Producer version.
  Version version;
  // Offset of the ProducerDesc in shared memory. Updated by the producer.
  std::atomic<uint32_t> producerOffset;
  // Producer id for remote notification or local callback.
  IdOrNotifyFn idOrNotifyFn;
  // Captures the current epoch of the block list and the block count. Updated
  // by the producer.
  // Format: { 0-15: epoch counter | 16-31: block count }
  std::atomic<uint32_t> blockListEpoch;
  // Block capacity (in elements).
  uint32_t blockCapacity;
  // Element alignment. Used to check Consumer compatibility. <0 indicates that
  // this is a variable data queue.
  int16_t elementAlignment;
  // True iff notifications are done using IdOrNotifyFn.fn
  uint8_t localNotify;
  // Padding bytes. Reserved for future use.
  uint8_t padding[5];
} __attribute__((packed));
static_assert(sizeof(Queue) == 40);

/** Header that precedes the aligned array of elements. */
struct alignas(8) BlockHeader {
  // Storage for the ProducerDesc in the current tail block.
  ProducerDesc producerDesc;
  // Offset of the next block in shared memory. May refer back to this block.
  std::atomic<uint32_t> nextBlockOffset;  // Updated by the producer.
  // Base index for reading/writing this block. Initialized to 0.
  std::atomic<uint32_t> baseIndex;  // Updated by the producer.
  // Index at which to jump to the next block. Initialized to kCapacity.
  std::atomic<uint32_t> skipIndex;  // Updated by the producer.
  // Reserved for future use.
  uint8_t reserved[12];
} __attribute__((packed));
static_assert(sizeof(BlockHeader) == 48);

/** Block of element storage. */
template <typename ElementType>
struct Block {
  BlockHeader header;
  ElementType data[];
};

/** @return Layout for allocating Blocks using pw::Allocator. */
template <typename ElementType>
constexpr pw::allocator::Layout blockLayout(size_t blockCapacity) {
  return pw::allocator::Layout(
      offsetof(Block<ElementType>, data) + blockCapacity * sizeof(ElementType),
      alignof(Block<ElementType>));
}

/** Header preceding each variable-size element. */
struct alignas(8) VariableDataHeader {
  uint32_t size;         // Element size in bytes.
  uint8_t reserved[12];  // Reserved for future use.
};
static_assert(sizeof(VariableDataHeader) == 16);

/** Block of variable-size element storage. */
struct VariableDataBlock {
  BlockHeader header;
  uint32_t firstElementIndex;  // Initialized to block capacity.
  // Element storage is 4-byte aligned to ensure that element size is always
  // aligned.
  std::byte data alignas(VariableDataHeader)[];
};
static_assert(offsetof(VariableDataBlock, data) == 56);

/** @return Layout for allocating VariableDataBlocks using pw::Allocator. */
constexpr pw::allocator::Layout variableDataBlockLayout(size_t blockCapacity) {
  auto size = (blockCapacity + 0x3) & ~0x3;  // Round up to 4-byte aligned size.
  return pw::allocator::Layout(offsetof(VariableDataBlock, data) + size,
                               alignof(VariableDataBlock));
}

/** Base class for item tracked in the consumer list for a queue. */
struct ConsumerListNode
    : public pw::containers::future::IntrusiveList<ConsumerListNode>::Item {};

/** Node for tracking a consumer descriptor in multiple containers. */
struct ConsumerNode : public ConsumerListNode {
  AllocatorRegion region;  // The region the descriptor was allocated from.
  ConsumerDesc *desc;      // The descriptor in shared memory.

  ConsumerNode(const AllocatorRegion &_region, ConsumerDesc *_desc)
      : region(_region), desc(_desc) {}
};

/** Queue shared metadata and producer data that is not part of the ABI. */
struct QueuePrivate : public Queue {
  pw::containers::future::IntrusiveList<ConsumerNode> consumerList;
};

/** Base class for Producers of any ElementType. */
class ProducerBase {
 public:
  // Move-only.
  ProducerBase(const ProducerBase &) = delete;
  ProducerBase &operator=(const ProducerBase &) = delete;
  ProducerBase(ProducerBase &&other) {
    *this = std::move(other);
  }
  ProducerBase &operator=(ProducerBase &&other) {
    if (&other != this) {
      if (other.mState != State::kMovedFrom) {
        mRegion = other.mRegion;
        mRemoteNotifyFn = std::move(other.mRemoteNotifyFn);
        mQueue = other.mQueue;
        mDataNotifier = other.mDataNotifier;
        mMemAccess = other.mMemAccess;
        kBlockLayout = other.kBlockLayout;
        kDataOffset = other.kDataOffset;
        kBlockCapacity = other.kBlockCapacity;
        mDesc = other.mDesc;
        mCurrBlock = other.mCurrBlock;
        mBlockCount = other.mBlockCount;
        mReserved = other.mReserved;
        mAvailable = other.mAvailable;
        mCurrBlockIndex = other.mCurrBlockIndex;
        mState = other.mState;
      }
      other.mState = State::kMovedFrom;
    }
    return *this;
  }

  /**
   * If required, cleans up any remaining allocations for this queue.
   *
   * Does nothing for moved-from instances. For other instances, invokes stop()
   * if it wasn't already called before cleaning up allocations.
   */
  virtual ~ProducerBase();

  /**
   * Disables this instance and signals to consumers to clean up.
   *
   * Should be called before destroying an active instance if the user wants
   * to wait for all consumers to signal that they are no longer accessing
   * their consumer descriptors before destroying the producer.
   */
  void stop();

  /**
   * Sets the desired maximum block count for this queue.
   *
   * Releases unused blocks if necessary to bring the queue size within the new
   * limit. If more blocks are in use than the new limit, asynchronously
   * releases them.
   *
   * @param count The requested maximum.
   * @param force Iff true, forcibly releases blocks which may be in use at the
   * time.
   * @return pw::OkStatus() on success. If the new maximum is less than the
   * previous minimum, sets the minimum to the new maximum.
   */
  pw::Status setMaxBlockCountTarget(size_t count, bool force = false);

  /** @return The current target maximum block count. */
  size_t getMaxBlockCountTarget() const {
    // TODO(b/448384247): Support dynamic sizing.
    return mBlockCount;
  };

  /**
   * Sets the desired minimum block count.
   *
   * If the current queue size is below the new minimum, attempts to immediately
   * allocate new blocks to satisfy the minimum. If unavailable, asynchronously
   * attempts to allocate the remainder.
   *
   * @param count The requested minimum.
   * @return pw::OkStatus() on success. If the new minimum is greater than the
   * previous maximum, sets the maximum to the new minimum.
   */
  pw::Status setMinBlockCountTarget(size_t count);

  /** @return The current target minimum block count. */
  size_t getMinBlockCountTarget() const {
    // TODO(b/448384247): Support dynamic sizing.
    return mBlockCount;
  };

  /** @return The current block count. */
  size_t getBlockCount() const {
    return mBlockCount;
  };

  /**
   * Reserve up-to-count contiguous bytes for writing if there is space.
   *
   * @param count The number of bytes to reserve.
   * @return If available, a span over the next up-to-count bytes.
   */
  pw::Result<pw::ByteSpan> reserve(size_t count);

  /**
   * Reduces the current reservation to the given size.
   *
   * @param size The new reservation size. Must be <= mReserved.
   * @return pw::Status::FailedPrecondition() if mReserved is 0;
   * pw::Status::OutOfRange() if size > mReserved.
   */
  pw::Status truncate(size_t size);

  /**
   * Release the first count bytes reserved for writing.
   *
   * @param count The number of bytes to release.
   * @return pw::OkStatus() on success.
   */
  pw::Status commit(size_t count);

  /**
   * Push the given data to the queue if space is available.
   *
   * @param elements The elements to push.
   * @param allOrNothing Iff true, this is an all-or-nothing operation.
   * @return The number of bytes pushed. May be less than data.size() if
   * allOrNothing is unset but is always > 0 on success.
   */
  pw::Result<size_t> push(pw::ConstByteSpan data, bool allOrNothing);

  /** Push the given data to the queue if space is available. */

  /** @return true iff the queue is full and at max capacity. */
  bool full() {
    return size() == capacity();
  }

  /**
   * Returns the number of bytes available to the furthest-behind Consumer.
   *
   * @param includeReserved Iff true, includes reserved space in the size.
   * @return the size of the queue in bytes.
   */
  size_t size(bool includeReserved = false);

  /** @return the current queue capacity. */
  size_t capacity() const {
    return kBlockCapacity * mBlockCount;
  }

 protected:
  friend class ::android::contexthub::data_flow::ConsumerManager;
  friend class ::android::contexthub::data_flow::DataNotifier;

  enum class State : uint8_t {
    kActive,
    kMovedFrom,
    kStopped,
  };

  /**
   * Allocates an initial ring of blocks and initializes producer metadata.
   *
   * @param region Shared memory region for the queue.
   * @param queue The queue metadata in shared memory.
   * @param layout Layout for allocating Blocks.
   * @param blockCapacity The capacity of each Block in bytes.
   * @param maxBlockCount The maximum allowed blocks of element storage. Must
   * be >= minBlockCount.
   * @param minBlockCount The minimum required blocks of element storage. Must
   * be > 0.
   * @param idOrNotifyFn The new instance's id for remote notifications or the
   * LocalNotifyFn for notifying it.
   * @return pw::OkStatus() on success.
   */
  static pw::Status initialize(const AllocatorRegion &region,
                               QueuePrivate *queue,
                               pw::allocator::Layout layout,
                               uint32_t blockCapacity, size_t maxBlockCount,
                               size_t minBlockCount, IdOrNotifyFn idOrNotifyFn);

  /**
   * See {@link Producer::createLocal()} for a description of most parameters.
   *
   * @param queue The queue metadata in shared memory.
   * @param blockLayout Layout for allocating Blocks.
   * @param dataOffset The offset of the data from the start of BlockHeader.
   * @param remoteNotifyFn Function for notifying Consumers out-of-band only for
   * remote queues.
   */
  ProducerBase(const AllocatorRegion &region, QueuePrivate &queue,
               pw::allocator::Layout blockLayout, size_t maxBlockCount,
               size_t minBlockCount, uint32_t dataOffset,
               DataNotifier &dataNotifier, RemoteNotifyFn remoteNotifyFn,
               MemoryAccess *memAccess);

  /**
   * Checks whether the queue can accommodate the given amount of data.
   *
   * On success, updates the available space by the returned size.
   *
   * @param count The number of bytes to be push()d or reserve()d.
   * @param allOrNothing Iff true, this is an all-or-nothing operation.
   * @return The number of bytes to push() or reserve() on success. May be less
   * than count if allOrNothing is unset.
   */
  pw::Result<size_t> checkAvailable(size_t count, bool allOrNothing);

  /**
   * Advances the write index, possibly copying data into the queue.
   *
   * This is invoked from either push() or commit(). Only push() provides data.
   * commit() effectively publishes data written to previously reserve()d space,
   * as advancing the write index makes that data accessible to consumers.
   *
   * @param count The number of bytes to advance.
   * @param data [optional] The data to copy. The size is greater than or equal
   * to count.
   */
  void advanceWriteIndex(uint32_t count, std::optional<pw::ConstByteSpan> data);

  /**
   * Advances a block index, possibly copying data into the queue.
   *
   * @param [in,out] block The starting (and ending) block.
   * @param [in,out] index The index within block.
   * @param [in,out] correction [optional] An optional index correction which is
   * updated on each block transtition.
   * @param count The number of bytes to advance.
   * @param data [optional] The data to copy. The size is greater than or equal
   * to count.
   * @param convertSkipToBase Iff true, converts the skip index to a base index
   * in the next block. This is used to ensure that the conversion only happens
   * once per transition in case the producer iterates through the blocks
   * multiple times (e.g. reserve()/commit()).
   */
  void advanceBlockIndexWithData(BlockHeader *&block, uint32_t &index,
                                 uint32_t *correction, uint32_t count,
                                 std::optional<pw::ConstByteSpan> data,
                                 bool convertSkipToBase);

  /**
   * Enters the next block, updating all of the given parameters.
   *
   * @param [in,out] block The current block. Stores the next block pointer.
   * @param [in,out] correction [optional] An optional index correction that is
   * updated on each block transition.
   * @param [out] index Stores the starting index in the next block.
   * @param convertSkipToBase Iff true, converts the skip index to a base index
   * in the next block. This is used to avoid converting more than once on the
   * same block on commit() since it would have been done on reserve().
   */
  virtual void enterNextBlock(BlockHeader *&block, uint32_t *correction,
                              uint32_t &index, bool convertSkipToBase);

  /**
   * Updates the write index.
   *
   * If necessary, initializes a new block and updates the queue metadata to
   * point to the ProducerDesc in the new block.
   *
   * @param tailBlock Pointer to the new tail block. May be the same as the
   * current tail block.
   * @param writeIndex The new write index value.
   * @param correction The new index correction value.
   */
  void updateWriteIndex(BlockHeader *tailBlock, uint32_t writeIndex,
                        uint32_t correction);

  /**
   * Iterates over consumers to recalculate available space.
   *
   * While iterating, checks whether a Consumer has been overwritten or would
   * block the producer, flagging them accordingly. The optional increment is
   * used to determine whether a push()/reserve() would result in these states.
   *
   * Consumers in that are overwritten or would block the producer are excluded
   * from the available space calculations.
   *
   * @param increment The size of a prospective push/reserve operation,
   * otherwise 0.
   */
  void updateAvailable(uint32_t increment = 0);

  /**
   * Sets the given flag on a consumer.
   *
   * @param desc The consumer descriptor.
   * @param current The current desc.producerFlags value.
   * @param flag The flag to set.
   * @param forceNotify If true, notify the consumer regardless of their policy.
   */
  void setConsumerFlag(ConsumerDesc &desc, uint32_t current, ProducerFlags flag,
                       bool forceNotify = false);

  /**
   * Notifies a consumer.
   *
   * @param desc The consumer descriptor.
   */
  void notifyConsumer(ConsumerDesc &desc);

  /**
   * Allocates a new consumer and links it to the list in shared memory.
   *
   * @param region The region from which to allocate the consumer.
   * @return The offset of the consumer descriptor in shared memory. Used to
   * initialize a Consumer instance.
   */
  pw::Result<uint32_t> addConsumer(const AllocatorRegion &region);

  /**
   * Removes the descriptor for the consumer at given offset.
   *
   * @param match A predicate that returns true iff the consumer should be
   * removed.
   * @return pw::OkStatus() on success.
   */
  pw::Status pruneConsumers(
      const pw::Function<bool(pw::ConstByteSpan remoteId)> &match);

  /**
   * Returns the current number of consumers on the queue.
   *
   * This can be used to refresh the consumer list when waiting for consumers to
   * release their descriptors after the producer has been stopped.
   *
   * @return On success, the number of consumers.
   */
  size_t getNumConsumers();

  /**
   * Applies a functor to all consumers of the queue.
   *
   * This is used by the Producer and DataNotifier instances to access Consumer
   * state.
   *
   * fn has the signature:
   * void(ConsumerDesc &desc, uint32_t producerFlags, Args... args).
   *
   * The producerFlags value is provided to avoid an extra atomic operation.
   */
  template <typename Fn, typename... Args>
  void forAllConsumers(uint16_t excludeMask, const Fn &fn, Args... args);

  /**
   * Checks if the Consumer is in a state in the mask.
   *
   * Also unsets flags if the consumer has acked them.
   *
   * @param desc ConsumerDesc.
   * @param producerFlags desc.producerFlags.load().
   * @param consumerFlags desc.consumerFlags.load().
   * @param producerMask The mask of ProducerFlag values to check for.
   * @return true iff the consumer is in the state.
   */
  bool isFlagInMask(internal::ConsumerDesc &desc, uint32_t producerFlags,
                    uint32_t consumerFlags, uint16_t producerMask);

  /**
   * Erases the given consumer node from the queue.
   *
   * @param [in,out] node The consumer node to erase. Updated to the next node.
   */
  void eraseConsumerNode(decltype(QueuePrivate::consumerList)::iterator &node);

  // Members fixed on construction.
  AllocatorRegion mRegion;
  RemoteNotifyFn mRemoteNotifyFn;
  QueuePrivate *mQueue;
  DataNotifier *mDataNotifier;
  MemoryAccess *mMemAccess;
  pw::allocator::Layout kBlockLayout;
  uint32_t kDataOffset;
  uint32_t kBlockCapacity;

  ProducerDesc *mDesc;
  BlockHeader *mCurrBlock;
  size_t mBlockCount;
  size_t mReserved = 0;
  size_t mAvailable = 0;
  uint32_t mCurrBlockIndex = 0;
  State mState = State::kActive;
};

/** Base class for Consumers of any ElementType. */
class ConsumerBase {
 public:
  // Move-only.
  ConsumerBase(const ConsumerBase &) = delete;
  ConsumerBase &operator=(const ConsumerBase &) = delete;
  ConsumerBase(ConsumerBase &&other) {
    *this = std::move(other);
  }
  ConsumerBase &operator=(ConsumerBase &&other) {
    if (&other != this) {
      if (other.mActive) {
        mRegion = other.mRegion;
        mQueue = other.mQueue;
        mDesc = other.mDesc;
        mRemoteNotifyFn = std::move(other.mRemoteNotifyFn);
        mMemAccess = other.mMemAccess;
        mOverwriteResetOffset = other.mOverwriteResetOffset;
        kBlockLayout = other.kBlockLayout;
        kBlockCapacity = other.kBlockCapacity;
        kDataOffset = other.kDataOffset;
        mAvailable = other.mAvailable;
        mPeeked = other.mPeeked;
        mHeadBlock = other.mHeadBlock;
        mCurrBlock = other.mCurrBlock;
        mCurrBlockIndex = other.mCurrBlockIndex;
        mBlockListEpoch = other.mBlockListEpoch;
        mCurrentFlags = other.mCurrentFlags;
        mActive = true;
      }
      other.mActive = false;
    }
    return *this;
  }

  virtual ~ConsumerBase();

  /**
   * Updates the current policy. Notifies the Producer.
   *
   * @param policyBuilder Builder for the new consumer policy.
   * @return pw::OkStatus() on success.
   */
  pw::Status updatePolicy(ConsumerPolicyBuilder &policy);

  /** Disables this instance. Should be called when the Producer crashes. */
  void disable();

  /**
   * Checks the current state of the Consumer.
   *
   * This API can be used to check whether an in-progress peek() operation is
   * still valid, i.e. the Producer hasn't overwritten this Consumer.
   *
   * @return pw::OkStatus() if state is ok. The following errors may be
   * returned:
   * - pw::Status::DataLoss(): The Consumer has been overwritten.
   * - pw::Status::Aborted(): The Producer is gone or this instance is empty.
   */
  pw::Status checkState();

  /**
   * If available, returns a span over the next available contiguous bytes;
   *
   * @param count The number of bytes to peek.
   * @return On success, a span over the next up-to-count contiguous bytes.
   */
  pw::Result<pw::ConstByteSpan> peek(size_t count);

  /**
   * Releases count bytes starting with bytes which have been peek()ed.
   *
   * @param count The number of bytes to release.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status release(size_t count);

  /**
   * If available, pops data.size() bytes into the provided memory.
   *
   * @param elements Span over the memory into which to pop data.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status pop(pw::ByteSpan data);

  /**
   * Syncs the read pointer to the write pointer minus an offset.
   *
   * @param offset The number of recent bytes to preserve. If greater than the
   * current size of the queue, resync() will fail.
   * @return pw::OkStatus() on success.
   */
  pw::Status resync(size_t offset);

  /** @return the number of bytes available to read from the queue. */
  pw::Result<size_t> size();

  /** @return true iff the queue is empty. */
  pw::Result<bool> empty() {
    PW_TRY_ASSIGN(auto res, size());
    return res == 0;
  }

 protected:
  /**
   * Checks arguments before initializing.
   *
   * @param region The shared memory region containing the queue.
   * @param queueOffset The queue metadata in region.
   * @param descOffset The consumer descriptor in region.
   * @return On success, a pair of pointers to the Queue and ConsumerDesc.
   */
  static pw::Result<std::pair<Queue *, ConsumerDesc *>> checkArgs(
      const Region &region, uint32_t queueOffset, uint32_t descOffset);

  /**
   * See {@link Consumer::createDynamic()} for most parameters.
   *
   * @param queue The Queue metadata in shared memory.
   * @param desc The ConsumerDesc in shared memory.
   * @param baseBlockLayout The layout of a block with 0 elements. This is
   * modified using the capacity obtained from queue and stored.
   * @param dataOffset The offset of the data from the start of BlockHeader.
   * @param remoteNotifyFn Function for notifying Consumers out-of-band only for
   * remote queues.
   */
  ConsumerBase(const Region &region, Queue &queue, ConsumerDesc &desc,
               pw::allocator::Layout baseBlockLayout, uint32_t dataOffset,
               RemoteNotifyFn remoteNotifyFn, MemoryAccess *memAccess);

  /**
   * One-time post-construction initialization.
   *
   * @param idOrNotifyFn The new instance's id for remote notifications or the
   * LocalNotifyFn for notifying it.
   * @param policyBuilder Builder for the consumer's policy.
   * @param overwriteResetOffset [optional] When recovering from being
   * overwritten, the offset from the write index to attempt to sync to.
   * @return pw::OkStatus() on success.
   */
  pw::Status initialize(IdOrNotifyFn idOrNotifyFn,
                        ConsumerPolicyBuilder &policyBuilder,
                        std::optional<size_t> overwriteResetOffset);

  /**
   * Checks whether the queue has enough data to read.
   *
   * On success, reduces mAvailable by count.
   *
   * @param count The number of bytes to read.
   * @return pw::OkStatus() on success.
   */
  pw::Status checkAvailable(size_t count);

  /**
   * Advances the read index, possibly copying data out of the queue.
   *
   * @param count The number of bytes to advance.
   * @param buf [optional] The buffer to copy data into. The size is greater
   * than or equal to count.
   */
  void advanceReadIndex(size_t count, std::optional<pw::ByteSpan> buf);

  /**
   * Attempts to restore this instance to a valid state after being overwritten.
   *
   * If the block list epoch has not changed, attempts to fast forward the read
   * index. Otherwise, resyncs state to the producer, minus
   * mOverwriteResetOffset.
   *
   * @return pw::OkStatus() on success.
   */
  pw::Status handleOverwrite();

  /**
   * Updates mAvailable based on the current state in shared memory.
   *
   * @return pw::OkStatus() on success. See {@link #ConsumerBase::checkState()}
   * for error conditions.
   */
  pw::Status updateAvailable();

  /** Syncs to the producer. */
  pw::Status syncToProducer();

  /** @return On success, the current producer descriptor. */
  pw::Result<ProducerDesc *> getProducerDesc();

  /** @return The current queue capacity in bytes. */
  size_t capacity();

  /** Disables this instance and notifies the producer. */
  void disableAndNotify();

  /** Notifies the Producer. */
  void notifyProducer();

  /** Clears the producer flags. */
  void clearFlags();

  // Members fixed on construction.
  Region mRegion;
  RemoteNotifyFn mRemoteNotifyFn;
  pw::allocator::Layout kBlockLayout;
  Queue *mQueue;
  ConsumerDesc *mDesc;
  MemoryAccess *mMemAccess;
  size_t mOverwriteResetOffset;
  uint32_t kBlockCapacity;
  uint32_t kDataOffset;

  BlockHeader *mHeadBlock;
  BlockHeader *mCurrBlock;
  size_t mAvailable = 0;
  size_t mPeeked = 0;
  uint32_t mCurrBlockIndex = 0;
  uint32_t mBlockListEpoch;
  uint32_t mCurrentFlags = static_cast<uint32_t>(ProducerFlags::kNone);
  bool mActive = true;
};

// Returns the offset of the object from base.
uint32_t toOffset(uintptr_t base, void *ptr) {
  auto addr = reinterpret_cast<uintptr_t>(ptr);
  if (addr < base || addr - base > UINT32_MAX) {
    return kOffsetInvalid;
  }
  return addr - base;
}

// Returns a pointer to the object at given offset from shmemBase or nullptr.
template <typename ObjType>
inline constexpr ObjType *fromOffset(
    const Region &region, uint32_t offset,
    pw::allocator::Layout layout = pw::allocator::Layout::Of<ObjType>()) {
  if (offset == kOffsetInvalid || offset > region.size - layout.size()) {
    return nullptr;
  }
  // All objects that would be accessed this way are allocated with fixed
  // alignment, however we should still check against bad values at runtime.
  if (auto addr = region.base + offset; !(addr & (layout.alignment() - 1))) {
    return reinterpret_cast<ObjType *>(addr);
  }
  return nullptr;
}

template <typename Fn, typename... Args>
void ProducerBase::forAllConsumers(uint16_t excludeMask, const Fn &fn,
                                   Args... args) {
  for (auto node = mQueue->consumerList.begin();
       node != mQueue->consumerList.end();) {
    auto *desc = node->desc;
    auto consumerFlags = desc->consumerFlags.load();
    auto producerFlags = desc->producerFlags.load();
    if (static_cast<uint16_t>(consumerFlags) ==
        static_cast<uint16_t>(internal::ConsumerFlags::kFinished)) {
      eraseConsumerNode(node);  // Moves node forward.
    } else {
      // NOTE: producerFlag and consumerFlags are cached and passed in to
      // avoid an unnecessary load(). fn() may reload them if required.
      if (!isFlagInMask(*desc, producerFlags, consumerFlags, excludeMask)) {
        fn(*desc, producerFlags, args...);
      }
      ++node;
    }
  }
}

}  // namespace internal
}  // namespace android::contexthub::data_flow
