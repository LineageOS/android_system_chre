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

#ifndef ANDROID_CHRE_AIDL_HAL_CLIENT_CONTEXT_HUB_CALLBACK_H
#define ANDROID_CHRE_AIDL_HAL_CLIENT_CONTEXT_HUB_CALLBACK_H

#include <aidl/android/hardware/contexthub/BnContextHubCallback.h>
#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <future>

#include "nanoapp_helper.h"

namespace android::chre::chre_aidl_hal_client {

using aidl::android::hardware::contexthub::AsyncEventType;
using aidl::android::hardware::contexthub::BnContextHubCallback;
using aidl::android::hardware::contexthub::ContextHubInfo;
using aidl::android::hardware::contexthub::ContextHubMessage;
using aidl::android::hardware::contexthub::HostEndpointInfo;
using aidl::android::hardware::contexthub::IContextHub;
using aidl::android::hardware::contexthub::MessageDeliveryStatus;
using aidl::android::hardware::contexthub::NanoappBinary;
using aidl::android::hardware::contexthub::NanoappInfo;
using aidl::android::hardware::contexthub::NanSessionRequest;
using aidl::android::hardware::contexthub::Setting;
using ndk::ScopedAStatus;

/** Default Context Hub ID used for commands when not specified otherwise. */
constexpr uint32_t kContextHubId = 0;

/** Transaction ID used for nanoapp load operations. */
constexpr int32_t kLoadTransactionId = 1;

/** Transaction ID used for nanoapp unload operations. */
constexpr int32_t kUnloadTransactionId = 2;

/**
 * Timeout threshold for HAL operations like load/unload.
 *
 * Although the AIDL definition specifies a 30s cap, the multiclient HAL
 * might enforce a shorter timeout (e.g., 5s) to prevent blocking other clients.
 */
constexpr auto kTimeOutThresholdInSec = std::chrono::seconds(5);

/**
 * Implements the IContextHubCallback AIDL interface to receive asynchronous
 * responses and events from the Context Hub HAL.
 *
 * This class handles callbacks related to nanoapp information, messages,
 * transaction results, and other events. It uses a std::promise to signal
 * the main thread when a callback is received.
 */
class ContextHubCallback final : public BnContextHubCallback {
 public:
  /**
   * See IContextHubCallback.aidl#handleNanoappInfo.
   */
  ScopedAStatus handleNanoappInfo(
      const std::vector<NanoappInfo> &appInfo) override;

  /**
   * See IContextHubCallback.aidl#handleContextHubMessage.
   */
  ScopedAStatus handleContextHubMessage(
      const ContextHubMessage &message,
      const std::vector<std::string> & /*msgContentPerms*/) override;

  /**
   * See IContextHubCallback.aidl#handleContextHubAsyncEvent.
   */
  ScopedAStatus handleContextHubAsyncEvent(AsyncEventType event) override;

  /**
   * See IContextHubCallback.aidl#handleTransactionResult.
   */
  ScopedAStatus handleTransactionResult(int32_t transactionId,
                                        bool success) override;

  /**
   * See IContextHubCallback.aidl#handleNanSessionRequest.
   */
  ScopedAStatus handleNanSessionRequest(
      const NanSessionRequest & /* request */) override;

  /**
   * See IContextHubCallback.aidl#handleMessageDeliveryStatus.
   */
  ScopedAStatus handleMessageDeliveryStatus(
      char16_t /* hostEndPointId */,
      const MessageDeliveryStatus & /* messageDeliveryStatus */) override;

  /**
   * See IContextHubCallback.aidl#getUuid.
   */
  ScopedAStatus getUuid(std::array<uint8_t, 16> *out_uuid) override;

  /**
   * See IContextHubCallback.aidl#getName.
   */
  ScopedAStatus getName(std::string *out_name) override;

  /**
   * Resets the internal promise, allowing the main thread to wait for the next
   * callback.
   */
  void resetPromise();

  /**
   * A promise used to signal the main thread when a callback is received.
   *
   * TODO(b/247124878):
   * This promise is shared among all the HAL callbacks to simplify the
   * implementation. This is based on the assumption that every command should
   * get a response before timeout and the first callback triggered is for the
   * response.
   *
   * In very rare cases, however, the assumption doesn't hold:
   *  - multiple callbacks are triggered by a command and come back out of order
   *  - one command is timed out and the user typed in another command then the
   *  first callback for the first command is triggered
   * Once we have a chance we should consider refactor this design to let each
   * callback use their specific promises.
   */
  std::promise<void> promise;
};
}  // namespace android::chre::chre_aidl_hal_client

#endif  // ANDROID_CHRE_AIDL_HAL_CLIENT_CONTEXT_HUB_CALLBACK_H