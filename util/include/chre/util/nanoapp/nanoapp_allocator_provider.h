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

#include "chre/util/always_false.h"
#include "chre_api/chre.h"

namespace chre {

//! An AllocatorProvider that uses the CHRE API functions provided to nanoapps
//! for memory allocation.
//! @see DefaultAllocatorProvider
class NanoappAllocatorProvider {
 public:
  void *allocate(size_t size) {
    return chreHeapAlloc(size);
  }

  //! Allocates uninitialized memory for an object of type T
  template <typename T>
  void *allocate() {
    static_assert(
        alignof(T) <= alignof(std::max_align_t),
        "NanoappAllocatorProvider does not support over-aligned allocations");
    return allocate(sizeof(T));
  }

  //! Allocates uninitialized memory for an array of objects of type T
  template <typename T>
  void *allocateArray(size_t count) {
    // The CHRE API does not currently provide a standard way for nanoapps to
    // allocate over-aligned memory, but individual platforms may support it.
    // If this is necessary, consider filing an FR to the CHRE team, or use
    // a specialized allocator that routes to a platform-specific API.
    static_assert(
        alignof(T) <= alignof(std::max_align_t),
        "NanoappAllocatorProvider does not support over-aligned allocations");
    return allocate(sizeof(T) * count);
  }

  void deallocate(void *ptr) {
    chreHeapFree(ptr);
  }
};

}  // namespace chre
