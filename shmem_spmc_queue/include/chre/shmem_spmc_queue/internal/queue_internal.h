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
namespace internal {

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

class ConsumerBase {
 public:
  virtual ~ConsumerBase();
};

}  // namespace internal
}  // namespace chre::shmem_spmc_queue
