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

#include "chre/shmem_spmc_queue/queue_defs.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace chre::shmem_spmc_queue {

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

//! Endpoint id for remote notifications or local callback.
union alignas(8) IdOrNotifyFn {
  LocalNotifyArgs localNotify;
  std::array<std::byte, 16> remoteId;
};
static_assert(sizeof(IdOrNotifyFn) == 16);

/** Producer metadata in shared memory. */
struct ProducerDesc {
  // Id for remote notification or local callback.
  IdOrNotifyFn idOrNotifyFn;
  // Current write index. Updated by the producer.
  std::atomic<uint32_t> writeIndex;
  // Correction to index for calculating index within Block::data.
  uint32_t indexCorrection;
  // Offset of the block containing the current write index in shared memory.
  uint32_t tailBlockOffset;
  // Counter incremented by the producer before any change to the block list.
  std::atomic<uint32_t> epoch;  // { 0-15: epoch counter | 16-31: block count }
};

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
  kReset = 0x1 << 3,      // Producer torn down.
};

/** Flags used by the Consumer to acknowledge ProducerFlags or tear down. */
enum class ConsumerFlags : uint16_t {
  kFlagsCleared = 0,  // Producer flags have been handled.
  kFinished,          // Consumer torn down and ready for deallocation.
};

/** Consumer metadata in shared memory. */
struct ConsumerDesc {
  // Id for remote notification or local callback.
  IdOrNotifyFn idOrNotifyFn;
  // Offset of the next dynamic consumer in shared memory.
  uint32_t nextConsumerOffset;
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
  // Padding bytes.
  uint8_t padding[8];
};

/** Queue metadata in shared memory. */
struct Queue {
  // Offset of the ProducerDesc in shared memory. Updated by the producer.
  std::atomic<uint32_t> producerOffset;
  // List of dynamic consumers.
  uint32_t dynamicConsumersHeadOffset;
  // Block capacity (in elements).
  uint32_t blockCapacity;
  // Element alignment. Used to check Consumer compatibility.
  uint8_t elementAlignment;
  // True iff notifications are done using IdOrNotifyFn.fn
  uint8_t localNotify;
  // Number of static consumers.
  uint8_t numStaticConsumers;
  // Padding bytes.
  uint8_t padding[1];
};

