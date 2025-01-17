/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "chre/core/event_loop_manager.h"

#include "chre/platform/atomic.h"
#include "chre/platform/fatal_error.h"
#include "chre/platform/memory.h"
#include "chre/util/lock_guard.h"

namespace chre {

Nanoapp *EventLoopManager::validateChreApiCall(const char *functionName) {
  chre::Nanoapp *currentNanoapp =
      EventLoopManagerSingleton::get()->getEventLoop().getCurrentNanoapp();
  CHRE_ASSERT_LOG(currentNanoapp, "%s called with no CHRE app context",
                  functionName);
  return currentNanoapp;
}

uint16_t EventLoopManager::getNextInstanceId() {
  // Get the next available instance ID and mask off the upper 16 bit.
  uint16_t instanceId =
      static_cast<uint16_t>(mNextInstanceId.fetch_increment() & 0x0000FFFF);

  // 65536 instance IDs should be enough for normal use cases. If we need to
  // support wraparound for stress testing load/unload, then we can set a flag
  // when wraparound occurs and use EventLoop::findNanoappByInstanceId to ensure
  // we avoid conflicts
  if (instanceId == kBroadcastInstanceId || instanceId == kSystemInstanceId) {
    FATAL_ERROR("Exhausted instance IDs!");
  }

  return instanceId;
}

void EventLoopManager::lateInit() {
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  mSensorRequestManager.init();
#endif  // CHRE_SENSORS_SUPPORT_ENABLED

#ifdef CHRE_GNSS_SUPPORT_ENABLED
  mGnssManager.init();
#endif  // CHRE_GNSS_SUPPORT_ENABLED

#ifdef CHRE_WIFI_SUPPORT_ENABLED
  mWifiRequestManager.init();
#endif  // CHRE_WIFI_SUPPORT_ENABLED

#ifdef CHRE_WWAN_SUPPORT_ENABLED
  mWwanRequestManager.init();
#endif  // CHRE_WWAN_SUPPORT_ENABLED

#ifdef CHRE_AUDIO_SUPPORT_ENABLED
  mAudioRequestManager.init();
#endif  // CHRE_AUDIO_SUPPORT_ENABLED

#ifdef CHRE_BLE_SUPPORT_ENABLED
  mBleRequestManager.init();
#endif  // CHRE_BLE_SUPPORT_ENABLED
}

// Explicitly instantiate the EventLoopManagerSingleton to reduce codesize.
template class Singleton<EventLoopManager>;

}  // namespace chre
