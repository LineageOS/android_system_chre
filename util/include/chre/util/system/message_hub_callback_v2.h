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

#ifndef CHRE_UTIL_SYSTEM_MESSAGE_HUB_CALLBACK_V2_H_
#define CHRE_UTIL_SYSTEM_MESSAGE_HUB_CALLBACK_V2_H_

#include "chre/util/system/message_hub_callback.h"

namespace chre::message {

//! The V2 callback used to register a MessageHub with the MessageRouter.
//! This callback supports all existing functionality as well as notification
//! APIs.
class MessageHubCallbackV2 : public MessageHubCallback {
 public:
  virtual ~MessageHubCallbackV2() = default;

  // TODO(b/452707307): Add notification APIs.
};

}  // namespace chre::message

#endif  // CHRE_UTIL_SYSTEM_MESSAGE_HUB_CALLBACK_V2_H_
