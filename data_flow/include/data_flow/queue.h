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

#include "data_flow/internal/queue_internal.h"
#include "data_flow/queue_defs.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"
#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {

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
  virtual bool isActive(pw::span<const std::byte, 16> /*id*/) {
    return true;
  }

  /**
   * Updates the consumer's batching period during onWrite().
   *
   * @param producer The associated producer.
   * @param consumer The consumer node.
   * @param periodMs The period to update to in milliseconds. Disables timer if
   * empty.
   */
  virtual void updatePeriod(internal::ProducerBase &producer,
                            internal::ConsumerNode &consumer,
                            std::optional<uint32_t> periodMs);

  /**
   * Notifies the consumer if the watermark has been reached.
   *
   * @param producer The associated producer.
   * @param writeIndex The current write index.
   * @param policyData The data field from the notification policy.
   * @param consumer The consumer descriptor.
   */
  virtual void notifyIfAtWatermark(internal::ProducerBase &producer,
                                   uint32_t writeIndex, uint32_t policyData,
                                   internal::ConsumerDesc &consumer);
};

// Forward declaration for friend access.
template <typename ElementType>
class Producer;
class UntypedProducer;
class VariableDataProducer;
class ConsumerManager;

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
  friend class ConsumerManager;

  internal::ConsumerPolicy build() const {
    internal::ConsumerPolicy policy;
    policy.notification = mNotificationPolicy;
    policy.overwrite = mOverwritePolicy;
    policy.data = 0;
    if (mNotificationPolicy == NotificationPolicy::kPeriodic ||
        mNotificationPolicy == NotificationPolicy::kHighWaterMark ||
        mNotificationPolicy == NotificationPolicy::kOpportunistic) {
      policy.data = mData;
    }
    return policy;
  }

  size_t mData;
  NotificationPolicy mNotificationPolicy;
  OverwritePolicy mOverwritePolicy;
};

/** User-facing interface for managing consumers on a queue. */
class ConsumerManager {
 public:
  /**
   * Allocates and tracks a new consumer descriptor.
   *
   * @param id The id of the consumer. This may or may not be the same as the
   * remoteId in the case of remote queues. Must be <= 16 bytes long.
   * @param policyBuilder Builder for the policy to apply to the consumer.
   * @param region [optional] If provided, used to allocate the descriptor. It
   * must outlive the consumer. If not provided, the producer's region is used.
   * @return The offset of the consumer descriptor in shared memory. Used to
   * initialize a Consumer instance.
   */
  pw::Result<uint32_t> addConsumer(pw::ConstByteSpan id,
                                   ConsumerPolicyBuilder &policyBuilder,
                                   const AllocatorRegion *region = nullptr) {
    if (id.size() > internal::kMaxIdSize) {
      return pw::Status::InvalidArgument();
    }
    std::array<std::byte, internal::kMaxIdSize> idArray = {std::byte(0)};
    std::memcpy(idArray.data(), id.data(),
                std::min(id.size(), internal::kMaxIdSize));
    return mProducer->addConsumer(
        idArray, region ? *region : mProducer->mRegion, policyBuilder.build());
  }

  /**
   * Updates the policy associated with a consumer.
   *
   * @param id The id of the consumer previously registered with addConsumer().
   * @param policyBuilder Builder for the new policy to apply to the consumer.
   * @return pw::NotFound() if the consumer is not found.
   */
  pw::Status updateConsumerPolicy(pw::ConstByteSpan id,
                                  ConsumerPolicyBuilder &policyBuilder) {
    std::array<std::byte, internal::kMaxIdSize> idArray = {std::byte(0)};
    std::memcpy(idArray.data(), id.data(),
                std::min(id.size(), internal::kMaxIdSize));
    return mProducer->updateConsumerPolicy(idArray, policyBuilder.build());
  }

  /**
   * Removes all consumers matched by the given predicate.
   *
   * This must be used to clean up consumers on endpoints that have either
   * disconnected or crashed. Marks the consumer as no longer valid.
   *
   * When a consumer is programmatically removed, e.g. ~ConsumerBase(), the
   * ProducerBase will automatically detect that and clean up the descriptor
   * without using this method.
   *
   * @param match A predicate that returns true iff the consumer should be
   * removed.
   * @return pw::OkStatus() on success.
   */
  pw::Status pruneConsumers(
      const pw::Function<bool(pw::ConstByteSpan id)> &match) {
    return mProducer->pruneConsumers(match);
  }

