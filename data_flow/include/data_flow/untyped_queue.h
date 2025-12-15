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

#include "data_flow/internal/queue_internal.h"
#include "data_flow/queue.h"
#include "data_flow/queue_defs.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {

/**
 * Allocates and initializes the metadata for a fixed-size queue.
 *
 * This variant is used where the element type is not known at compile-time on
 * the producer side. Instead, the element size and alignment are provided
 * explicitly.
 *
 * @param allocator The allocator used to allocate the queue metadata.
 * @param blockCapacity The capacity of each block in elements.
 * @param elementSize The size of each element in bytes. Must be > 0.
 * @param elementAlignment The alignment of each element.
 * @param local Iff true, this is a local queue. Otherwise, it is a remote
 * queue.
 * @return On success, a pointer to the queue metadata. The caller is expected
 * to deallocate this memory once the queue is no longer in use.
 */
pw::Result<void *> createQueueUntyped(pw::Allocator &allocator,
                                      size_t blockCapacity, size_t elementSize,
                                      size_t elementAlignment, bool local);

/**
 * An untyped producer instance that is aware of element size and alignment.
 *
 * This class is used where the element type is not known at compile-time and
 * uses element information extracted from the queue metadata to do runtime
 * checks on buffer sizes. Otherwise, it is just a thin wrapper around {@link
 * #internal::ProducerBase}. An UntypedProducer can communicate with both
 * UntypedConsumers and Consumer<T> instances where the element size and
 * alignment are the same.
 */
class UntypedProducer : protected internal::ProducerBase {
 public:
  /**
   * Creates an UntypedProducer instance for the given local Queue.
   *
   * See {@link #Producer::createLocal()} for details and an explanation of the
   * parameters.
   */
  static pw::Result<UntypedProducer> createLocal(
      AllocatorRegion region, void *queue, size_t maxBlockCount,
      size_t minBlockCount, DataNotifier &dataNotifier,
      LocalNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr);

  /**
   * Creates an UntypedProducer instance for the given remote Queue.
   *
   * See {@link #Producer::createRemote()} for details and an explanation of the
   * parameters.
   */
  static pw::Result<UntypedProducer> createRemote(
      AllocatorRegion region, void *queue, size_t maxBlockCount,
      size_t minBlockCount, DataNotifier &dataNotifier,
      RemoteNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr);

  // Moveable.
  UntypedProducer(UntypedProducer &&other)
      : ProducerBase(std::move(other)),
        mElementSize(other.mElementSize),
        mElementAlignment(other.mElementAlignment) {}
  UntypedProducer &operator=(UntypedProducer &&other) {
    ProducerBase::operator=(std::move(other));
    mElementSize = other.mElementSize;
    mElementAlignment = other.mElementAlignment;
    return *this;
  }

  /** Marks the Producer inactive, notifies consumers, and releases storage. */
  virtual ~UntypedProducer() = default;

  // See {@link #internal::ProducerBase} for documentation.
  using ProducerBase::full;
  using ProducerBase::getBlockCount;
  using ProducerBase::getMaxBlockCountTarget;
  using ProducerBase::getMinBlockCountTarget;
  using ProducerBase::setMaxBlockCountTarget;
  using ProducerBase::setMinBlockCountTarget;
  using ProducerBase::stop;

  /** @return a {@link #ConsumerManager} for this Producer. */
  ConsumerManager getConsumerManager() {
    return ConsumerManager(*this);
  }

  /** @return the size of each element in bytes. */
  size_t getElementSize() const {
    return mElementSize;
  }

  /** @return the alignment of each element. */
  size_t getElementAlignment() const {
    return mElementAlignment;
  }

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
  pw::Result<pw::ByteSpan> reserve(size_t count) {
    return ProducerBase::reserve(count * mElementSize);
  }

