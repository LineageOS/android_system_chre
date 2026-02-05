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

#ifndef CHRE_UTIL_SYSTEM_MESSAGE_COMMON_INFO_TYPES_H_
#define CHRE_UTIL_SYSTEM_MESSAGE_COMMON_INFO_TYPES_H_

#include "chre/util/system/message_common_types.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace chre::message {

//! Represents information about an endpoint
struct EndpointInfo {
  static constexpr size_t kMaxNameLength = 50;

  EndpointInfo(EndpointId initId, const char *initName, uint32_t initVersion,
               EndpointType initType, uint32_t initRequiredPermissions)
      : id(initId),
        version(initVersion),
        type(initType),
        requiredPermissions(initRequiredPermissions) {
    if (initName != nullptr) {
      std::strncpy(this->name, initName, kMaxNameLength);
    } else {
      this->name[0] = '\0';
    }
    this->name[kMaxNameLength] = '\0';
  }

  EndpointId id;
  char name[kMaxNameLength + 1];
  uint32_t version;
  EndpointType type;
  uint32_t requiredPermissions;

  bool operator==(const EndpointInfo &other) const {
    return id == other.id && version == other.version && type == other.type &&
           requiredPermissions == other.requiredPermissions &&
           std::strncmp(name, other.name, kMaxNameLength) == 0;
  }

  bool operator!=(const EndpointInfo &other) const {
    return !(*this == other);
  }
};

//! Represents information about a service provided by an endpoint.
struct ServiceInfo {
  ServiceInfo(const char *initServiceDescriptor, uint32_t initMajorVersion,
              uint32_t initMinorVersion, RpcFormat initFormat)
      : serviceDescriptor(initServiceDescriptor),
        majorVersion(initMajorVersion),
        minorVersion(initMinorVersion),
        format(initFormat) {}

  bool operator==(const ServiceInfo &other) const {
    if (majorVersion != other.majorVersion ||
        minorVersion != other.minorVersion || format != other.format) {
      return false;
    }

    if ((serviceDescriptor == nullptr) !=
        (other.serviceDescriptor == nullptr)) {
      return false;
    }
    if (serviceDescriptor != nullptr &&
        std::strcmp(serviceDescriptor, other.serviceDescriptor) != 0) {
      return false;
    }
    return true;
  }

  bool operator!=(const ServiceInfo &other) const {
    return !(*this == other);
  }

  //! The service descriptor, a null-terminated ASCII string. This must be valid
  //! only for the lifetime of the service iteration methods in MessageRouter.
  const char *serviceDescriptor;

  //! Version of the service.
  uint32_t majorVersion;
  uint32_t minorVersion;

  //! The format of the RPC messages sent using this service.
  RpcFormat format;
};

//! Represents information about a MessageHub
struct MessageHubInfo {
  MessageHubId id;
  const char *name;

  bool operator==(const MessageHubInfo &other) const {
    if (id != other.id) {
      return false;
    }

    if ((name == nullptr) != (other.name == nullptr)) {
      return false;
    }
    if (name != nullptr && std::strcmp(name, other.name) != 0) {
      return false;
    }
    return true;
  }

  bool operator!=(const MessageHubInfo &other) const {
    return !(*this == other);
  }
};

}  // namespace chre::message

#endif  // CHRE_UTIL_SYSTEM_MESSAGE_COMMON_INFO_TYPES_H_
