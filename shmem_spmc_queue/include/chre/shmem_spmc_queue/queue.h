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
#include <optional>
#include <type_traits>
#include <utility>

#include "chre/shmem_spmc_queue/internal/queue_internal.h"
#include "chre/shmem_spmc_queue/queue_defs.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace chre::shmem_spmc_queue {

/**
 * Interface for accessing shared memory.
 */
class MemoryAccess {
 public:
  virtual ~MemoryAccess() = default;

  /** Acquires access to shared memory. */
  virtual void acquire() = 0;

  /** Releases access to shared memory. */
  virtual void release() = 0;

 protected:
  MemoryAccess() = default;
};

/**
 * Handles data availability notifications on a Queue.
 *
 * The base implementation supports the policies kNever, kHighWaterMark, and
 * kStreaming. kOpportunistic defaults to kHighWaterMark, and kPeriodic defaults
 * to kStreaming. Users or platforms may override the protected methods to
 * specifically handle these cases or override onWrite() entirely.
 */
class DataNotifier {
 public:
  DataNotifier() = default;
  virtual ~DataNotifier() = default;

  /**
   * Called whenever the write index is advanced.
   *
   * @param producer The producer that wrote.
   */
  virtual void onWrite(internal::ProducerBase &producer);

 protected:
  /**
   * Checks if the given endpoint should receive opportunistic notifications.
   *
   * @param id The endpoint id.
   * @return true iff the endpoint is active, e.g. core is on and endpoint
   * available.
   */
  virtual bool isActive(pw::span<const uint8_t, 16> /*id*/) {
    return true;
  }

  /**
   * Updates the consumer's batching period during onWrite().
   *
   * @param producer The associated producer.
   * @param consumerId The consumer id.
   * @param periodMs The period to update to in milliseconds. Disables timer if
   * empty.
   */
  virtual void updatePeriod(internal::ProducerBase &producer,
                            pw::span<const uint8_t, 16> consumerId,
                            std::optional<uint32_t> periodMs);
};

/** Manages the Consumers for one or more queues in the same region. */
class ConsumerManager {
 public:
  /**
   * @param shmemBase The base address of the shared memory region.
   * @param shmemSize The size of the shared memory region.
   * @param allocator The allocator from which to allocate consumer descriptors.
   * @param memAccess [optional] MemoryAccess implementation for accessing
   * shared memory.
   */
  ConsumerManager(void *shmemBase, size_t shmemSize, pw::Allocator &allocator,
                  MemoryAccess *memAccess = nullptr)
      : kShmemBase(reinterpret_cast<uintptr_t>(shmemBase)),
        mAllocator(&allocator),
        mMemAccess(memAccess),
        kShmemSize(shmemSize) {}

  /**
   * Allocates a new consumer and links it to the list in shared memory.
   *
   * @param queue Pointer to the queue metadata in shared memory.
   * @return The offset of the consumer descriptor in shared memory. Used to
   * initialize a Consumer instance.
   */
  pw::Result<uint32_t> addConsumer(void *queue);

  /**
   * Removes the descriptor for the consumer at given offset.
   *
   * This is generally used to remove a consumer on a process/core that has
   * crashed. When a Consumer removes itself programmatically, the Producer
   * identifies this through an in-band mechanism and removes the state for that
   * Consumer.
   *
   * @param queue Pointer to the queue metadata in shared memory.
   * @param offset The offset of the consumer descriptor in shared memory.
   * @return pw::OkStatus() on success.
   */
  pw::Status removeConsumer(void *queue, uint32_t offset);

 protected:
  friend class internal::ProducerBase;
  friend class DataNotifier;

