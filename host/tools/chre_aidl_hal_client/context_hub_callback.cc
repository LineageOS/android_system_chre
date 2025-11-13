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

#include "context_hub_callback.h"
#include "chre_api/chre/version.h"

namespace android::chre::chre_aidl_hal_client {
namespace {

using internal::ToString;

// 34a3a27e-9b83-4098-b564-e83b0c28d4bb
constexpr std::array<uint8_t, 16> kUuid = {0x34, 0xa3, 0xa2, 0x7e, 0x9b, 0x83,
                                           0x40, 0x98, 0xb5, 0x64, 0xe8, 0x3b,
                                           0x0c, 0x28, 0xd4, 0xbb};

const std::string kClientName{"ChreAidlHalClient"};

std::string parseTransactionId(int32_t transactionId) {
  switch (transactionId) {
    case kLoadTransactionId:
      return "Loading";
    case kUnloadTransactionId:
      return "Unloading";
    default:
      return "Unknown";
  }
}
}  // namespace

ScopedAStatus ContextHubCallback::handleNanoappInfo(
    const std::vector<NanoappInfo> &appInfo) {
  std::cout << appInfo.size() << " nanoapps loaded" << std::endl;
  for (const NanoappInfo &app : appInfo) {
    std::cout << "appId: 0x" << std::hex << app.nanoappId << std::dec << " {"
              << "\n\tappVersion: "
              << NanoappHelper::parseAppVersion(app.nanoappVersion)
              << "\n\tenabled: " << (app.enabled ? "true" : "false")
              << "\n\tpermissions: " << ToString(app.permissions)
              << "\n\trpcServices: " << ToString(app.rpcServices) << "\n}"
              << std::endl;
  }
  resetPromise();
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHubCallback::handleContextHubMessage(
    const ContextHubMessage &message,
    const std::vector<std::string> & /*msgContentPerms*/) {
  std::cout << "Received a message!" << std::endl
            << "   From: 0x" << std::hex << message.nanoappId << std::endl
            << "     To: 0x" << static_cast<int>(message.hostEndPoint)
            << std::endl
            << "   Body: (type " << message.messageType << " size "
            << message.messageBody.size() << ") 0x";
  for (const uint8_t &data : message.messageBody) {
    std::cout << std::hex << static_cast<uint16_t>(data);
  }
  std::cout << std::endl << std::endl;
  resetPromise();
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHubCallback::handleContextHubAsyncEvent(
    AsyncEventType event) {
  std::cout << "Received async event " << toString(event) << std::endl;
  resetPromise();
  return ScopedAStatus::ok();
}

// Called after loading/unloading a nanoapp.
ScopedAStatus ContextHubCallback::handleTransactionResult(int32_t transactionId,
                                                          bool success) {
  std::cout << parseTransactionId(transactionId) << " transaction is "
            << (success ? "successful" : "failed") << std::endl;
  resetPromise();
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHubCallback::handleNanSessionRequest(
    const NanSessionRequest & /* request */) {
  resetPromise();
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHubCallback::handleMessageDeliveryStatus(
    char16_t /* hostEndPointId */,
    const MessageDeliveryStatus & /* messageDeliveryStatus */) {
  resetPromise();
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHubCallback::getUuid(std::array<uint8_t, 16> *out_uuid) {
  *out_uuid = kUuid;
  return ScopedAStatus::ok();
}

ScopedAStatus ContextHubCallback::getName(std::string *out_name) {
  *out_name = kClientName;
  return ScopedAStatus::ok();
}

void ContextHubCallback::resetPromise() {
  promise.set_value();
  promise = std::promise<void>{};
}
}  // namespace android::chre::chre_aidl_hal_client
