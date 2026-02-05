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

#include "chre/platform/shared/init.h"

#include <optional>

#include "chre/core/event_loop_manager.h"
#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "chre/platform/system_time.h"
#include "chre/platform/version.h"
#include "chre/util/system/message_router.h"

/**
 * @file This file provides a shareable example of the instantiation of manager
 * objects and early initialization of CHRE. Platform implementations are not
 * required to use it, and can instead choose to supply their own implementation
 * of this functionality, but it serves as a reference in any case.
 *
 * When using this file, *Manager objects can be placed in a high power memory
 * regions by defining the corresponding CHRE_*_MEMORY_REGION macros as the
 * CHRE_HIGH_POWER_BSS_ATTRIBUTE. For platforms that do not support different
 * memory regions, these macros will not do anything.
 *
 * Platforms using this file should perform initialization in this order:
 *
 *  1. Initialize CHRE logging
 *  2. initBleSocketManager() (if CHRE_BLE_SOCKET_SUPPORT_ENABLED is true)
 *  3. initCommon()
 *  4. Start the thread that will run the EventLoop
 *
 * After this point, it is safe for other threads to access CHRE, e.g. incoming
 * requests from the host can be posted to the EventLoop. Then within the CHRE
 * thread:
 *
 *  5. EventLoopManager::lateInit() (this typically involves blocking on
 *     readiness of other subsystems as part of PAL initialization)
 *  6. loadStaticNanoapps()
 *  7. EventLoopManagerSingleton::get()->getEventLoop().run()
 *
 * Platforms may also perform additional platform-specific initialization steps
 * at any point along the way as needed.
 */

using ::chre::message::MessageRouter;
using ::chre::message::MessageRouterSingleton;
using ::chre::message::Session;
using ::chre::message::SessionId;

