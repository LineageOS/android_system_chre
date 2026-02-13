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

#include <atomic>

namespace chre {

/** Implementation of AtomicUint32RefBase using std::atomic_ref. */
class AtomicUint32RefBase {
 public:
  explicit AtomicUint32RefBase(uint32_t &object) : mAtomic(object) {}

 protected:
  //! The underlying std::atomic_ref.
  std::atomic_ref<uint32_t> mAtomic;
};

}  // namespace chre
