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

#include <cstddef>
#include <cstdint>

#if __has_include(<aidl/android/hardware/contexthub/SharedDataRegion.h>)
#include <aidl/android/hardware/contexthub/SharedDataRegion.h>
#else  // __has_include(<aidl/android/hardware/contexthub/SharedDataRegion.h>)
#include "data_flow/internal/SharedDataRegion.h"
#endif  // __has_include(<aidl/android/hardware/contexthub/SharedDataRegion.h>)

#include "chre/platform/atomic_ref.h"
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

// This feature requires lock-free 32-bit atomic load/store operations to work
// across core clusters.
static_assert(::chre::AtomicUint32Ref::is_always_lock_free());

// Forward declarations.
class ConsumerManager;
class ConsumerPolicyBuilder;
class DataNotifier;
class MemoryAccess;

namespace internal {

// The following types are aliases of the shared memory ABI types defined in the
// ContextHub HAL AIDL. Note that in the interface, the names are mapped in the
// following way: queue -> data flow, producer -> source, consumer -> sink.
using ProducerDesc = ::aidl::android::hardware::contexthub::SharedDataRegion::
    DataFlowSourceMetadata;
using ConsumerDesc = ::aidl::android::hardware::contexthub::SharedDataRegion::
    DataFlowSinkMetadata;
using Queue =
    ::aidl::android::hardware::contexthub::SharedDataRegion::DataFlowMetadata;
using ElementConfig = ::aidl::android::hardware::contexthub::SharedDataRegion::
    DataFlowElementConfig;
using BlockHeader = ::aidl::android::hardware::contexthub::SharedDataRegion::
    DataFlowBlockHeader;
using VariableDataBlockHeader = ::aidl::android::hardware::contexthub::
    SharedDataRegion::DataFlowVariableSizeBlockHeader;
using VariableElementHeader = ::aidl::android::hardware::contexthub::
    SharedDataRegion::DataFlowVariableSizeElementHeader;
using SourceFlags = ::aidl::android::hardware::contexthub::SharedDataRegion::
    DataFlowSinkMetadata::SourceFlags;
using SinkFlags = ::aidl::android::hardware::contexthub::SharedDataRegion::
    DataFlowSinkMetadata::SinkFlags;

//! Analog to nullptr for offsets in shared memory.
constexpr uint32_t kOffsetInvalid = static_cast<uint32_t>(
    ::aidl::android::hardware::contexthub::SharedDataRegion::OFFSET_INVALID);

//! Maximum size of an endpoint id.
constexpr size_t kMaxIdSize = 16;

/**
 * Endpoint id for remote notifications or local callback.
 *
 * This overloads SharedDataRegion::EndpointIdFixedSize (remoteId.aidlId) for
 * the purposes of local queues or remote queues using a different id format.
 * Its size must be the same.
 */
union IdOrNotifyFn {
  LocalNotifyArgs localNotify;
  RemoteEndpointId remoteId;
};
static_assert(sizeof(IdOrNotifyFn) == sizeof(AidlEndpointId));
static_assert(alignof(IdOrNotifyFn) == 8);

/**
 * Flags used by the Producer to indicate exceptional state.
 *
 * Flags are mutually exclusive and the latest value takes precedence over
 * previous ones. The values are a bitmask to make it easy to compare against a
 * set of values.
 */
enum class ProducerFlags : uint16_t {
  kNone = 0x0,  // No flags set. Consumer does not need to ack this.
  kPendingInit = static_cast<uint16_t>(SourceFlags::PENDING_INIT),
  kBlocking = static_cast<uint16_t>(SourceFlags::BLOCKING),
  kOverwrite = static_cast<uint16_t>(SourceFlags::OVERWRITE),
  kFinished = static_cast<uint16_t>(SourceFlags::FINISHED),
  kDisconnected = static_cast<uint16_t>(SourceFlags::DISCONNECTED),
};

/** Flags used by the Consumer to acknowledge ProducerFlags or tear down. */
enum class ConsumerFlags : uint16_t {
  kFlagsCleared = static_cast<uint16_t>(SinkFlags::CLEARED),
  kFinished = static_cast<uint16_t>(SinkFlags::FINISHED),
};

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

/** Block of variable-size element storage. */
struct VariableDataBlock {
  VariableDataBlockHeader header;
  // Element storage is aligned to the size of the header so that the header can
  // always be read contiguously.
  alignas(VariableElementHeader) std::byte data[];
};

/** @return Layout for allocating VariableDataBlocks using pw::Allocator. */
constexpr pw::allocator::Layout variableDataBlockLayout(size_t blockCapacity) {
  constexpr auto kHdrAlignment = alignof(VariableElementHeader);
  constexpr auto kUnalignedBits = kHdrAlignment - 1;
  // Round up the block capacity to a multiple of the element header alignment
  // (power-of-2) to ensure that an element header is never split in an
  // alignment-breaking way.
  auto size = (blockCapacity + kUnalignedBits) & ~kUnalignedBits;
  return pw::allocator::Layout(offsetof(VariableDataBlock, data) + size,
                               alignof(VariableDataBlock));
}

/** Base class for item tracked in the consumer list for a queue. */
struct ConsumerListNode
    : public pw::containers::future::IntrusiveList<ConsumerListNode>::Item {};

/** Consumer notification and overwrite policy. */
struct ConsumerPolicy {
  NotificationPolicy notification;
  OverwritePolicy overwrite;
  union {
    uint32_t watermark;
    uint32_t periodMs;
    uint32_t data;
  };
};

/** Node for tracking a consumer descriptor in multiple containers. */
struct ConsumerNode : public ConsumerListNode {
  AllocatorRegion region;  // The region the descriptor was allocated from.
  RemoteEndpointId id;     // The consumer's id.
  ConsumerDesc *desc;      // The descriptor in shared memory.
  ConsumerPolicy policy;   // The consumer's policy.

