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

#ifndef ANDROID_CHRE_AIDL_HAL_CLIENT_SESSION_BASED_MESSAGING_H
#define ANDROID_CHRE_AIDL_HAL_CLIENT_SESSION_BASED_MESSAGING_H

#include <vector>

#include <aidl/android/hardware/contexthub/BnContextHubCallback.h>
#include <aidl/android/hardware/contexthub/BnEndpointCallback.h>
#include <aidl/android/hardware/contexthub/ContextHubMessage.h>
#include <aidl/android/hardware/contexthub/HostEndpointInfo.h>
#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <aidl/android/hardware/contexthub/IContextHubCallback.h>
#include <aidl/android/hardware/contexthub/NanoappBinary.h>
#include <android/binder_process.h>

namespace android::chre::chre_aidl_hal_client {

using aidl::android::hardware::contexthub::AsyncEventType;
using aidl::android::hardware::contexthub::BnContextHubCallback;
using aidl::android::hardware::contexthub::BnEndpointCallback;
using aidl::android::hardware::contexthub::ContextHubInfo;
using aidl::android::hardware::contexthub::ContextHubMessage;
using aidl::android::hardware::contexthub::EndpointId;
using aidl::android::hardware::contexthub::EndpointInfo;
using aidl::android::hardware::contexthub::HostEndpointInfo;
using aidl::android::hardware::contexthub::HubInfo;
using aidl::android::hardware::contexthub::IContextHub;
using aidl::android::hardware::contexthub::IContextHubCallback;
using aidl::android::hardware::contexthub::IContextHubDefault;
using aidl::android::hardware::contexthub::IEndpointCallback;
using aidl::android::hardware::contexthub::IEndpointCallbackDefault;
using aidl::android::hardware::contexthub::IEndpointCommunication;
using aidl::android::hardware::contexthub::Message;
using aidl::android::hardware::contexthub::MessageDeliveryStatus;
using aidl::android::hardware::contexthub::NanoappBinary;
using aidl::android::hardware::contexthub::NanoappInfo;
using aidl::android::hardware::contexthub::NanSessionRequest;
using aidl::android::hardware::contexthub::Reason;
using aidl::android::hardware::contexthub::Service;
using aidl::android::hardware::contexthub::Setting;
using aidl::android::hardware::contexthub::VendorHubInfo;
using ndk::ScopedAStatus;

const VendorHubInfo kVendorHubInfo = {
    .name = "chre_aidl_hal_client_hub",
    .version = 1,
};

const HubInfo kHubInfo = {
    .hubId = 0xbeefbeef,
    .hubDetails = kVendorHubInfo,
};

class EndpointCallback : public BnEndpointCallback {
 public:
  ScopedAStatus onEndpointStarted(
      const std::vector<EndpointInfo> &in_endpointInfos) override;

  ScopedAStatus onEndpointStopped(const std::vector<EndpointId> &in_endpointIds,
                                  Reason in_reason) override;

  ScopedAStatus onMessageReceived(int32_t in_sessionId,
                                  const Message &in_msg) override;

  ScopedAStatus onMessageDeliveryStatusReceived(
      int32_t in_sessionId, const MessageDeliveryStatus &in_msgStatus) override;

  ScopedAStatus onEndpointSessionOpenRequest(
      int32_t in_sessionId, const EndpointId &in_destination,
      const EndpointId &in_initiator,
      const std::optional<std::string> &in_serviceDescriptor) override;

  ScopedAStatus onCloseEndpointSession(int32_t in_sessionId,
                                       Reason in_reason) override;

  ScopedAStatus onEndpointSessionOpenComplete(int32_t in_sessionId) override;
};

class EndpointHelper {
 public:
  static void printEndpoints(std::vector<EndpointInfo> &endpoints);
};

}  // namespace android::chre::chre_aidl_hal_client

#endif  // ANDROID_CHRE_AIDL_HAL_CLIENT_SESSION_BASED_MESSAGING_H