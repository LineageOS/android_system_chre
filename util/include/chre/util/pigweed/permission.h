/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef CHRE_UTIL_PIGWEED_PERMISSION_H_
#define CHRE_UTIL_PIGWEED_PERMISSION_H_

#include <cstdint>

#include "chre/util/non_copyable.h"
#include "chre/util/optional.h"

namespace chre {

/**
 * Holds the permission for the next message sent by a server.
 */
class RpcPermission : public NonCopyable {
 public:
  /** Sets the permission for the next message. */
  void set(uint32_t permission);

  /**
   * Returns the permission for the next message and resets the value.
   *
   * @return The permission for the next message.
   */
  uint32_t getAndReset();

 private:
  /** Bitmasked CHRE_MESSAGE_PERMISSION_ */
  Optional<uint32_t> mPermission;
};

}  // namespace chre

#endif  // CHRE_UTIL_PIGWEED_PERMISSION_H_