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

#include "chre/util/ring_buffer_util.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "chre/util/raw_storage.h"

namespace chre {
namespace {

const uint8_t uint8Ring[] = {2, 3, 0, 1};
const size_t uint8RingOffset = 2;
const uint8_t uint8RingData[] = {0, 1, 2, 3};

const std::vector<std::shared_ptr<uint8_t>> &sharedPtrRing() {
  static auto *vec = new std::vector<std::shared_ptr<uint8_t>>(
      {std::make_shared<uint8_t>(2), std::make_shared<uint8_t>(3),
       std::make_shared<uint8_t>(0), std::make_shared<uint8_t>(1)});
  return *vec;
}
const size_t sharedPtrRingOffset = 2;
const std::vector<std::shared_ptr<uint8_t>> &sharedPtrRingData() {
  static auto *vec = new std::vector<std::shared_ptr<uint8_t>>(
      {sharedPtrRing()[2], sharedPtrRing()[3], sharedPtrRing()[0],
       sharedPtrRing()[1]});
  return *vec;
}

TEST(RingBufferUtilTest, GetSpansUint8NoWrap) {
  std::pair<const uint8_t *, const uint8_t *> span1, span2;
  const size_t kCount = 1;
  ring_util::getSpans(uint8Ring, std::size(uint8Ring), uint8RingOffset, kCount,
                      span1, span2);
  EXPECT_EQ(span1.first, uint8Ring + uint8RingOffset);
  EXPECT_EQ(span1.second, uint8Ring + uint8RingOffset + kCount);
  EXPECT_EQ(span2.first, span2.second);
}

TEST(RingBufferUtilTest, GetSpansSharedPtrNoWrap) {
  std::pair<const std::shared_ptr<uint8_t> *, const std::shared_ptr<uint8_t> *>
      span1, span2;
  const size_t kCount = 1;
  ring_util::getSpans(sharedPtrRing().data(), sharedPtrRing().size(),
                      sharedPtrRingOffset, kCount, span1, span2);
  EXPECT_EQ(span1.first, sharedPtrRing().data() + sharedPtrRingOffset);
  EXPECT_EQ(span1.second,
            sharedPtrRing().data() + sharedPtrRingOffset + kCount);
  EXPECT_EQ(span2.first, span2.second);
}

TEST(RingBufferUtilTest, GetSpansUint8Wrap) {
  std::pair<const uint8_t *, const uint8_t *> span1, span2;
  const size_t kCount = std::size(uint8Ring);
  ring_util::getSpans(uint8Ring, std::size(uint8Ring), uint8RingOffset, kCount,
                      span1, span2);
  EXPECT_EQ(span1.first, uint8Ring + uint8RingOffset);
  EXPECT_EQ(span1.second, uint8Ring + std::size(uint8Ring));
  EXPECT_EQ(span2.first, uint8Ring);
  EXPECT_EQ(span2.second, uint8Ring + uint8RingOffset);
}

TEST(RingBufferUtilTest, GetSpansSharedPtrWrap) {
  std::pair<const std::shared_ptr<uint8_t> *, const std::shared_ptr<uint8_t> *>
      span1, span2;
  ring_util::getSpans(sharedPtrRing().data(), sharedPtrRing().size(),
                      sharedPtrRingOffset, sharedPtrRing().size(), span1,
                      span2);
  EXPECT_EQ(span1.first, sharedPtrRing().data() + sharedPtrRingOffset);
  EXPECT_EQ(span1.second, sharedPtrRing().data() + sharedPtrRing().size());
  EXPECT_EQ(span2.first, sharedPtrRing().data());
  EXPECT_EQ(span2.second, sharedPtrRing().data() + sharedPtrRingOffset);
}

TEST(RingBufferUtilTest, CopyFromUint8) {
  uint8_t buf[std::size(uint8Ring)];
  ring_util::copyFrom(uint8Ring, std::size(uint8Ring), uint8RingOffset,
                      std::size(uint8Ring), buf);
  EXPECT_EQ(std::memcmp(buf, uint8RingData, sizeof(uint8RingData)), 0);
}

TEST(RingBufferUtilTest, CopyFromSharedPtr) {
  RawStorage<std::shared_ptr<uint8_t>, 4> buf;
  ring_util::copyFrom(sharedPtrRing().data(), sharedPtrRing().size(),
                      sharedPtrRingOffset, sharedPtrRing().size(), buf.data());
  for (auto i = 0; i < buf.capacity(); ++i) {
    EXPECT_EQ(buf[i], sharedPtrRingData()[i]);
  }
}

TEST(RingBufferUtilTest, CopyToUint8) {
  uint8_t ring[std::size(uint8Ring)];
  ring_util::copyTo(ring, std::size(ring), uint8RingOffset, uint8RingData,
                    std::size(uint8RingData));
  EXPECT_EQ(std::memcmp(ring, uint8Ring, sizeof(uint8Ring)), 0);
}

TEST(RingBufferUtilTest, CopyToSharedPtr) {
  RawStorage<std::shared_ptr<uint8_t>, 4> ring;
  ring_util::copyTo(ring.data(), ring.capacity(), sharedPtrRingOffset,
                    sharedPtrRingData().data(), sharedPtrRingData().size());
  for (auto i = 0; i < ring.capacity(); ++i) {
    EXPECT_EQ(ring[i], sharedPtrRing()[i]);
  }
}

}  // namespace
}  // namespace chre