/** Header that precedes the aligned array of elements. */
struct BlockHeader {
  // Storage for the ProducerDesc in the current tail block.
  ProducerDesc producerDesc;
  // Offset of the next block in shared memory. May refer back to this block.
  std::atomic<uint32_t> nextBlockOffset;  // Updated by the producer.
  // Base index for reading/writing this block. Initialized to 0.
  std::atomic<uint32_t> baseIndex;  // Updated by the producer.
  // Index at which to jump to the next block. Initialized to kCapacity.
  std::atomic<uint32_t> skipIndex;  // Updated by the producer.
  // Padding bytes.
  uint8_t padding[4];
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
      sizeof(Block<ElementType>) + blockCapacity * sizeof(ElementType),
      alignof(Block<ElementType>));
}

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
      if (other.mActive) {
        mRemoteNotifyFn = std::move(other.mRemoteNotifyFn);
        kShmemBase = other.kShmemBase;
        kShmemSize = other.kShmemSize;
        mQueue = other.mQueue;
        mAllocator = other.mAllocator;
        mDataNotifier = other.mDataNotifier;
        mConsumerManager = other.mConsumerManager;
        mMemAccess = other.mMemAccess;
        kBlockLayout = other.kBlockLayout;
        kDataOffset = other.kDataOffset;
        kBlockCapacity = other.kBlockCapacity;
        kLocal = other.kLocal;
        mDesc = other.mDesc;
        mBlockCount = other.mBlockCount;
        mActive = true;
      }
      other.mActive = false;
    }
    return *this;
  }

  virtual ~ProducerBase();

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
  /**
   * Allocates an initial ring of blocks and initializes producer metadata.
   *
   * @param shmemBase The base address of the shared memory region.
   * @param shmemSize The size of the shared memory region.
   * @param queue The queue metadata in shared memory.
   * @param allocator Allocator used for element storage.
   * @param layout Layout for allocating Blocks.
   * @param maxBlockCount The maximum allowed blocks of element storage. Must
   * be >= minBlockCount.
   * @param minBlockCount The minimum required blocks of element storage. Must
   * be > 0.
   * @param idOrNotifyFn The new instance's id for remote notifications or the
   * LocalNotifyFn for notifying it.
   * @return pw::OkStatus() on success.
   */
  static pw::Status initialize(uintptr_t shmemBase, size_t shmemSize,
                               Queue *queue, pw::Allocator &allocator,
                               pw::allocator::Layout layout,
                               size_t maxBlockCount, size_t minBlockCount,
                               IdOrNotifyFn idOrNotifyFn);

  /**
   * See {@link Producer::create()} for a description of most parameters.
   *
   * @param queue The queue metadata in shared memory.
   * @param blockLayout Layout for allocating Blocks.
   * @param dataOffset The offset of the data from the start of BlockHeader.
   * @param remoteNotifyFn Function for notifying Consumers out-of-band only for
   * remote queues.
   */
  ProducerBase(uintptr_t shmemBase, uint32_t shmemSize, Queue &queue,
               pw::Allocator &allocator, pw::allocator::Layout blockLayout,
               size_t maxBlockCount, size_t minBlockCount, uint32_t dataOffset,
               DataNotifier &dataNotifier, ConsumerManager &consumerManager,
               RemoteNotifyFn remoteNotifyFn, MemoryAccess *memAccess);

  /**
   * Iterates over consumers to recalculate available space.
   *
   * While iterating, checks whether a Consumer has been overwritten or would
   * block the producer, flagging them accordingly. The optional increment is
   * used to determine whether a push()/reserve() would result in these states.
   *
   * Consumers in exceptional states (overwritten, blocking, etc.) except for
   * blocking the producer are excluded from the available space calculations.
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

  // Members fixed on construction.
  RemoteNotifyFn mRemoteNotifyFn;
  uintptr_t kShmemBase;
  Queue *mQueue;
  pw::Allocator *mAllocator;
  DataNotifier *mDataNotifier;
  ConsumerManager *mConsumerManager;
  MemoryAccess *mMemAccess;
  pw::allocator::Layout kBlockLayout;
  uint32_t kShmemSize;
  uint32_t kDataOffset;
  uint32_t kBlockCapacity;
  bool kLocal;

  ProducerDesc *mDesc;
  BlockHeader *mCurrBlock;
  size_t mBlockCount;
  size_t mReserved = 0;
  size_t mAvailable = 0;
  bool mActive = true;
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
      if (other.mStatus.ok()) {
        kShmemBase = other.kShmemBase;
        kShmemSize = other.kShmemSize;
        mQueue = other.mQueue;
        mDesc = other.mDesc;
        mRemoteNotifyFn = std::move(other.mRemoteNotifyFn);
        mMemAccess = other.mMemAccess;
        mOverwriteResetOffset = other.mOverwriteResetOffset;
        kBlockCapacity = other.kBlockCapacity;
        kDataOffset = other.kDataOffset;
        kLocal = other.kLocal;
        mEpoch = other.mEpoch;
        mStatus = pw::OkStatus();
      }
      other.mStatus = pw::Status::NotFound();
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
   * - pw::Status::Aborted(): The Producer is gone. The Consumer is not safe to
   * use.
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
   * Releases the count bytes previously peek()ed.
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
  size_t size();

  /** @return true iff the queue is empty. */
  bool empty();

 protected:
  /**
   * Checks arguments before initializing.
   *
   * @param shmemBase The base address of the queue shared memory region.
   * @param shmemSize The size of the queue shared memory region.
   * @param queueOffset The queue metadata in shared memory.
   * @param descOffset The consumer descriptor in shared memory.
   * @return On success, a pair of pointers to the Queue and ConsumerDesc.
   */
  static pw::Result<std::pair<Queue *, ConsumerDesc *>> checkArgs(
      uintptr_t base, uint32_t shmemSize, uint32_t queueOffset,
      uint32_t descOffset);

  /**
   * See {@link Consumer::createDynamic()} for most parameters.
   *
   * @param queue The Queue metadata in shared memory.
   * @param desc The ConsumerDesc in shared memory.
   * @param dataOffset The offset of the data from the start of BlockHeader.
   * @param remoteNotifyFn Function for notifying Consumers out-of-band only for
   * remote queues.
   */
  ConsumerBase(uintptr_t shmemBase, uint32_t shmemSize, Queue &queue,
               ConsumerDesc &desc, uint32_t dataOffset,
               RemoteNotifyFn remoteNotifyFn, MemoryAccess *memAccess,
               std::optional<size_t> overwriteResetOffset);

  /**
   * One-time post-construction initialization.
   *
   * @param idOrNotifyFn The new instance's id for remote notifications or the
   * LocalNotifyFn for notifying it.
   * @param policyBuilder Builder for the consumer's policy.
   * @return pw::OkStatus() on success.
   */
  pw::Status initialize(IdOrNotifyFn idOrNotifyFn,
                        ConsumerPolicyBuilder &policyBuilder);

  /** @return On success, the current producer descriptor. */
  pw::Result<ProducerDesc *> getProducerDesc();

  /**
   * Notifies the Producer.
   *
   * @param producerDesc The producer descriptor.
   */
  void notifyProducer(ProducerDesc &producerDesc);

  /**
   * Clears the producer flags.
   *
   * @param flag The value loaded from ConsumerDesc.producerFlags.
   */
  void clearFlag(uint32_t flag);

  // Members fixed on construction.
  RemoteNotifyFn mRemoteNotifyFn;
  uintptr_t kShmemBase;
  Queue *mQueue;
  ConsumerDesc *mDesc;
  MemoryAccess *mMemAccess;
  size_t mOverwriteResetOffset;
  uint32_t kShmemSize;
  uint32_t kBlockCapacity;
  uint32_t kDataOffset;
  bool kLocal;

  uint32_t mEpoch;
  pw::Status mStatus = pw::OkStatus();
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
    uintptr_t shmemBase, uint32_t shmemSize, uint32_t offset,
    pw::allocator::Layout layout = pw::allocator::Layout::Of<ObjType>()) {
  if (offset == kOffsetInvalid || offset > shmemSize - layout.size()) {
    return nullptr;
  }
  // All objects that would be accessed this way are allocated with fixed
  // alignment, however we should still check against bad values at runtime.
  if (auto addr = shmemBase + offset; !(addr & (layout.alignment() - 1))) {
    return reinterpret_cast<ObjType *>(addr);
  }
  return nullptr;
}

}  // namespace internal
}  // namespace chre::shmem_spmc_queue
