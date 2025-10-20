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

#ifndef CHRE_UTIL_DEFAULT_ALLOCATOR_PROVIDER_H_
#define CHRE_UTIL_DEFAULT_ALLOCATOR_PROVIDER_H_

#include <cstddef>

#include "chre/util/container_support.h"

namespace chre {

//! A class that provides allocation and deallocation functionality for
//! containers. This class is intended to be used as the default template
//! parameter for CHRE containers' allocator types, and is suitable for most
//! usages, though nanoapps that are commonly used in production should
//! typically prefer NanoappAllocatorProvider.
//! Note that allocation providers are never expected to initialize memory, so
//! containers using this provider should either initialize memory themselves
//! or use one of the memory.h functions.
class DefaultAllocatorProvider {
 public:
  //! Allocates uninitialized memory of the given size
  void *allocate(size_t size) {
    return memoryAlloc(size);
  }

  //! Allocates uninitialized memory for an object of type T, which may be
  //! over-aligned.
  template <typename T>
  void *allocate() {
    return allocateArray<T>(1);
  }

  //! Allocates uninitialized memory for an array of objects of type T, which
  //! may be over-aligned.
  template <typename T>
  void *allocateArray(size_t count) {
    if constexpr (alignof(T) > alignof(std::max_align_t)) {
      return memoryAlignedAllocArray<T>(count);
    } else {
      return allocate(sizeof(T) * count);
    }
  }

  //! Deallocates memory allocated by this allocator
  void deallocate(void *ptr) {
    memoryFree(ptr);
  }
};

}  // namespace chre

#endif  // CHRE_UTIL_DEFAULT_ALLOCATOR_PROVIDER_H_
