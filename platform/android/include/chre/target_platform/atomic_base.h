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

#ifndef CHRE_PLATFORM_ANDROID_ATOMIC_BASE_H_
#define CHRE_PLATFORM_ANDROID_ATOMIC_BASE_H_

#include <atomic>

#include "chre/variant/config.h"

namespace chre {

/**
 * The CHRE AP implementation of AtomicBase.
 */
template <typename AtomicType>
class AtomicBase {
 public:
  constexpr explicit AtomicBase(AtomicType startingValue)
      : mAtomic(startingValue) {}

 protected:
  //! The underlying std::atomic struct.
  std::atomic<AtomicType> mAtomic;
};

typedef AtomicBase<bool> AtomicBoolBase;
#if CHRE_ATOMIC_UINT8_ENABLED
typedef AtomicBase<uint8_t> AtomicUint8Base;
#endif  // CHRE_ATOMIC_UINT8_ENABLED
typedef AtomicBase<uint32_t> AtomicUint32Base;

}  // namespace chre

#endif  // CHRE_PLATFORM_ANDROID_ATOMIC_BASE_H_
