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

#pragma once

#include "pw_bluetooth_proxy/proxy_host.h"
#include "pw_bluetooth_proxy/rfcomm/rfcomm_manager.h"

namespace chre {

class PlatformBtSocketResourcesBase {
 public:
  PlatformBtSocketResourcesBase(
      pw::bluetooth::proxy::ProxyHost &proxyHost,
      pw::bluetooth::proxy::rfcomm::RfcommManager &rfcommProxyHost)
      : mProxyHost(proxyHost), mRfcommProxyHost(rfcommProxyHost) {}

  pw::bluetooth::proxy::ProxyHost &getProxyHost() {
    return mProxyHost;
  }

  pw::bluetooth::proxy::rfcomm::RfcommManager &getRfcommProxyHost() {
    return mRfcommProxyHost;
  }

 protected:
  uint32_t mLeCocMtu = 2048;
  uint32_t mRfcommMaxFrameSize = 1024;

 private:
  pw::bluetooth::proxy::ProxyHost &mProxyHost;
  pw::bluetooth::proxy::rfcomm::RfcommManager &mRfcommProxyHost;
};

}  // namespace chre
