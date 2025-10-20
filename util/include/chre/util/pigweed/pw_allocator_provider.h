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

#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"

namespace chre {

//! A class that provides allocation and deallocation functionality for
//! containers using a Pigweed allocator.
//! @see DefaultAllocatorProvider
class PwAllocatorProvider {
 public:
  PwAllocatorProvider() = delete;
  PwAllocatorProvider(pw::Allocator &allocator) : mAllocator(allocator) {}

  void *allocate(size_t size) {
    return mAllocator.Allocate(pw::allocator::Layout(size));
  }

  //! Allocates uninitialized memory for an object of type T
  template <typename T>
  void *allocate() {
    // Note that skipping the alignment specification is slightly more
    // efficient, hence the branch
    if constexpr (alignof(T) > alignof(std::max_align_t)) {
      return allocateArray<T>(1);
    } else {
      return allocate(sizeof(T));
    }
  }

  //! Allocates uninitialized memory for an array of objects of type T
  template <typename T>
  void *allocateArray(size_t count) {
    if constexpr (alignof(T) > alignof(std::max_align_t)) {
      return static_cast<T *>(mAllocator.Allocate(
          pw::allocator::Layout(sizeof(T) * count, alignof(T))));
    } else {
      return allocate(sizeof(T) * count);
    }
  }

  void deallocate(void *ptr) {
    mAllocator.Deallocate(ptr);
  }

  bool resize(void *ptr, size_t newSize) {
    return mAllocator.Resize(ptr, newSize);
  }

 private:
  pw::Allocator &mAllocator;
};

}  // namespace chre
