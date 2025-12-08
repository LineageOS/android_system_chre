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

#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED
#include "pw_bluetooth_proxy/proxy_host.h"
#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED

namespace chre {

/**
 * Performs initialization of CHRE common functionality. This involves the
 * following:
 *
 *  1. SystemTime::init()
 *  2. Construct the *Manager objects accepted in the EventLoopManager
 *     constructor.
 *  3. EventLoopManagerSingleton::init()
 */
void initCommon();

/**
 * Performs deinitialization of CHRE common functionality. This will deinit the
 * EventLoopManagerSingleton and the *Manager objects passed into it.
 */
void deinitCommon();

#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED
/**
 * Initializes the BleSocketManager in systems where BT socket offload is
 * supported.
 *
 * @param proxyHost BT ProxyHost used by the BleSocketManager
 */
void initBleSocketManager(pw::bluetooth::proxy::ProxyHost &proxyHost);
#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED

}  // namespace chre