  /**
   * Applies a functor to all consumers.
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
  void forAllConsumers(internal::Queue &queue, uint16_t excludeMask,
                       const Fn &fn, Args... args) {
    uint32_t *descOffsetPtr = &queue.dynamicConsumersHeadOffset;
    auto *desc = internal::fromOffset<internal::ConsumerDesc>(
        kShmemBase, kShmemSize, *descOffsetPtr);
    while (desc) {
      auto consumerFlags = desc->consumerFlags.load();
      auto producerFlags = desc->producerFlags.load();
      if (static_cast<uint16_t>(consumerFlags) ==
          static_cast<uint16_t>(internal::ConsumerFlags::kFinished)) {
        // Remove a dynamic Consumer that has marked itself for removal.
        *descOffsetPtr = desc->nextConsumerOffset;
        mAllocator->Deallocate(desc);
        desc = internal::fromOffset<internal::ConsumerDesc>(
            kShmemBase, kShmemSize, *descOffsetPtr);
      } else {
        // NOTE: producerFlag and consumerFlags are cached and passed in to
        // avoid an unnecessary load(). fn() may reload them if required.
        if (!isFlagInMask(*desc, producerFlags, consumerFlags, excludeMask)) {
          fn(*desc, producerFlags, args...);
        }
        descOffsetPtr = &desc->nextConsumerOffset;
        desc = internal::fromOffset<internal::ConsumerDesc>(
            kShmemBase, kShmemSize, *descOffsetPtr);
      }
    }
    // TODO(b/445479433): Add support for static Consumers.
  }

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

  uintptr_t kShmemBase;
  internal::Queue *mQueue;
  pw::Allocator *mAllocator;
  MemoryAccess *mMemAccess;
  uint32_t kShmemSize;
};

/**
 * Builder for notification and overwrite policy passed to a Consumer instance.
 */
class ConsumerPolicyBuilder {
 public:
  ConsumerPolicyBuilder()
      : mData(0),
        mNotificationPolicy(NotificationPolicy::kNever),
        mOverwritePolicy(OverwritePolicy::kAllowed) {}

  /** Sets the overwrite policy to allowed. */
  ConsumerPolicyBuilder &setOverwritable() {
    mOverwritePolicy = OverwritePolicy::kAllowed;
    return *this;
  }

  /** Sets the overwrite policy to disallowed. */
  ConsumerPolicyBuilder &setNonOverwritable() {
    mOverwritePolicy = OverwritePolicy::kDisallowed;
    return *this;
  }

  /** Sets the notification policy to never. */
  ConsumerPolicyBuilder &setNeverNotify() {
    mNotificationPolicy = NotificationPolicy::kNever;
    return *this;
  }

  /**
   * Sets the notification policy to opportunistic.
   *
   * @param lowWatermark The watermark above which to notify the consumer if
   * active.
   */
  ConsumerPolicyBuilder &setOpportunistic(size_t lowWatermark) {
    mNotificationPolicy = NotificationPolicy::kOpportunistic;
    mData = lowWatermark >= 1 << 24 ? (1 << 24) - 1 : lowWatermark;
    return *this;
  }

  /**
   * Sets the notification policy to high watermark.
   *
   * @param highWatermark The watermark above which to notify the consumer.
   */
  ConsumerPolicyBuilder &setHighWaterMark(size_t highWatermark) {
    mNotificationPolicy = NotificationPolicy::kHighWaterMark;
    mData = highWatermark >= 1 << 24 ? (1 << 24) - 1 : highWatermark;
    return *this;
  }

  /**
   * Sets the notification policy to periodic.
   *
   * @param periodMs The period in milliseconds to wait between notifications.
   */
  ConsumerPolicyBuilder &setPeriodic(size_t periodMs) {
    if (periodMs == 0) {
      return setStreaming();
    }
    mNotificationPolicy = NotificationPolicy::kPeriodic;
    mData = periodMs >= 1 << 24 ? (1 << 24) - 1 : periodMs;
    return *this;
  }

  /** Sets the notification policy to streaming. */
  ConsumerPolicyBuilder &setStreaming() {
    mNotificationPolicy = NotificationPolicy::kStreaming;
    return *this;
  }

 protected:
  friend class internal::ConsumerBase;

  internal::ConsumerPolicy build() const {
    internal::ConsumerPolicy policy;
    policy.policy = static_cast<uint8_t>(mNotificationPolicy) |
                    static_cast<uint8_t>(mOverwritePolicy);
    memcpy(policy.data, &mData, sizeof(policy.data));
    return policy;
  }

  size_t mData;
  NotificationPolicy mNotificationPolicy;
  OverwritePolicy mOverwritePolicy;
};

template <typename ElementType>
class Producer : protected internal::ProducerBase {
  static_assert(std::is_standard_layout_v<ElementType>);
  using Base = internal::ProducerBase;