  ConsumerNode(const RemoteEndpointId &_id, const AllocatorRegion &_region,
               ConsumerDesc *_desc, ConsumerPolicy _policy)
      : region(_region), id(_id), desc(_desc), policy(_policy) {}
};

/** Queue shared metadata and producer data that is not part of the ABI. */
struct QueuePrivate {
  Queue queue;
  pw::containers::future::IntrusiveList<ConsumerNode> consumerList;
};

/**
 * RAII wrapper for MemoryAccess.
 */
struct ScopedMemoryAccess {
  MemoryAccess *mMemAccess;
  uint8_t *mCount;

  /**
   * Enables access to memory for the lifetime of this instance.
   *
   * Supports nested accesses using the provided counter.
   *
   * @param memAccess The MemoryAccess to use for memory access.
   * @param count A reference to the memory access count to manage nested
   * accesses.
   */
  ScopedMemoryAccess(MemoryAccess *memAccess, uint8_t &count);

  /** Similar to above, but does not support nested accesses. */
  explicit ScopedMemoryAccess(MemoryAccess *memAccess);

  /** Releases access to memory if this is the outermost scoped access. */
  ~ScopedMemoryAccess();
};

/** Base class for Producers of any ElementType. */
class ProducerBase {
 public:
  // Move-only.
  ProducerBase(const ProducerBase &) = delete;
  ProducerBase &operator=(const ProducerBase &) = delete;
  ProducerBase(ProducerBase &&other) {
    mState = State::kMovedFrom;
    *this = std::move(other);
  }
  ProducerBase &operator=(ProducerBase &&other) {
    if (&other != this) {
      clear();
      if (other.mState == State::kMovedFrom) {
        mState = State::kMovedFrom;
      } else {
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
        mMemAccessCnt = other.mMemAccessCnt;
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

  /** @return the offset of the queue from the base of its allocator region. */
  uint32_t getQueueOffset() const;

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
  }

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
  }

  /** @return The current block count. */
  size_t getBlockCount() const {
    return mBlockCount;
  }

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

  /** Checks creation arguments. */
  static pw::Status checkArgs(const AllocatorRegion &region,
                              size_t maxBlockCount, size_t minBlockCount);

  /**
   * Allocates and initializes all fields in the queue metadata.
   *
   * @param region AllocatorRegion for allocating the queue.
   * @param capacity The capacity of each block in bytes.
   * @param elementSize The size of each element in bytes. 0 indicates that this
   * is a variable data queue.
   * @param elementAlignment The alignment of an element.
   * @param idOrNotifyFn The new instance's id for remote notifications or the
   * LocalNotifyFn for notifying it.
   * @param local True iff the queue is local.
   * @return On success, a pointer to the new queue metadata.
   */
  static pw::Result<QueuePrivate *> initQueue(
      const AllocatorRegion &region, size_t capacity, size_t elementSize,
      size_t elementAlignment, IdOrNotifyFn idOrNotifyFn, bool local);

  /**
   * See {@link Producer::createLocal()} for a description of most parameters.
   *
   * @param queue The queue metadata in shared memory.
   * @param blockLayout Layout for allocating Blocks.
   * @param blockCapacity The capacity of each Block in bytes.
   * @param dataOffset Offset of the data field in a Block.
   * @param remoteNotifyFn Function for notifying Consumers out-of-band only for
   * remote queues.
   */
  ProducerBase(const AllocatorRegion &region, QueuePrivate &queue,
               pw::allocator::Layout blockLayout, uint32_t blockCapacity,
               uint32_t dataOffset, size_t maxBlockCount, size_t minBlockCount,
               DataNotifier &dataNotifier, RemoteNotifyFn remoteNotifyFn,
               MemoryAccess *memAccess);

  /**
   * Allocates the initial block ring and finalizes the queue metadata.
   *
   * @param variableData true iff this is a variable data queue.
   * @return pw::OkStatus() on success.
   */
  pw::Status initialize(bool variableData);

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
   * @param node The consumer node.
   * @param current The current desc.producerFlags value.
   * @param flag The flag to set.
   * @param forceNotify If true, notify the consumer regardless of their policy.
   */
  void setConsumerFlag(ConsumerNode &node, uint32_t current, ProducerFlags flag,
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
   * @param id The id of the consumer.
   * @param region The region from which to allocate the consumer.
   * @param policy The policy to apply to the consumer.
   * @return The offset of the consumer descriptor in shared memory. Used to
   * initialize a Consumer instance.
   */
  pw::Result<uint32_t> addConsumer(const RemoteEndpointId &id,
                                   const AllocatorRegion &region,
                                   ConsumerPolicy policy);

  /**
   * Updates the policy associated with a consumer.
   *
   * @param id The id of the consumer previously registered with addConsumer().
   * @param policy The new policy to apply to the consumer.
   */
  pw::Status updateConsumerPolicy(const RemoteEndpointId &id,
                                  ConsumerPolicy policy);

  /**
   * Removes and deletes all consumers whose ids are matched by the predicate.
   *
   * @param match A predicate that returns true iff the consumer should be
   * removed.
   * @return pw::OkStatus() on success.
   */
  pw::Status pruneConsumers(
      const pw::Function<bool(const RemoteEndpointId &id)> &match);

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
   * void(ConsumerNode &node, uint32_t producerFlags, Args... args).
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

  /**
   * Checks that the policy is valid.
   *
   * @param policy The policy to check.
   * @return pw::InvalidArgument() if the policy is invalid.
   */
  pw::Status checkPolicy(ConsumerPolicy policy);

  /** Returns pw::Status::FailedPrecondition() if the producer is not active. */
  pw::Status checkActive() const;

  /** Clears the state of the producer, as if the destructor was called. */
  void clear();

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
  uint8_t mMemAccessCnt = 0;
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
        mMemAccessCnt = other.mMemAccessCnt;
      }
      other.mActive = false;
    }
    return *this;
  }

