/*
 * Copyright (C) 2026 The Android Open Source Project
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

#include "chre/variant/config.h"

#if defined(__linux__) || CHRE_ATOMIC_UINT32_REF_ENABLED
#include "chre/target_platform/atomic_ref_base.h"

namespace chre {

/**
 * Provides an implementation of a reference to an atomic uint32_t, similar to
 * std::atomic_ref<uint32_t>. AtomicUint32RefBase is subclassed here to allow
 * platforms to use their own underlying atomic APIs.
 */
class AtomicUint32Ref : public AtomicUint32RefBase {
 public:
  /**
   * Initializes an atomic reference to the given non-atomic object. The object
   * must outlive the AtomicUint32Ref. Accesses to the object during the
   * lifetime of this atomic reference must be through an atomic reference,
   * including this one, or otherwise using compatible atomic operations.
   *
   * @param The object to create an atomic reference to.
   */
  template <typename T>
  explicit AtomicUint32Ref(T &object)
      : AtomicUint32RefBase(reinterpret_cast<uint32_t &>(
            const_cast<std::remove_cv_t<T> &>(object))) {
    static_assert(sizeof(T) == sizeof(uint32_t));
    static_assert(alignof(T) == alignof(uint32_t));
    static_assert(std::is_integral_v<T>);
  }

  /**
   * Returns true if the implementation of AtomicUint32Ref on the current
   * platform is always lock-free.
   */
  static constexpr bool is_always_lock_free();

  /**
   * Atomically assigns the desired value to the atomic object. Equivalent to
   * store().
   *
   * @param The value the object will be replaced with.
   *
   * @return The desired value.
   */
  uint32_t operator=(uint32_t desired);

  /**
   * Atomically loads the current value of the atomic object. Equivalent to
   * load().
   *
   * @return The current value of the object.
   */
  operator uint32_t() const {
    return load();
  }

  /**
   * Atomically increments the value stored in the atomic object by 1.
   *
   * @return The previous value of the object.
   */
  uint32_t operator++(int) {
    return fetch_increment();
  }

  /**
   * Atomically decrements the value stored in the atomic object by 1.
   *
   * @return The previous value of the object.
   */
  uint32_t operator--(int) {
    return fetch_decrement();
  }

  /**
   * Atomically loads the current value of the atomic object.
   *
   * @return The current value of the object.
   */
  uint32_t load() const;

  /**
   * Atomically replaces the current value of the atomic object.
   *
   * @param The value the object will be replaced with.
   */
  void store(uint32_t desired);

  /**
   * Atomically replaces the value of the atomic object.
   *
   * @param The value the object should have when the method returns.
   *
   * @return The previous value of the object.
   */
  uint32_t exchange(uint32_t desired);

  /**
   * Atomically adds the argument to the current value of the object.
   *
   * @param The amount which the object should be increased by.
   *
   * @return The previous value of the object.
   */
  uint32_t fetch_add(uint32_t arg);

  /**
   * Atomically increments the value stored in the atomic object by 1.
   *
   * @return The previous value of the object.
   */
  uint32_t fetch_increment();

  /**
   * Atomically subtracts the argument from the current value of the object.
   *
   * @param The amount which the object should be decreased by.
   *
   * @return The previous value of the object.
   */
  uint32_t fetch_sub(uint32_t arg);

  /**
   * Atomically decrements the value stored in the atomic object by 1.
   *
   * @return The previous value of the object.
   */
  uint32_t fetch_decrement();
};

}  // namespace chre

#include "chre/target_platform/atomic_ref_impl.h"
#endif  // defined(__linux__) || CHRE_ATOMIC_UINT32_REF_ENABLED
