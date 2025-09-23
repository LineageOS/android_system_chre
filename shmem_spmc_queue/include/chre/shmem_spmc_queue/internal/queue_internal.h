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

#include "chre/shmem_spmc_queue/queue_defs.h"
#include "pw_status/status.h"

namespace chre::shmem_spmc_queue {

/** Consumer notification and overwrite policy. */
struct ConsumerPolicy {
  uint8_t policy;   // { 0-3: NotificationPolicy | 4-7: OverwritePolicy }
  uint8_t data[3];  // Interpreted based on NotificationPolicy.
};

namespace internal {

// TODO(b/444261568): Replace std::atomic<uint32_t> with chre::AtomicUint32 to
// allow for platforms that don't have <atomic> support. This will require a way
// to report something like is_always_lock_free for a given platform's
// implementation.
static_assert(std::atomic<uint32_t>::is_always_lock_free);

//! Endpoint id for remote notifications or local callback.
// NOTE: 16-byte aligned to ensure the same padding across platforms.
union alignas(16) IdOrNotifyFn {
  LocalNotifyFn fn;
  uint8_t id[16];
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
 * previous ones.
 */
enum class ProducerFlags : uint16_t {
  kNone = 0,
  kPendingInit,  // Consumer state allocated, pending Consumer().
  kBlocking,     // Producer cannot write until this Consumer reads.
  kOverwrite,    // Producer overwrote this Consumer.
  kReset,        // Producer torn down.
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
  ConsumerPolicy policy;
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

/** Base class for Producers of any ElementType. */
class ProducerBase {
 public:
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
  size_t getMaxBlockCountTarget() const;

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
  size_t getMinBlockCountTarget() const;

  /** @return The current block count. */
  size_t getBlockCount() const;
};

/** Base class for Consumers of any ElementType. */
class ConsumerBase {
 public:
  virtual ~ConsumerBase();
};

}  // namespace internal
}  // namespace chre::shmem_spmc_queue