  virtual ~ConsumerBase();

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
  pw::Status checkState() {
    ScopedMemoryAccess memAccessScope(mMemAccess, mMemAccessCnt);
    return checkStateInternal();
  }

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
  pw::Status release(size_t count) {
    ScopedMemoryAccess memAccessScope(mMemAccess, mMemAccessCnt);
    PW_TRY(checkState());
    PW_TRY(releaseNoNotify(count));
    maybeNotifyOnRead();
    return pw::OkStatus();
  }

  /**
   * If available, pops data.size() bytes into the provided memory.
   *
   * @param data Span over the memory into which to pop data.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status pop(pw::ByteSpan data) {
    ScopedMemoryAccess memAccessScope(mMemAccess, mMemAccessCnt);
    PW_TRY(checkState());
    PW_TRY(popNoNotify(data));
    maybeNotifyOnRead();
    return pw::OkStatus();
  }

  /**
   * Syncs the read pointer to the write pointer minus an offset.
   *
   * Spans for existing peeked data are invalidated.
   *
   * @param offset The number of recent bytes to preserve. If greater than the
   * current size of the queue, resync() will fail.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status resync(size_t offset);

  /** @return the number of bytes available to read from the queue. */
  pw::Result<size_t> size();

  /** @return true iff the queue is empty. */
  pw::Result<bool> empty() {
    PW_TRY_ASSIGN(auto res, size());
    return res == 0;
  }

