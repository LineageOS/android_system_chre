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

#include <cstdlib>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "chre/util/pigweed/pw_allocator_provider.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"

using ::pw::allocator::Layout;
using ::testing::_;

namespace chre {
namespace {

// A simple allocator that can be used to check that the allocator is being
// called when expected.
class TestAllocator : public pw::Allocator {
 public:
  TestAllocator() {
    ON_CALL(*this, DoAllocate(_)).WillByDefault([](Layout layout) {
      return malloc(layout.size());
    });
    ON_CALL(*this, DoDeallocate(_)).WillByDefault([](void *ptr) { free(ptr); });
  }

  MOCK_METHOD(void *, DoAllocate, (Layout layout), (override));
  MOCK_METHOD(void, DoDeallocate, (void *ptr), (override));
};

TEST(pwAllocatorProviderTest, TestAllocate) {
  TestAllocator allocator;
  PwAllocatorProvider provider(allocator);

  EXPECT_CALL(allocator, DoAllocate(Layout(10)));
  void *ptr = provider.allocate(10);
  EXPECT_NE(ptr, nullptr);

  EXPECT_CALL(allocator, DoDeallocate(ptr));
  provider.deallocate(ptr);
}

}  // namespace
}  // namespace chre
