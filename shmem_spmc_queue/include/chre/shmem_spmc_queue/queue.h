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
#include "pw_span/cast.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace chre::shmem_spmc_queue {

/** Layout used to allocate queue metadata in shared memory. */
pw::allocator::Layout queueLayout() {
  return pw::allocator::Layout::Of<internal::Queue>();
}

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
  /**
   * Called whenever the write index is advanced.
   *
   * @param producer The producer that wrote.
   */
  virtual void onWrite(internal::ProducerBase &producer);

 protected:
  DataNotifier() = default;
  ~DataNotifier() = default;

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
  virtual void updatePeriod(internal::ProducerBase & /*producer*/,
                            pw::span<const uint8_t, 16> /*consumerId*/,
                            std::optional<uint32_t> /*periodMs*/);
};

/** Manages the Consumers for one Queue. */
class ConsumerManager {
 public:
  /**
   * @param base The base address of the shared memory region.
   * @param queue Pointer to the queue metadata in shared memory.
   * @param allocator The allocator from which to allocate consumer descriptors.
   */
  ConsumerManager(uintptr_t base, void *queue, pw::Allocator &allocator);

  /**
   * Allocates a new consumer and links it to the list in shared memory.
   *
   * @return The offset of the consumer descriptor in shared memory. Used to
   * initialize a Consumer instance.
   */
  pw::Result<uint32_t> addConsumer();

  /**
   * Removes the descriptor for the consumer at given offset.
   *
   * This is generally used to remove a consumer on a process/core that has
   * crashed. When a Consumer removes itself programmatically, the Producer
   * identifies this through an in-band mechanism and removes the state for that
   * Consumer.
   *
   * @param offset The offset of the consumer descriptor in shared memory.
   * @return pw::OkStatus() on success.
   */
  pw::Status removeConsumer(uint32_t offset);

 private:
  friend class ProducerBase;
  friend class DataNotifier;

  /**
   * Applies a functor to all consumers.
   *
   * This is used by the Producer and DataNotifier instances to access Consumer
   * state.
   *
   * fn has the following signature:
   * void(ConsumerDesc &desc, ConsumerDesc &prev, Args... args).
   * prev can be used to remove a consumer while iterating.
   */
  template <typename Fn, typename... Args>
  void forAllConsumers(const Fn &fn, Args... args);
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
   * @param maxBlockCount The maximum allowed blocks of element storage. Must
   * be >= minBlockCount. This can be adjusted at runtime.
   * @param minBlockCount The minimum required blocks of element storage. Must
   * be > 0.
   * @param dataNotifier DataNotifier implementation for making notification
   * decisions on write.
   * @param consumerManager ConsumerManager instance for Queue.
   * @param notifyFn Callback for notifying this Producer.
   * @param memAccess [optional] MemoryAccess implementation for accessing
   * Queue and element storage.
   * @return An initialized Producer instance on success.
   */
  template <size_t kBlockCapacity>
  static pw::Result<Producer> createLocal(
      void *shmemBase, size_t shmemSize, void *queue,
      pw::Allocator &allocator, size_t maxBlockCount, size_t minBlockCount,
      DataNotifier &dataNotifier, ConsumerManager &consumerManager,
      LocalNotifyFn notifyFn, MemoryAccess *memAccess = nullptr) {
    auto base = reinterpret_cast<uintptr_t>(shmemBase);
    auto blockLayout = internal::blockLayout<ElementType, kBlockCapacity>();
    auto &queueRef = *static_cast<internal::Queue *>(queue);
    PW_TRY(Base::initialize(base, shmemSize, queueRef, allocator, blockLayout,
                            minBlockCount, kBlockCapacity * sizeof(ElementType),
                            std::move(notifyFn), /*id=*/std::nullopt));
    return Producer(base, shmemSize, queueRef, allocator, blockLayout,
                    maxBlockCount, minBlockCount, dataNotifier, consumerManager,
                    /*remoteNotifyFn=*/{}, memAccess);
  }