namespace chre {

namespace {

#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED
#ifndef CHRE_BLE_SOCKET_MEMORY_REGION
#define CHRE_BLE_SOCKET_MEMORY_REGION
#endif  // CHRE_BLE_SOCKET_MEMORY_REGION

CHRE_BLE_SOCKET_MEMORY_REGION
std::optional<BleSocketManager> gBleSocketManager;
#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED

#ifdef CHRE_GNSS_SUPPORT_ENABLED
#ifndef CHRE_GNSS_MEMORY_REGION
#define CHRE_GNSS_MEMORY_REGION
#endif  // CHRE_GNSS_MEMORY_REGION

CHRE_GNSS_MEMORY_REGION
std::optional<GnssManager> gGnssManager;
#endif  // CHRE_GNSS_SUPPORT_ENABLED

#ifdef CHRE_WIFI_SUPPORT_ENABLED
#ifndef CHRE_WIFI_MEMORY_REGION
#define CHRE_WIFI_MEMORY_REGION
#endif  // CHRE_WIFI_MEMORY_REGION

CHRE_WIFI_MEMORY_REGION
std::optional<WifiRequestManager> gWifiRequestManager;
#endif  // CHRE_WIFI_SUPPORT_ENABLED

#ifdef CHRE_WWAN_SUPPORT_ENABLED
#ifndef CHRE_WWAN_MEMORY_REGION
#define CHRE_WWAN_MEMORY_REGION
#endif  // CHRE_WWAN_MEMORY_REGION

CHRE_WWAN_MEMORY_REGION
std::optional<WwanRequestManager> gWwanRequestManager;
#endif  // CHRE_WWAN_SUPPORT_ENABLED

#ifdef CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED

#ifndef CHRE_MESSAGE_ROUTER_MAX_MESSAGE_HUBS
#error "CHRE_MESSAGE_ROUTER_MAX_MESSAGE_HUBS must be defined"
#endif  // CHRE_MESSAGE_ROUTER_MAX_MESSAGE_HUBS
#ifndef CHRE_MESSAGE_ROUTER_MAX_SESSIONS
#error "CHRE_MESSAGE_ROUTER_MAX_SESSIONS must be defined"
#endif  // CHRE_MESSAGE_ROUTER_MAX_SESSIONS
#ifndef CHRE_MESSAGE_ROUTER_RESERVED_SESSION_ID
#error "CHRE_MESSAGE_ROUTER_RESERVED_SESSION_ID must be defined"
#endif  // CHRE_MESSAGE_ROUTER_RESERVED_SESSION_ID

#ifndef CHRE_MESSAGE_ROUTER_MEMORY_REGION
#define CHRE_MESSAGE_ROUTER_MEMORY_REGION
#endif  // CHRE_MESSAGE_ROUTER_MEMORY_REGION

constexpr size_t kMaxMessageHubs = CHRE_MESSAGE_ROUTER_MAX_MESSAGE_HUBS;
constexpr size_t kMaxSessions = CHRE_MESSAGE_ROUTER_MAX_SESSIONS;
constexpr SessionId kReservedSessionId =
    CHRE_MESSAGE_ROUTER_RESERVED_SESSION_ID;

CHRE_MESSAGE_ROUTER_MEMORY_REGION
pw::Vector<MessageRouter::MessageHubRecord, kMaxMessageHubs> gMessageHubs;

CHRE_MESSAGE_ROUTER_MEMORY_REGION
pw::Vector<Session, kMaxSessions> gSessions;

CHRE_MESSAGE_ROUTER_MEMORY_REGION
std::optional<ChreMessageHubManager> gChreMessageHubManager;

CHRE_MESSAGE_ROUTER_MEMORY_REGION
std::optional<HostMessageHubManager> gHostMessageHubManager;

#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED

#ifndef CHRE_DATA_FLOW_MEMORY_REGION
#define CHRE_DATA_FLOW_MEMORY_REGION
#endif  // CHRE_DATA_FLOW_MEMORY_REGION

CHRE_DATA_FLOW_MEMORY_REGION
std::optional<DataFlowManager> gDataFlowManager;

#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED

#else  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED

#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
#error \
    "CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED must be defined to use CHRE_DATA_FLOW_SUPPORT_ENABLED"
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED

#endif  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED

std::optional<EventLoop> gEventLoop;

static const char *kChreVersionString = chre::getChreVersionString();

BleSocketManager *getBleSocketManager() {
#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED
  CHRE_ASSERT_LOG(gBleSocketManager.has_value(),
                  "Initialized EventLoopManager before BleSocketManager");
  return &gBleSocketManager.value();
#else
  return nullptr;
#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED
}

void deinitBleSocketManager() {
#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED
  gBleSocketManager.reset();
#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED
}

GnssManager *initAndGetGnssManager() {
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  gGnssManager.emplace();
  return &gGnssManager.value();
#else
  return nullptr;
#endif  // CHRE_GNSS_SUPPORT_ENABLED
}

void deinitGnssManager() {
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  gGnssManager.reset();
#endif  // CHRE_GNSS_SUPPORT_ENABLED
}

WifiRequestManager *initAndGetWifiRequestManager() {
#ifdef CHRE_WIFI_SUPPORT_ENABLED
  gWifiRequestManager.emplace();
  return &gWifiRequestManager.value();
#else
  return nullptr;
#endif  // CHRE_WIFI_SUPPORT_ENABLED
}

void deinitWifiRequestManager() {
#ifdef CHRE_WIFI_SUPPORT_ENABLED
  gWifiRequestManager.reset();
#endif  // CHRE_WIFI_SUPPORT_ENABLED
}

WwanRequestManager *initAndGetWwanRequestManager() {
#ifdef CHRE_WWAN_SUPPORT_ENABLED
  gWwanRequestManager.emplace();
  return &gWwanRequestManager.value();
#else
  return nullptr;
#endif  // CHRE_WWAN_SUPPORT_ENABLED
}

void deinitWwanRequestManager() {
#ifdef CHRE_WWAN_SUPPORT_ENABLED
  gWwanRequestManager.reset();
#endif  // CHRE_WWAN_SUPPORT_ENABLED
}

ChreMessageHubManager *initAndGetChreMessageHubManager() {
#ifdef CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
  gChreMessageHubManager.emplace();
  return &gChreMessageHubManager.value();
#else
  return nullptr;
#endif  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
}

void deinitChreMessageHubManager() {
#ifdef CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
  gChreMessageHubManager.reset();
#endif  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
}

HostMessageHubManager *initAndGetHostMessageHubManager() {
#ifdef CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
  gHostMessageHubManager.emplace();
  return &gHostMessageHubManager.value();
#else
  return nullptr;
#endif  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
}

void deinitHostMessageHubManager() {
#ifdef CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
  gHostMessageHubManager.reset();
#endif  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
}

void initMessageRouter() {
#ifdef CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
  MessageRouterSingleton::init(gMessageHubs, gSessions, kReservedSessionId);
#endif  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
}

void deinitMessageRouter() {
#ifdef CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
  MessageRouterSingleton::deinit();
#endif  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
}

DataFlowManager *initAndGetDataFlowManager() {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  gDataFlowManager.emplace();
  return &gDataFlowManager.value();
#else
  return nullptr;
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

void deinitDataFlowManager() {
#ifdef CHRE_DATA_FLOW_SUPPORT_ENABLED
  gDataFlowManager.reset();
#endif  // CHRE_DATA_FLOW_SUPPORT_ENABLED
}

}  // namespace

void initCommon() {
  gEventLoop.emplace();
  pw::span<EventLoop> span(&gEventLoop.value(), 1);
  initCommon(span);
}

void initCommon(pw::span<EventLoop> eventLoops) {
  LOGI("CHRE init, version: %s", kChreVersionString);

  SystemTime::init();

  initMessageRouter();

  EventLoopManagerSingleton::init(
      eventLoops, getBleSocketManager(), initAndGetGnssManager(),
      initAndGetWifiRequestManager(), initAndGetWwanRequestManager(),
      initAndGetChreMessageHubManager(), initAndGetHostMessageHubManager(),
      initAndGetDataFlowManager());
}

void deinitCommon() {
  deinitBleSocketManager();
  deinitGnssManager();
  deinitWifiRequestManager();
  deinitWwanRequestManager();
  deinitChreMessageHubManager();
  deinitHostMessageHubManager();
  deinitDataFlowManager();

  // The event loop manager must be deinitialized after other managers to avoid
  // issues in case manager destructors reference the singleton.
  EventLoopManagerSingleton::deinit();
  deinitMessageRouter();
  gEventLoop.reset();
  LOGD("CHRE deinit");
}

#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED
void initBleSocketManager(pw::bluetooth::proxy::ProxyHost &proxyHost) {
  gBleSocketManager.emplace(proxyHost);
}
#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED

}  // namespace chre