 public:
  /**
   * Creates a Producer instance for the given local Queue.
   *
   * @param shmemBase The base address in the calling thread's memory space for
   * offsets in queue. Used to convert offsets in shared memory to pointers.
   * @param shmemSize The size of the shared memory region. Used to validate
   * offsets.
   * @param queue Pointer to queue metadata in shared memory.
   * @param allocator Allocator used for element storage. Allocations are within
   * the shared memory region beginning at base. Must outlive the new instance.
   * @param blockCapacity The capacity of each Block in elements.
   * @param maxBlockCount The maximum allowed blocks of element storage. Must
   * be >= minBlockCount. This can be adjusted at runtime.
   * @param minBlockCount The minimum required blocks of element storage. Must
   * be > 0.
   * @param dataNotifier DataNotifier implementation for making notification
   * decisions on write.
   * @param consumerManager ConsumerManager instance for Queue.
   * @param notifyArgs Callback and context for notifying this Producer.
   * @param memAccess [optional] MemoryAccess implementation for accessing
   * Queue and element storage.
   * @return An initialized Producer instance on success.
   */
  static pw::Result<Producer> createLocal(
      void *shmemBase, uint32_t shmemSize, void *queue,
      pw::Allocator &allocator, size_t blockCapacity, size_t maxBlockCount,
      size_t minBlockCount, DataNotifier &dataNotifier,
      ConsumerManager &consumerManager, LocalNotifyArgs notifyArgs,
      MemoryAccess *memAccess = nullptr) {
    if (notifyArgs.fn == nullptr) {
      return pw::Status::InvalidArgument();
    }
    auto queuePtr = static_cast<internal::Queue *>(queue);
    auto base = reinterpret_cast<uintptr_t>(shmemBase);
    auto blockLayout = internal::blockLayout<ElementType>(blockCapacity);
    PW_TRY(Base::initialize(base, shmemSize, queuePtr, allocator, blockLayout,
                            blockCapacity * sizeof(ElementType), maxBlockCount,
                            minBlockCount, {.localNotify = notifyArgs}));
    return Producer(base, shmemSize, *queuePtr, allocator, blockLayout,
                    maxBlockCount, minBlockCount, dataNotifier, consumerManager,
                    /*remoteNotifyFn=*/{}, memAccess);
  }

  /**
   * Like {@link #createLocal()} but for a remote queue.
   *
   * All parameters are the same except that notifyArgs is replaced by the
   * following:
   * @param notifyArgs Mechanism for notifying Consumers out-of-band and for
   * Consumers to notify this Producer.
   */
  static pw::Result<Producer> createRemote(
      void *shmemBase, uint32_t shmemSize, void *queue,
      pw::Allocator &allocator, size_t blockCapacity, size_t maxBlockCount,
      size_t minBlockCount, DataNotifier &dataNotifier,
      ConsumerManager &consumerManager, RemoteNotifyArgs notifyArgs,
      MemoryAccess *memAccess = nullptr) {
    if (!notifyArgs.fn) {
      return pw::Status::InvalidArgument();
    }
    auto queuePtr = static_cast<internal::Queue *>(queue);
    auto base = reinterpret_cast<uintptr_t>(shmemBase);
    auto blockLayout = internal::blockLayout<ElementType>(blockCapacity);
    PW_TRY(Base::initialize(base, shmemSize, queuePtr, allocator, blockLayout,
                            blockCapacity * sizeof(ElementType), maxBlockCount,
                            minBlockCount, {.remoteId = notifyArgs.id}));
    return Producer(base, shmemSize, *queuePtr, allocator, blockLayout,
                    maxBlockCount, minBlockCount, dataNotifier, consumerManager,
                    std::move(notifyArgs.fn), memAccess);
  }

  // Moveable.
  Producer(Producer &&other) : Base(std::move(other)) {}
  Producer &operator=(Producer &&other) {
    Base::operator=(std::move(other));
    return *this;
  }

  /** Marks the Producer inactive, notifies consumers, and releases element
   * storage. */
  virtual ~Producer() = default;

  // See {@link internal::ProducerBase} for documentation.
  using Base::getBlockCount;
  using Base::getMaxBlockCountTarget;
  using Base::getMinBlockCountTarget;
  using Base::setMaxBlockCountTarget;
  using Base::setMinBlockCountTarget;