  /**
   * Release the first count elements reserved for writing.
   *
   * @param count The number of elements to release.
   * @return pw::OkStatus() on success.
   */
  pw::Status commit(size_t count) {
    return ProducerBase::commit(count * mElementSize);
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
  pw::Result<size_t> push(pw::ConstByteSpan elements,
                          bool allOrNothing = true) {
    if (elements.size() % mElementSize != 0) {
      return pw::Status::InvalidArgument();
    }
    PW_TRY_ASSIGN(size_t numBytes,
                  ProducerBase::push(pw::as_bytes(elements), allOrNothing));
    return numBytes / mElementSize;
  }

  /**
   * Returns the size of the queue based on the furthest-behind consumer.
   *
   * @param includeReserved Iff true, includes reserved space in the size.
   * @return the size of the queue.
   */
  size_t size(bool includeReserved = false) {
    return ProducerBase::size(includeReserved) / mElementSize;
  }

  /** @return the current queue capacity. */
  size_t capacity() const {
    return ProducerBase::capacity() / mElementSize;
  }

 protected:
  UntypedProducer(const AllocatorRegion &region, internal::QueuePrivate &queue,
                  pw::allocator::Layout blockLayout, size_t maxBlockCount,
                  size_t minBlockCount, DataNotifier &dataNotifier,
                  RemoteNotifyFn remoteNotifyFn, MemoryAccess *memAccess);

  size_t mElementSize;
  size_t mElementAlignment;
};

/**
 * An untyped consumer instance that is aware of element size and alignment.
 *
 * This class is used where the element type is not known at compile-time and
 * uses element information extracted from the queue metadata to do runtime
 * checks on buffer sizes. Otherwise, it is just a thin wrapper around {@link
 * #internal::ConsumerBase}. An UntypedConsumer can communicate with both
 * UntypedProducers and Producer<T> instances where the element size and
 * alignment are the same.
 */
class UntypedConsumer : protected internal::ConsumerBase {
 public:
  /**
   * Creates an UntypedConsumer instance for a local queue.
   *
   * See {@link #Consumer::createLocal()} for details and an explanation of the
   * parameters.
   */
  static pw::Result<UntypedConsumer> createLocal(
      Region region, uint32_t queueOffset, uint32_t descOffset,
      LocalNotifyArgs notifyArgs, MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt);

  /**
   * Creates an UntypedConsumer instance for a remote queue.
   *
   * See {@link #Consumer::createRemote()} for details and an explanation of the
   * parameters.
   */
  static pw::Result<UntypedConsumer> createRemote(
      Region region, std::optional<Region> descRegion, uint32_t queueOffset,
      uint32_t descOffset, RemoteNotifyArgs notifyArgs,
      MemoryAccess *memAccess = nullptr,
      std::optional<size_t> overwriteResetOffset = std::nullopt);

  // Moveable.
  UntypedConsumer(UntypedConsumer &&other)
      : ConsumerBase(std::move(other)),
        mElementSize(other.mElementSize),
        mElementAlignment(other.mElementAlignment) {}
  UntypedConsumer &operator=(UntypedConsumer &&other) {
    ConsumerBase::operator=(std::move(other));
    mElementSize = other.mElementSize;
    mElementAlignment = other.mElementAlignment;
    return *this;
  }

  /**
   * If active, marks this consumer removed in shared memory and notifies the
   * producer.
   */
  virtual ~UntypedConsumer() = default;

  // See {@link #internal::ConsumerBase} for documentation.
  using ConsumerBase::checkState;
  using ConsumerBase::disable;
  using ConsumerBase::empty;
  using ConsumerBase::isOverwritable;

  /** @return the size of each element in bytes. */
  size_t getElementSize() const {
    return mElementSize;
  }

  /** @return the alignment of each element. */
  size_t getElementAlignment() const {
    return mElementAlignment;
  }

  /**
   * If available, returns a span over the next available contiguous elements.
   *
   * See {@link #Consumer::peek()} for more details.
   *
   * @param count The number of elements to peek.
   * @return On success, a span over the next up-to-count contiguous elements.
   */
  pw::Result<pw::ConstByteSpan> peek(size_t count) {
    return ConsumerBase::peek(count * mElementSize);
  }

  /**
   * Releases the first count available elements back to the queue.
   *
   * See {@link #Consumer::release()} for more details.
   *
   * @param count The number of elements to release.
   * @return pw::OkStatus() on success.
   */
  pw::Status release(size_t count) {
    return ConsumerBase::release(count * mElementSize);
  }

  /**
   * If available, pops elements.size() elements into the provided memory.
   *
   * @param elements Span over the memory into which to pop the elements. Must
   * be a multiple of getElementSize().
   * @return pw::OkStatus() on success. See checkState() for error conditions.
   */
  pw::Status pop(pw::ByteSpan elements) {
    if (elements.size() % mElementSize != 0) {
      return pw::Status::InvalidArgument();
    }
    return ConsumerBase::pop(elements);
  }

  /**
   * Syncs the read pointer to the write pointer minus an offset.
   *
   * See {@link #Consumer::resync()} for more details.
   *
   * @param offset The number of recent elements to preserve.
   * @return pw::OkStatus() on success.
   */
  pw::Status resync(size_t offset) {
    return ConsumerBase::resync(offset * mElementSize);
  }

  /**
   * @return On success, the number of elements available for reading.
   *
   * See {@link #Consumer::size()} for more details.
   */
  pw::Result<size_t> size() {
    PW_TRY_ASSIGN(auto size, ConsumerBase::size());
    return size / mElementSize;
  }

 protected:
  UntypedConsumer(const Region &region, internal::Queue &queue,
                  internal::ConsumerDesc &desc, RemoteNotifyFn remoteNotifyFn,
                  MemoryAccess *memAccess);

  size_t mElementSize;
  size_t mElementAlignment;
};

}  // namespace android::contexthub::data_flow