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

#ifndef CHRE_PLATFORM_SHARED_HOST_PROTOCOL_CONSTS_H_
#define CHRE_PLATFORM_SHARED_HOST_PROTOCOL_CONSTS_H_

#include <stdint.h>

namespace chre {

//! On a message sent from CHRE, specifies that the host daemon should determine
//! which client to send the message to. Usually, this is all clients, but for a
//! message from a nanoapp, the host daemon can use the endpoint ID to determine
//! the destination client ID.
constexpr uint16_t kHostClientIdUnspecified = 0;

}  // namespace chre

#endif  // CHRE_PLATFORM_SHARED_HOST_PROTOCOL_CONSTS_H_