  /**
   * Reserve up-to-count contiguous elements for writing if there is space.
   *
   * NOTE: On success, this call may return a span of size less than count. In
   * this case, there must be at least enough space in the queue for count
   * elements, however the element storage may not be contiguous (i.e. wraps
   * around a block or moves to a new block). Subsequent calls to
   * tryReserve()/reserve() can claim the next contiguous chunk.
   * WARNING: It is unsafe to make reservations from multiple threads as there
   * is no mechanism to preserve the ordering of reservations or to ensure that
   * commit() releases a specific reservation.
   *
   * @param count The number of elements to reserve.
   * @return If available, a span over the next up-to-count elements.
   */
  pw::Result<pw::span<ElementType>> reserve(size_t count) {
    PW_TRY_ASSIGN(pw::ByteSpan reservation,
                  Base::reserve(count * sizeof(ElementType)));
    return pw::span<ElementType>(
        reinterpret_cast<ElementType *>(reservation.data()),
        reservation.size() / sizeof(ElementType));
  }

  /**
   * Release the first count elements reserved for writing.
   *
   * @param count The number of elements to release.
   * @return pw::OkStatus() on success.
   */
  pw::Status commit(size_t count) {
    return Base::commit(count * sizeof(ElementType));
  }

  /**
   * Push the given elements to the queue if space is available.
   *
   * NOTE: This call will fail if there is an active reservation.
   *
   * @param elements The elements to push.
   * @param allOrNothing Iff true, this is an all-or-nothing operation.
   * @return The number of elements pushed. May be less than elements.size() if
   * allOrNothing is unset but is always > 0 on success.
   */
  pw::Result<size_t> push(pw::span<const ElementType> elements,
                          bool allOrNothing = true) {
    PW_TRY_ASSIGN(size_t numBytes,
                  Base::push(pw::as_bytes(elements), allOrNothing));
    return numBytes / sizeof(ElementType);
  }

  /**
   * Push a single element to the queue if space is available.
   *
   * @param element The element to push.
   * @return pw::OkStatus() on success.
   */
  pw::Status push(const ElementType &element) {
    return push({&element, 1}, /*allOrNothing=*/true).status();
  }

  /** @return true if full. See {@link internal::ProducerBase::full()}. */
  using Base::full;

  /**
   * Returns the size of the queue based on the furthest-behind consumer.
   *
   * @param includeReserved Iff true, includes reserved space in the size.
   * @return the size of the queue.
   */
  size_t size(bool includeReserved = false) {
    return Base::size(includeReserved) / sizeof(ElementType);
  }

  /** @return the current queue capacity. */
  size_t capacity() const {
    return Base::capacity() / sizeof(ElementType);
  }

 protected:
  Producer(uintptr_t shmemBase, uint32_t shmemSize, internal::Queue &queue,
           pw::Allocator &allocator, pw::allocator::Layout blockLayout,
           size_t maxBlockCount, size_t minBlockCount,
           DataNotifier &dataNotifier, ConsumerManager &consumerManager,
           RemoteNotifyFn remoteNotifyFn, MemoryAccess *memAccess)
      : Base(shmemBase, shmemSize, queue, allocator, blockLayout, maxBlockCount,
             minBlockCount, offsetof(internal::Block<ElementType>, data),
             dataNotifier, consumerManager, std::move(remoteNotifyFn),
             memAccess) {}
};

template <typename ElementType>
class Consumer : protected internal::ConsumerBase {
  static_assert(std::is_standard_layout_v<ElementType>);
  using Base = internal::ConsumerBase;

