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

#ifndef CHRE_PLATFORM_ANDROID_HOST_LINK_BASE_H_
#define CHRE_PLATFORM_ANDROID_HOST_LINK_BASE_H_

#include <functional>

namespace chre {

class HostLinkBase {
 public:
  using MessageCallback =
      std::function<void(int64_t nanoAppId, int32_t messageType,
                         void *messageBody, size_t messageBodyLen)>;

  /**
   * Enqueues a NAN configuration request to be sent to the host.
   * For CHRE AP, the request is simply echoed back via a NAN configuration
   * update event since there's no actual host to send the request to.
   *
   * @param enable Requests that NAN be enabled or disabled based on the
   *        boolean's value.
   */
  void sendNanConfiguration(bool enable);

  /**
   * Registers a callback function to handle messages from nanoapp to host.
   *
   * @param callback The call back function.
   */
  void registerMessageCallback(const MessageCallback &callback);

 protected:
  MessageCallback mMessageCallback = nullptr;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_ANDROID_HOST_LINK_BASE_H_
