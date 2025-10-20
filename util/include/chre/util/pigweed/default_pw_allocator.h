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

#include "chre/util/container_support.h"

#include "pw_allocator/allocator.h"
#include "pw_allocator/capability.h"
#include "pw_allocator/layout.h"

namespace chre {

//! A basic implementation of the pw::Allocator interface that uses the
//! existing allocation functionality provided by container_support.h.
class DefaultPwAllocator : public pw::Allocator {
 public:
  static constexpr pw::allocator::Capabilities kCapabilities = 0;

  constexpr DefaultPwAllocator() : Allocator(kCapabilities) {}

 private:
  void *DoAllocate(pw::allocator::Layout layout) override {
    return memoryAlloc(layout.size());
  }

  void DoDeallocate(void *ptr) override {
    memoryFree(ptr);
  }
};

}  // namespace chre
