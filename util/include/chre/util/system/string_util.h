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

#ifndef CHRE_UTIL_SYSTEM_STRING_UTIL_H_
#define CHRE_UTIL_SYSTEM_STRING_UTIL_H_

#include "chre/platform/log.h"

#include <cstring>

namespace chre {
/**
 * Allocates memory using a provided allocation function and duplicates a
 * C-style string.
 *
 * The caller is responsible for freeing the returned memory using a
 * deallocation function compatible with the `allocator` used for allocation.
 * For example, if `chre::memoryAlloc` is used in the allocator, the caller
 * must use `chre::memoryFree` to free the memory.
 *
 * @tparam Allocator The allocator class that must have
 * @param source The null-terminated C-style string to duplicate.
 * @param allocator The allocator to use for memory allocation.
 * @return A pointer to the newly allocated null-terminated string. The content
 *         pointed to is constant. Returns `nullptr` if the source string is
 *         `nullptr` or if memory allocation fails. The caller owns the
 *         returned memory.
 */
template <typename Allocator>
const char *strdup(const char *source, Allocator &allocator) {
  if (source == nullptr) {
    return nullptr;
  }
  size_t len = strlen(source);
  // Allocate memory for the string plus the null terminator.
  char *destBuffer = static_cast<char *>(allocator.allocate(len + 1));

  if (destBuffer != nullptr) {
    // Copy the string including the null terminator.
    memcpy(destBuffer, source, len + 1);
  } else {
    LOG_OOM();
  }

  return destBuffer;
}
}  // namespace chre

#endif  // CHRE_UTIL_SYSTEM_STRING_UTIL_H_