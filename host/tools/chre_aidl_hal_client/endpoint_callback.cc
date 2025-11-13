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

#include "endpoint_callback.h"
#include <iostream>

#include "nanoapp_helper.h"

namespace android::chre::chre_aidl_hal_client {

ScopedAStatus EndpointCallback::onEndpointStarted(
    const std::vector<EndpointInfo> &in_endpointInfos) {
  std::cout << "EndpointCallback::onEndpointStarted called with "
            << in_endpointInfos.size() << " endpoints." << std::endl;
  return ScopedAStatus::ok();
}

ScopedAStatus EndpointCallback::onEndpointStopped(
    const std::vector<EndpointId> &in_endpointIds, Reason in_reason) {
  std::cout << "EndpointCallback::onEndpointStopped called for "
            << in_endpointIds.size() << " endpoints. Reason: "
            << aidl::android::hardware::contexthub::toString(in_reason)
            << std::endl;
  return ScopedAStatus::ok();
}

ScopedAStatus EndpointCallback::onMessageReceived(int32_t in_sessionId,
                                                  const Message &in_msg) {
  std::cout << "EndpointCallback::onMessageReceived called for session "
            << in_sessionId << " seqNum=" << in_msg.sequenceNumber << std::endl;
  return ScopedAStatus::ok();
}

ScopedAStatus EndpointCallback::onMessageDeliveryStatusReceived(
    int32_t in_sessionId, const MessageDeliveryStatus &in_msgStatus) {
  std::cout << "EndpointCallback::onMessageDeliveryStatusReceived called for "
               "session "
            << in_sessionId << ". Seq=" << in_msgStatus.messageSequenceNumber
            << " errorCode="
            << aidl::android::hardware::contexthub::toString(
                   in_msgStatus.errorCode)
            << std::endl;  // Use toString for enum
  return ScopedAStatus::ok();
}

ScopedAStatus EndpointCallback::onEndpointSessionOpenRequest(
    int32_t in_sessionId, const EndpointId &in_destination,
    const EndpointId &in_initiator,
    const std::optional<std::string> &in_serviceDescriptor) {
  std::cout << "EndpointCallback::onEndpointSessionOpenRequest called for "
               "session "
            << in_sessionId << " from " << in_initiator.toString() << " to "
            << in_destination.toString();
  if (in_serviceDescriptor.has_value()) {
    std::cout << " with service descriptor: " << in_serviceDescriptor.value();
  }
  std::cout << std::endl;
  return ScopedAStatus::ok();
}

ScopedAStatus EndpointCallback::onCloseEndpointSession(int32_t in_sessionId,
                                                       Reason in_reason) {
  std::cout << "EndpointCallback::onCloseEndpointSession called for session "
            << in_sessionId << ". Reason: "
            << aidl::android::hardware::contexthub::toString(in_reason)
            << std::endl;  // Use toString for enum
  return ScopedAStatus::ok();
}

ScopedAStatus EndpointCallback::onEndpointSessionOpenComplete(
    int32_t in_sessionId) {
  std::cout
      << "EndpointCallback::onEndpointSessionOpenComplete called for session "
      << in_sessionId << std::endl;
  return ScopedAStatus::ok();
}

void EndpointHelper::printEndpoints(std::vector<EndpointInfo> &endpoints) {
  if (endpoints.empty()) {
    std::cout << "No endpoints found" << std::endl;
    return;
  }
  std::cout << "Found " << endpoints.size() << " endpoint(s):" << std::endl;
  for (const auto &[endpoint, type, name, version, tag, requiredPermissions,
                    services] : endpoints) {
    const std::string versionString =
        type == EndpointInfo::EndpointType::NANOAPP
            ? NanoappHelper::parseAppVersion(version)
            : std::to_string(version);
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  Hub ID:      0x" << std::hex << endpoint.hubId << std::endl;
    std::cout << "  Endpoint ID: 0x" << std::hex << endpoint.id << std::dec
              << std::endl;
    std::cout << "  Name:        " << name << std::endl;
    std::cout << "  Type:        " << toString(type) << std::endl;
    std::cout << "  Version:     " << versionString << std::endl;
    std::cout << "  Tag:         " << (tag.has_value() ? tag.value() : "<none>")
              << std::endl;

    std::cout << "  Permissions: ";
    if (requiredPermissions.empty()) {
      std::cout << "<none>" << std::endl;
    } else {
      std::cout << std::endl;
      for (const auto &perm : requiredPermissions) {
        std::cout << "    - " << perm << std::endl;
      }
    }

    std::cout << "  Services:    ";
    if (services.empty()) {
      std::cout << "<none>" << std::endl;
    } else {
      std::cout << std::endl;
      for (const auto &service : services) {
        std::cout << "    - " << service.toString() << std::endl;
      }
    }
  }
  std::cout << "----------------------------------------" << std::endl;
}

}  // namespace android::chre::chre_aidl_hal_client