 public:
  /**
   * Creates a Consumer instance for a local queue.
   *
   * @param shmemBase The base address in the calling thread's memory space for
   * offsets in queue. Used to convert offsets in shared memory to pointers.
   * @param shmemSize The size of the shared memory region. Used to validate
   * offsets.
   * @param queueOffset The offset of the queue metadata in shared memory.
   * Allocated and shared by the producer endpoint. It should only be
   * deallocated after all consumers have marked themselves inactive.
   * Regardless, even if the producer endpoint crashes, the region [shmemBase,
   * shmemSize) is valid to access for the lifetime of the Consumer instance.
   * @param descOffset The offset of the consumer's descriptor in shared
   * memory. Allocated and shared by the producer endpoint.
   * @param notifyArgs Callback and context for notifying this Consumer.
   * @param policyBuilder Builder for the consumer's policy.
   * @param memAccess [optional] MemoryAccess implementation for accessing
   * Queue and element storage.
   * @param overwriteResetOffset [optional] Offset before the Producer's write
   * index to which to attempt to restore this Consumer's read index after an
   * overwrite event. Defaults to the queue block capacity / 2.
   * @return An initialized Consumer instance.
   */
  static pw::Result<Consumer> createLocal(
      void *shmemBase, uint32_t shmemSize, uint32_t queueOffset,
      uint32_t descOffset, LocalNotifyArgs notifyArgs,
      ConsumerPolicyBuilder &policyBuilder, MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt) {
    if (!notifyArgs.fn) {
      return pw::Status::InvalidArgument();
    }
    auto base = reinterpret_cast<uintptr_t>(shmemBase);
    PW_TRY_ASSIGN(auto queueAndDesc,
                  checkArgs(base, shmemSize, queueOffset, descOffset));
    Consumer consumer(base, shmemSize, *queueAndDesc.first,
                      *queueAndDesc.second, /*remoteNotifyFn=*/{}, memAccess);
    PW_TRY(consumer.initialize({.localNotify = notifyArgs}, policyBuilder,
                               overwriteResetOffset));
    return consumer;
  }

  /**
   * Like {@link #createLocal()} but for a remote queue.
   *
   * All parameters are the same except that notifyArgs is replaced by the
   * following:
   * @param notifyArgs Mechanism for notifying the Producer out-of-band and
   * for the Producer to notify this Consumer.
   */
  static pw::Result<Consumer> createRemote(
      void *shmemBase, uint32_t shmemSize, uint32_t queueOffset,
      uint32_t descOffset, RemoteNotifyArgs notifyArgs,
      ConsumerPolicyBuilder &policyBuilder, MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt) {
    if (!notifyArgs.fn) {
      return pw::Status::InvalidArgument();
    }
    auto base = reinterpret_cast<uintptr_t>(shmemBase);
    PW_TRY_ASSIGN(auto queueAndDesc,
                  checkArgs(base, shmemSize, queueOffset, descOffset));
    Consumer consumer(base, shmemSize, *queueAndDesc.first,
                      *queueAndDesc.second, std::move(notifyArgs.fn),
                      memAccess);
    PW_TRY(consumer.initialize({.remoteId = notifyArgs.id}, policyBuilder,
                               overwriteResetOffset));
    return consumer;
  }

  // Moveable.
  Consumer(Consumer &&other) : Base(std::move(other)) {}
  Consumer &operator=(Consumer &&other) {
    Base::operator=(std::move(other));
    return *this;
  }

  /** TODO(b/445479433) Support static consumers. */

  /** If active, marks this consumer removed in shared memory and notifies the
   * producer. */
  virtual ~Consumer() = default;

  /**
   * Updates the current policy, notifying the producer if necessary.
   *
   * See {@link #internal::ConsumerBase::updatePolicy()} for more details.
   */
  using Base::updatePolicy;

  /** Disables this instance. See {@link #internal::ConsumerBase::disable()} */
  using Base::disable;

  /**
   * Returns a pw::Status indicating the state of this Consumer.
   *
   * See {@link #internal::ConsumerBase::checkState()} for more details.
   */
  using Base::checkState;

  /**
   * If available, returns a span over the next available contiguous elements.
   *
   * NOTE: The span will follow the spans previously returned by peek().
   * NOTE: On success, even if the span is less than the requested count,
   * there is at least count available for reading.
   * WARNING: When used with OverwritePolicy::kAllowed, it is expected that the
   * Producer may overwrite this Consumer. The contents of a peek are only
   * guaranteed to have been valid if the subsequent call to release()
   * succeeded. checkState() may be used to confirm the validity of peek()ed
   * data in the middle of a long-running operation without calling release().
   *
   * @param count The number of elements to peek.
   * @return On success, a span over the next up-to-count contiguous elements.
   */
  pw::Result<pw::span<const ElementType>> peek(size_t count) {
    PW_TRY_ASSIGN(pw::ConstByteSpan bytes,
                  Base::peek(count * sizeof(ElementType)));
    return pw::span<const ElementType>(
        reinterpret_cast<const ElementType *>(bytes.data()),
        bytes.size() / sizeof(ElementType));
  }

