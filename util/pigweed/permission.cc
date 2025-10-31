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

#include "chre/util/pigweed/permission.h"

#include "chre/util/nanoapp/assert.h"
#include "chre_api/chre.h"

namespace chre {

void RpcPermission::set(uint32_t permission) {
  mPermission = permission;
}

uint32_t RpcPermission::getAndReset() {
  CHRE_ASSERT(mPermission.has_value());
  uint32_t permission = mPermission.has_value() ? mPermission.value()
                                                : CHRE_MESSAGE_PERMISSION_NONE;
  mPermission.reset();
  return permission;
}

}  // namespace chre