  /**
   * @return the number of active consumers on the queue.
   *
   * Prunes any consumers that have set the ConsumerFlags::kFinished flag. This
   * can be used after calling Producer::stop() to check for outstanding
   * consumers.
   */
  size_t getNumConsumers() {
    return mProducer->getNumConsumers();
  }

 protected:
  template <typename ElementType>
  friend class Producer;
  friend class UntypedProducer;
  friend class VariableDataProducer;

  /**
   * @param region The region of shared memory from which to allocate
   * consumer descriptors.
   * @param queue The queue metadata.
   * @param memAccess [optional] MemoryAccess implementation for accessing
   * shared memory.
   */
  ConsumerManager(internal::ProducerBase &producer) : mProducer(&producer) {}

  internal::ProducerBase *mProducer;
};

template <typename ElementType>
class ProducerPeer;

template <typename ElementType>
class Producer : protected internal::ProducerBase {
  static_assert(std::is_standard_layout_v<ElementType>);
  using Base = internal::ProducerBase;

 public:
  /**
   * Creates a Producer instance for a new local Queue.
   *
   * @param region The region of memory from which to allocate the queue.
   * @param blockCapacity The capacity of each block in elements.
   * @param maxBlockCount The maximum allowed blocks of element storage. Must
   * be >= minBlockCount. This can be adjusted at runtime.
   * @param minBlockCount The minimum required blocks of element storage. Must
   * be > 0.
   * @param dataNotifier DataNotifier implementation for making notification
   * decisions on write.
   * @param notifyArgs Callback and context for notifying this Producer.
   * @param memAccess [optional] MemoryAccess implementation for accessing
   * Queue and element storage.
   * @return An initialized Producer instance on success.
   */
  static pw::Result<Producer> createLocal(
      AllocatorRegion region, size_t blockCapacity, size_t maxBlockCount,
      size_t minBlockCount, DataNotifier &dataNotifier,
      LocalNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr) {
    if (notifyArgs.fn == nullptr) {
      return pw::Status::InvalidArgument();
    }
    PW_TRY_ASSIGN(internal::QueuePrivate * queuePtr,
                  Base::initQueue(region, blockCapacity * sizeof(ElementType),
                                  sizeof(ElementType), alignof(ElementType),
                                  {.localNotify = notifyArgs}, /*local=*/true));
    Producer producer(region, *queuePtr, blockCapacity, maxBlockCount,
                      minBlockCount, dataNotifier, /*remoteNotifyFn=*/{},
                      memAccess);
    PW_TRY(producer.initialize(/*variableData=*/false));
    return producer;
  }