  /**
   * Like {@link #createLocal()} but for a remote queue.
   *
   * All parameters are the same except that notify_fn is replaced by the
   * following:
   * @param notifyArgs Mechanism for notifying Consumers out-of-band and for
   * Consumers to notify this Producer.
   */
  template <size_t kBlockCapacity>
  static pw::Result<Producer> createRemote(
      void *shmemBase, size_t shmemSize, void *queue,
      pw::Allocator &allocator, size_t maxBlockCount, size_t minBlockCount,
      DataNotifier &dataNotifier, ConsumerManager &consumerManager,
      RemoteNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr) {
    auto base = reinterpret_cast<uintptr_t>(shmemBase);
    auto blockLayout = internal::blockLayout<ElementType, kBlockCapacity>();
    auto &queueRef = *static_cast<internal::Queue *>(queue);
    PW_TRY(Base::initialize(base, shmemSize, queueRef, allocator, blockLayout,
                            minBlockCount, kBlockCapacity * sizeof(ElementType),
                            /*notifyFn=*/{}, pw::ConstByteSpan(notifyArgs.id)));
    return Producer(base, shmemSize, queueRef, allocator, blockLayout,
                    maxBlockCount, minBlockCount, dataNotifier, consumerManager,
                    std::move(notifyArgs.fn), memAccess);
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
    return pw::span_cast<ElementType>(reservation);
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

  /** @return true if full. See {@link internal::ProducerBase::full()}. */
  using Base::full;

  /**
   * Returns the size of the queue based on the furthest-behind consumer.
   *
   * @param includeReserved Iff true, includes reserved space in the size.
   * @param includeOverwriteable Iff true, includes overwriteable space in the
   * size.
   * @return the size of the queue.
   */
  size_t size(bool includeReserved = false,
              bool includeOverwriteable = true) const {
    return Base::size(includeReserved, includeOverwriteable) /
           sizeof(ElementType);
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
   * @param notifyFn Callback for notifying this Consumer.
   * @param policy The policy for receiving notifications / overwriting data.
   * @param memAccess [optional] MemoryAccess implementation for accessing
   * Queue and element storage.
   * @param overwriteResetOffset [optional] Offset before the Producer's write
   * index to which to attempt to restore this Consumer's read index after an
   * overwrite event. Defaults to the queue block capacity / 2.
   * @return An initialized Consumer instance.
   */
  static pw::Result<Consumer> createLocal(
      void *shmemBase, size_t shmemSize, uint32_t queueOffset,
      uint32_t descOffset, LocalNotifyFn notifyFn, ConsumerPolicy policy,
      MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt);

  /**
   * Like {@link #createLocal()} but for a remote queue.
   *
   * All parameters are the same except that notify_fn is replaced by the
   * following:
   * @param notifyArgs Mechanism for notifying the Producer out-of-band and
   * for the Producer to notify this Consumer.
   */
  static pw::Result<Consumer> createRemote(
      void *shmemBase, size_t shmemSize, uint32_t queueOffset,
      uint32_t descOffset, RemoteNotifyArgs notifyArgs, ConsumerPolicy policy,
      MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt);

  /** TODO(b/445479433) Support static consumers. */

  /** If active, marks this consumer removed in shared memory and notifies the
   * producer. */
  ~Consumer();

  /**
   * Updates the current policy. Notifies the Producer.
   *
   * @param policy The new ConsumerPolicy.
   * @return pw::OkStatus() on success.
   */
  pw::Status updatePolicy(ConsumerPolicy policy);

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
  pw::Result<pw::span<const ElementType>> peek(size_t count);

  /**
   * Releases the first count available elements back to the queue.
   *
   * NOTE: This invalidates the views previously returned by peek().
   *
   * @param count The number of elements to release.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status release(size_t count);

  /**
   * If available, pops elements.size() elements into the provided memory.
   *
   * @param elements Span over the memory into which to pop the elements.
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status pop(pw::span<ElementType> elements);

  /**
   * Syncs the read pointer to the write pointer minus an offset.
   *
   * @param offset The number of recent elements to preserve. If greater than
   * the number of available elements resync() will fail.
   * @return pw::OkStatus() on success.
   */
  pw::Status resync(size_t offset);

  /** @return the number of elements currently in the queue. */
  size_t size();

  /** @return true iff the queue is empty. */
  bool empty();
};

}  // namespace chre::shmem_spmc_queue
