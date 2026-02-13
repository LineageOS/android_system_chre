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

#include "chre/platform/atomic_ref.h"

#include <atomic>

namespace chre {

constexpr bool AtomicUint32Ref::is_always_lock_free() {
  return std::atomic_ref<uint32_t>::is_always_lock_free;
}

inline uint32_t AtomicUint32Ref::operator=(uint32_t desired) {
  return mAtomic = desired;
}

inline uint32_t AtomicUint32Ref::load() const {
  return mAtomic.load();
}

inline void AtomicUint32Ref::store(uint32_t desired) {
  mAtomic.store(desired);
}

inline uint32_t AtomicUint32Ref::exchange(uint32_t desired) {
  return mAtomic.exchange(desired);
}

inline uint32_t AtomicUint32Ref::fetch_add(uint32_t arg) {
  return mAtomic.fetch_add(arg);
}

inline uint32_t AtomicUint32Ref::fetch_increment() {
  return mAtomic.fetch_add(1);
}

inline uint32_t AtomicUint32Ref::fetch_sub(uint32_t arg) {
  return mAtomic.fetch_sub(arg);
}

inline uint32_t AtomicUint32Ref::fetch_decrement() {
  return mAtomic.fetch_sub(1);
}

}  // namespace chre