  /**
   * Like {@link #createLocal()} but for a remote queue.
   *
   * All parameters are the same except for the following:
   * @param notifyArgs Mechanism for notifying Consumers out-of-band and for
   * Consumers to notify this Producer.
   */
  static pw::Result<Producer> createRemote(
      AllocatorRegion region, size_t blockCapacity, size_t maxBlockCount,
      size_t minBlockCount, DataNotifier &dataNotifier,
      RemoteNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr) {
    if (!notifyArgs.fn) {
      return pw::Status::InvalidArgument();
    }
    PW_TRY_ASSIGN(
        internal::QueuePrivate * queuePtr,
        Base::initQueue(region, blockCapacity * sizeof(ElementType),
                        sizeof(ElementType), alignof(ElementType),
                        {.remoteId = notifyArgs.id}, /*local=*/false));
    Producer producer(region, *queuePtr, blockCapacity, maxBlockCount,
                      minBlockCount, dataNotifier, std::move(notifyArgs.fn),
                      memAccess);
    PW_TRY(producer.initialize(/*variableData=*/false));
    return producer;
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

  /** @return a ConsumerManager for this Producer. */
  ConsumerManager getConsumerManager() {
    return ConsumerManager(*this);
  }

  /**
   * Disables queue API calls on this instance and notifies all consumers.
   *
   * The user should call this method before destroying this instance if they
   * want to wait for all consumers to signal that they are no longer accessing
   * their consumer descriptors before destroying the producer. The number of
   * outstanding consumers can be retrieved with
   * ConsumerManager::getNumConsumers().
   *
   * See {@link #internal::ProducerBase::stop()} for more details.
   */
  using Base::stop;

  // See {@link internal::ProducerBase} for documentation.
  using Base::getBlockCount;
  using Base::getMaxBlockCountTarget;
  using Base::getMinBlockCountTarget;
  using Base::getQueueOffset;
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
  friend class ::android::contexthub::data_flow::ProducerPeer<ElementType>;

  Producer(const AllocatorRegion &region, internal::QueuePrivate &queue,
           size_t blockCapacity, size_t maxBlockCount, size_t minBlockCount,
           DataNotifier &dataNotifier, RemoteNotifyFn remoteNotifyFn,
           MemoryAccess *memAccess)
      : Base(region, queue, internal::blockLayout<ElementType>(blockCapacity),
             blockCapacity * sizeof(ElementType),
             offsetof(internal::Block<ElementType>, data), maxBlockCount,
             minBlockCount, dataNotifier, std::move(remoteNotifyFn),
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
   * @param region The shared memory region containing the queue.
   * @param queueOffset The offset of the queue metadata in shared memory.
   * Allocated and shared by the producer endpoint. It should only be
   * deallocated after all consumers have marked themselves inactive.
   * Regardless, even if the producer endpoint crashes, the region [shmemBase,
   * shmemSize) is valid to access for the lifetime of the Consumer instance.
   * @param descOffset The offset of the consumer's descriptor in shared
   * memory. Allocated and shared by the producer endpoint.
   * @param notifyArgs Callback and context for notifying this Consumer.
   * @param memAccess [optional] MemoryAccess implementation for accessing
   * Queue and element storage.
   * @param overwriteResetOffset [optional] Offset before the Producer's write
   * index to which to attempt to restore this Consumer's read index after an
   * overwrite event. Defaults to the queue block capacity / 2.
   * @return An initialized Consumer instance.
   */
  static pw::Result<Consumer> createLocal(
      Region region, uint32_t queueOffset, uint32_t descOffset,
      LocalNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt) {
    if (!notifyArgs.fn) {
      return pw::Status::InvalidArgument();
    }
    PW_TRY_ASSIGN(auto queueAndDesc, checkArgs(region, /*descRegion=*/nullptr,
                                               queueOffset, descOffset));
    Consumer consumer(region, *queueAndDesc.first, *queueAndDesc.second,
                      /*remoteNotifyFn=*/{}, memAccess);
    PW_TRY(
        consumer.initialize({.localNotify = notifyArgs}, overwriteResetOffset));
    return consumer;
  }

  /**
   * Like {@link #createLocal()} but for a remote queue.
   *
   * All parameters are the same except for the following:
   * @param descRegion [optional] The shared memory region containing the
   * consumer descriptor. If not provided, the descriptor is in the primary
   * region.
   * @param notifyArgs Mechanism for notifying the Producer out-of-band and
   * for the Producer to notify this Consumer.
   */
  static pw::Result<Consumer> createRemote(
      Region region, std::optional<Region> descRegion, uint32_t queueOffset,
      uint32_t descOffset, RemoteNotifyArgs notifyArgs,
      MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt) {
    if (!notifyArgs.fn) {
      return pw::Status::InvalidArgument();
    }
    auto *regionPtr = descRegion ? &*descRegion : nullptr;
    PW_TRY_ASSIGN(auto queueAndDesc,
                  checkArgs(region, regionPtr, queueOffset, descOffset));
    Consumer consumer(region, *queueAndDesc.first, *queueAndDesc.second,
                      std::move(notifyArgs.fn), memAccess);
    PW_TRY(
        consumer.initialize({.remoteId = notifyArgs.id}, overwriteResetOffset));
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

  /** @return true iff the producer can overwrite this consumer. */
  using Base::isOverwritable;

 protected:
  Consumer(const Region &region, internal::Queue &queue,
           internal::ConsumerDesc &desc, RemoteNotifyFn remoteNotifyFn,
           MemoryAccess *memAccess)
      : Base(region, queue, desc, internal::blockLayout<ElementType>(0),
             offsetof(internal::Block<ElementType>, data),
             std::move(remoteNotifyFn), memAccess) {}
};

/** A producer on a queue of variable-size elements. */
class VariableDataProducer : protected internal::ProducerBase {
  using Base = internal::ProducerBase;

 public:
  /**
   * Creates a VariableDataProducer instance for a new local Queue.
   *
   * See {@link #Producer::createLocal()} for details and an explanation of the
   * parameters.
   */
  static pw::Result<VariableDataProducer> createLocal(
      AllocatorRegion region, size_t blockCapacity, size_t maxBlockCount,
      size_t minBlockCount, DataNotifier &dataNotifier,
      LocalNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr);

  /**
   * Creates a VariableDataProducer instance for a new remote Queue.
   *
   * See {@link #Producer::createRemote()} for details and an explanation of the
   * parameters.
   */
  static pw::Result<VariableDataProducer> createRemote(
      AllocatorRegion region, size_t blockCapacity, size_t maxBlockCount,
      size_t minBlockCount, DataNotifier &dataNotifier,
      RemoteNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr);

  // Moveable.
  VariableDataProducer(VariableDataProducer &&other) : Base(std::move(other)) {
    mCurrentHdrPtr = other.mCurrentHdrPtr;
  }
  VariableDataProducer &operator=(VariableDataProducer &&other) {
    Base::operator=(std::move(other));
    mCurrentHdrPtr = other.mCurrentHdrPtr;
    return *this;
  }

  /** Marks the Producer inactive, notifies consumers, and releases element
   * storage. */
  virtual ~VariableDataProducer() = default;

  /** @return a ConsumerManager for this Producer. */
  ConsumerManager getConsumerManager() {
    return ConsumerManager(*this);
  }

  // See {@link internal::ProducerBase} for documentation.
  using Base::getBlockCount;
  using Base::getMaxBlockCountTarget;
  using Base::getMinBlockCountTarget;
  using Base::getQueueOffset;
  using Base::setMaxBlockCountTarget;
  using Base::setMinBlockCountTarget;

  /**
   * Reserve up-to-count bytes for a variable-size element if there is space.
   *
   * Subsequent calls to reserve() before commit() increase the size of the
   * element.
   *
   * NOTE: On success, even if the reservation is less than the requested count,
   * there are at least count bytes available for writing which can be claimed
   * via additional calls to reserve().
   *
   * @param count The number of bytes to reserve.
   * @return If available, a span over the next up-to-count bytes. Returns
   * pw::Status::Unavailable() if there isn't available space.
   */
  pw::Result<pw::ByteSpan> reserve(size_t count);

  /**
   * Truncates the most recent unsent element to the given size.
   *
   * NOTE: This is only valid if there is an active reservation.
   * NOTE: If size is 0, the reservation is effectively released (i.e. push()
   * may be called).
   *
   * @param size The size to truncate the current element to. Must be <= the
   * current size of the reservation.
   * @return Fails with pw::Status::FailedPrecondition() if there isn't an
   * active reservation. Fails with pw::Status::OutOfRange() if the requested
   * size is greater than the current size of the reservation.
   */
  pw::Status truncate(size_t size);

  /**
   * Release the current reserved variable-size element.
   *
   * @return pw::OkStatus() on success. Fails with
   * pw::Status::FailedPrecondition() if there isn't an active reservation.
   */
  pw::Status commit();

  /**
   * Push a variable-size element to the queue.
   *
   * NOTE: Fails if there is an active reservation.
   *
   * @param element Span over the element to push.
   * @return pw::OkStatus() on success. Fails with
   * pw::Status::FailedPrecondition() if there is an active reservation. Fails
   * with pw::Status::Unavailable() if there isn't available space.
   */
  pw::Status push(pw::ConstByteSpan element);

  // See {@link internal::ProducerBase} for documentation.
  using Base::capacity;
  using Base::full;
  using Base::size;

 protected:
  VariableDataProducer(const AllocatorRegion &region,
                       internal::QueuePrivate &queue, size_t blockCapacity,
                       size_t maxBlockCount, size_t minBlockCount,
                       DataNotifier &dataNotifier,
                       RemoteNotifyFn remoteNotifyFn, MemoryAccess *memAccess);

  /**
   * On commit()/push(), if necessary, sets the block first element index.
   *
   * This index is used by the consumer when overwritten to seek to an element
   * while trying to catch up.
   */
  void updateFirstElementIndex();

  /**
   * Does additional setup for variable data blocks on entering a new block.
   *
   * Clears the first element index in the new block.
   */
  void enterNextBlock(internal::BlockHeader *&block, uint32_t *correction,
                      uint32_t &index, bool convertSkipToBase) override;

  // If set, size of the current reserved element in shared memory.
  internal::VariableDataHeader *mCurrentHdrPtr = nullptr;
};

/** A consumer on a queue of variable-size elements. */
class VariableDataConsumer : protected internal::ConsumerBase {
  using Base = internal::ConsumerBase;

 public:
  /**
   * Creates a VariableDataConsumer instance for the given local Queue.
   *
   * See {@link #Consumer::createLocal()} for details and an explanation of the
   * parameters.
   */
  static pw::Result<VariableDataConsumer> createLocal(
      Region region, uint32_t queueOffset, uint32_t descOffset,
      LocalNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt);

  /**
   * Creates a VariableDataConsumer instance for the given remote Queue.
   *
   * See {@link #Consumer::createRemote()} for details and an explanation of the
   * parameters.
   */
  static pw::Result<VariableDataConsumer> createRemote(
      Region region, std::optional<Region> descRegion, uint32_t queueOffset,
      uint32_t descOffset, RemoteNotifyArgs notifyArgs,
      MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt);

  // Moveable.
  VariableDataConsumer(VariableDataConsumer &&other) : Base(std::move(other)) {}
  VariableDataConsumer &operator=(VariableDataConsumer &&other) {
    Base::operator=(std::move(other));
    return *this;
  }

  /** If active, marks this consumer removed in shared memory and notifies the
   * producer. */
  virtual ~VariableDataConsumer() = default;

  /** Disables this instance. See {@link #internal::ConsumerBase::disable()} */
  using Base::disable;

  /**
   * Returns a pw::Status indicating the state of this Consumer.
   *
   * NOTE: All API calls other than disable() may fail with the statuses
   * returned by checkState().
   *
   * See {@link #internal::ConsumerBase::checkState()} for more details.
   */
  using Base::checkState;

  /**
   * Returns the size of the current head element if it exists.
   *
   * @return On success, the size of the current head element. Fails with
   * pw::Status::Unavailable() if there are no available elements.
   */
  pw::Result<size_t> getHeadSize();

  /** @return The next contiguous span of the head element. */
  pw::Result<pw::ConstByteSpan> peek();

  /**
   * Releases the head element.
   *
   * NOTE: This will invalidate the view(s) returned by peek().
   * NOTE: This will release the entire element even if it has only been partly
   * read.
   *
   * @return pw::OkStatus() on success. Fails with pw::Status::Unavailable() if
   * there are no elements.
   */
  pw::Status release() {
    PW_TRY(releaseNoNotify());
    maybeNotifyOnRead();
    return pw::OkStatus();
  }

  /**
   * If available, pops the next element into buffer.
   *
   * NOTE: This can be called without calling getNextSize() so long as buffer is
   * always at least as large as the largest expected element size.
   *
   * @param buffer Span over the memory into which to copy the next element.
   * Must be >= getNextSize(). Resized to the size of the element.
   * @return pw::OkStatus() on success. Fails with
   * pw::Status::FailedPrecondition() if there is an unreleased peek(). Fails
   * with pw::Status::Unavailable() if there are no elements.
   */
  pw::Status pop(pw::ByteSpan &buffer);

  /**
   * Syncs the read pointer to the write pointer minus an offset in bytes.
   *
   * Once reaching the offset, the read pointer seeks to the next element
   * boundary.
   *
   * @param offset The number of recent bytes to preserve. If greater than
   * the number of available bytes resync() will fail.
   * @return pw::OkStatus() on success. Fails with pw::Status::OutOfRange() if
   * offset is greater than the size of the queue.
   */
  pw::Status resync(size_t offset);

  // See {@link internal::ConsumerBase} for documentation.
  using Base::empty;
  using Base::isOverwritable;
  using Base::size;

 protected:
  VariableDataConsumer(const Region &region, internal::Queue &queue,
                       internal::ConsumerDesc &desc,
                       RemoteNotifyFn remoteNotifyFn, MemoryAccess *memAccess);

  /**
   * release() but without notifying the Producer.
   *
   * @return pw::OkStatus() on success. Fails with pw::Status::Unavailable() if
   * there are no elements.
   */
  pw::Status releaseNoNotify();

  /**
   * Specialized overwrite fast-forwarding for variable-data queues.
   *
   * @param offset The number of recent bytes to attempt to preserve. If not an
   * element boundary, the read index will be advanced to the nearest element
   * past it.
   * @return See {@link #internal::ConsumerBase::checkState()} for error
   * conditions.
   */
  pw::Status overwriteFastForward(size_t offset) override;

  std::optional<internal::VariableDataHeader> mCurrentHdr = std::nullopt;
};

/** Layout used to allocate queue metadata in shared memory. */
inline pw::allocator::Layout queueLayout() {
  return pw::allocator::Layout::Of<internal::QueuePrivate>();
}

}  // namespace android::contexthub::data_flow
