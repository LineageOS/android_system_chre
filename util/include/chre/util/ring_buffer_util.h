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
#include <memory>
#include <utility>

namespace chre::ring_util {

/**
 * Populates span(s) over ring buffer data from a given offset.
 *
 * @param base The base of the ring buffer.
 * @param capacity The total number of elements the ring can store.
 * @param offset The element offset from which to start the first span.
 * @param count The number of elements to capture.
 * @param span1 Span starting at offset. The span is over the range
 * [span1.first,span1.second).
 * @param span2 Span starting at base. It is only populated (i.e. span2.second
 * is only set > than base) if the ring buffer data wraps around.
 */
template <typename ElementType>
void getSpans(const ElementType *base, size_t capacity, size_t offset,
              size_t count,
              std::pair<const ElementType *, const ElementType *> &span1,
              std::pair<const ElementType *, const ElementType *> &span2) {
  auto front = base + offset;
  span1 = {front, front};
  span2 = {base, base};
  auto diff = capacity - offset;
  if (diff >= count) {
    span1.second += count;
  } else {
    span1.second += diff;
    span2.second += count - diff;
  }
}

/**
 * Copies data from a ring buffer to a contiguous uninitialized region.
 *
 * @param base The base of the ring buffer.
 * @param capacity The total number of elements the ring can store.
 * @param offset The element offset from which to start the first span.
 * @param count The number of elements to capture.
 * @param dest The destination for the copied data.
 */
template <typename ElementType>
void copyFrom(const ElementType *base, size_t capacity, size_t offset,
              size_t count, ElementType *dest) {
  auto diff = capacity - offset;
  if (count <= diff) {
    std::uninitialized_copy_n(base + offset, count, dest);
  } else {
    std::uninitialized_copy_n(base + offset, diff, dest);
    std::uninitialized_copy_n(base, count - diff, dest + diff);
  }
}

/**
 * Copies contiguous data into an uninitialized region of a ring buffer.
 *
 * @param base The base of the ring buffer.
 * @param capacity The total number of elements the ring can store.
 * @param offset The element offset from which to start the first span.
 * @param src Pointer to the buffer from which to copy.
 * @param count The number of elements to copy.
 */
template <typename ElementType>
void copyTo(ElementType *base, size_t capacity, size_t offset,
            const ElementType *src, size_t count) {
  auto dest = base + offset;
  auto diff = capacity - offset;
  if (count <= diff) {
    std::uninitialized_copy_n(src, count, dest);
  } else {
    std::uninitialized_copy_n(src, diff, dest);
    std::uninitialized_copy_n(src + diff, count - diff, base);
  }
}

}  // namespace chre::ring_util