  /**
   * Releases the first count available elements back to the queue.
   *
   * NOTE: This invalidates the views previously returned by peek().
   *
   * @param count The number of elements to release.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status release(size_t count) {
    return Base::release(count * sizeof(ElementType));
  }

  /**
   * If available, pops elements.size() elements into the provided memory.
   *
   * @param elements Span over the memory into which to pop the elements.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status pop(pw::span<ElementType> elements) {
    return Base::pop(pw::as_writable_bytes(elements));
  }

  /** @return On success, the element popped from the head of the queue. */
  pw::Result<ElementType> pop() {
    pw::Result<ElementType> result(ElementType{});
    PW_TRY(pop(pw::span<ElementType>{&*result, 1}));
    return result;
  }

  /**
   * Syncs the read pointer to the write pointer minus an offset.
   *
   * @param offset The number of recent elements to preserve. If greater than
   * the number of available elements resync() will fail.
   * @return pw::OkStatus() on success.
   */
  pw::Status resync(size_t offset) {
    return Base::resync(offset * sizeof(ElementType));
  }

  /**
   * @return On success, the number of elements available for reading.
   * Otherwise, returns an error status indicating why the size could not be
   * returned. See {@link #checkState()} for possible errors.
   */
  pw::Result<size_t> size() {
    PW_TRY_ASSIGN(auto size, Base::size());
    return size / sizeof(ElementType);
  }

  /** @return true iff the queue is empty. */
  using Base::empty;

 protected:
  Consumer(uintptr_t shmemBase, uint32_t shmemSize, internal::Queue &queue,
           internal::ConsumerDesc &desc, RemoteNotifyFn remoteNotifyFn,
           MemoryAccess *memAccess)
      : Base(shmemBase, shmemSize, queue, desc,
             internal::blockLayout<ElementType>(0),
             offsetof(internal::Block<ElementType>, data),
             std::move(remoteNotifyFn), memAccess) {}
};

/** Layout used to allocate queue metadata in shared memory. */
pw::allocator::Layout queueLayout() {
  return pw::allocator::Layout::Of<internal::Queue>();
}

/**
 * Initializes the queue metadata.
 *
 * @tparam ElementType The type of elements in the queue.
 * @tparam kBlockCapacity The capacity of each Block in elements.
 * @param queue Pointer to queue metadata in shared memory.
 * @param local True iff the queue is local.
 * @param numStaticConsumers [optional] The number of static consumers.
 */
template <typename ElementType, size_t kBlockCapacity>
void initQueue(void *queue, bool local, size_t numStaticConsumers = 0) {
  auto &queueRef = *static_cast<internal::Queue *>(queue);
  queueRef.producerOffset = internal::kOffsetInvalid;
  queueRef.dynamicConsumersHeadOffset = internal::kOffsetInvalid;
  queueRef.blockCapacity = kBlockCapacity * sizeof(ElementType);
  queueRef.elementAlignment = alignof(ElementType);
  queueRef.localNotify = local;
  queueRef.numStaticConsumers = numStaticConsumers;
  // TODO(b/445479433): Initialize static consumer descriptors.
}

/**
 * Allocates and initializes a new queue in shared memory.
 *
 * @tparam ElementType The type of elements in the queue.
 * @tparam kBlockCapacity The capacity of each Block in elements.
 * @param allocator Allocator used for allocating the queue metadata.
 * @param local True iff the queue is local.
 * @param numStaticConsumers [optional] The number of static consumers.
 * @return On success, a pointer to the new queue metadata.
 */
template <typename ElementType, size_t kBlockCapacity>
pw::Result<void *> createQueue(pw::Allocator &allocator, bool local,
                               size_t numStaticConsumers = 0) {
  if (auto *queue = allocator.Allocate(queueLayout()); queue) {
    initQueue<ElementType, kBlockCapacity>(queue, local, numStaticConsumers);
    return queue;
  }
  return pw::Status::ResourceExhausted();
}

}  // namespace chre::shmem_spmc_queue