  /** @return true iff the producer can overwrite this consumer. */
  pw::Result<bool> isOverwritable();

 protected:
  /**
   * Checks arguments before initializing.
   *
   * @param region The shared memory region containing the queue.
   * @param descRegion If not nullptr, the region containing the consumer
   * descriptor. Otherwise, the descriptor is in the primary region.
   * @param queueOffset The queue metadata in region.
   * @param descOffset The consumer descriptor in region.
   * @return On success, a pair of pointers to the Queue and ConsumerDesc.
   */
  static pw::Result<std::pair<Queue *, ConsumerDesc *>> checkArgs(
      const Region &region, const Region *descRegion, uint32_t queueOffset,
      uint32_t descOffset);

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
   * @param overwriteResetOffset [optional] When recovering from being
   * overwritten, the offset from the write index to attempt to sync to.
   * @return pw::OkStatus() on success.
   */
  pw::Status initialize(IdOrNotifyFn idOrNotifyFn,
                        std::optional<size_t> overwriteResetOffset);

  /** Implementation of checkState(). */
  pw::Status checkStateInternal();

  /**
   * release() but without notifying the Producer.
   *
   * @param count The number of bytes to release.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status releaseNoNotify(size_t count);

  /**
   * pop() but without notifying the Producer.
   *
   * @param data Span over the memory into which to pop data.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status popNoNotify(pw::ByteSpan data);

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
   * @return The number of bytes actually advanced.
   */
  size_t advanceReadIndex(size_t count, std::optional<pw::ByteSpan> buf,
                          bool stopOnNextBlock = false);

  /** Depending on whether the producer is blocked, notify it on read. */
  void maybeNotifyOnRead();

  /**
   * Attempts to restore this instance to a valid state after being overwritten.
   *
   * If the block list epoch has not changed, attempts to fast forward the read
   * index. Otherwise, resyncs state to the producer, minus
   * mOverwriteResetOffset.
   *
   * @return Returns pw::Status::Aborted() if the producer is gone.
   */
  pw::Status handleOverwrite();

  /**
   * Updates mAvailable based on the current state in shared memory.
   *
   * @return pw::OkStatus() on success. See {@link #ConsumerBase::checkState()}
   * for error conditions.
   */
  pw::Status updateAvailable();

  /**
   * Fast-forwards the read index during overwrite recovery.
   *
   * @param offset The number of recent bytes to attempt to preserve.
   */
  virtual pw::Status overwriteFastForward(size_t offset);

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
  uint8_t mMemAccessCnt = 0;
};

// Returns the offset of the object from base.
inline uint32_t toOffset(uintptr_t base, void *ptr) {
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
    auto consumerFlags = ::chre::AtomicUint32Ref(desc->sinkFlags).load();
    auto producerFlags = ::chre::AtomicUint32Ref(desc->sourceFlags).load();
    if (static_cast<uint16_t>(consumerFlags) ==
        static_cast<uint16_t>(internal::ConsumerFlags::kFinished)) {
      eraseConsumerNode(node);  // Moves node forward.
    } else {
      // NOTE: producerFlag and consumerFlags are cached and passed in to
      // avoid an unnecessary load(). fn() may reload them if required.
      if (!isFlagInMask(*desc, producerFlags, consumerFlags, excludeMask)) {
        fn(*node, producerFlags, args...);
      }
      ++node;
    }
  }
}

}  // namespace internal
}  // namespace android::contexthub::data_flow
