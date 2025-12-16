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

#include "chre/event.h"
#include "chre/platform/atomic.h"
#include "chre/platform/context.h"
#include "chre/platform/fatal_error.h"
#include "chre/platform/memory.h"
#include "chre/util/lock_guard.h"
#include "chre/util/system/system_callback_type.h"

namespace chre {

Nanoapp *EventLoopManager::validateChreApiCall(const char *functionName) {
  EventLoop *eventLoop = getCurrentEventLoop();
  CHRE_ASSERT_LOG(eventLoop, "%s called with no CHRE context", functionName);

  chre::Nanoapp *currentNanoapp = eventLoop->getCurrentNanoapp();
  CHRE_ASSERT_LOG(currentNanoapp, "%s called with no CHRE app context",
                  functionName);
  return currentNanoapp;
}

bool EventLoopManager::inEventLoopForNanoapp(uint64_t appId) {
  EventLoop *eventLoop = getCurrentEventLoop();
  if (!eventLoop) {
    return false;
  }

  uint16_t instanceId;
  return eventLoop->findNanoappInstanceIdByAppId(appId, &instanceId);
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

// TODO(b/264108686): Refactor this function and postSystemEvent
void EventLoopManager::postEventOrDie(uint16_t eventType, void *eventData,
                                      chreEventCompleteFunction *freeCallback,
                                      uint16_t targetInstanceId,
                                      uint16_t targetGroupMask) {
  postEvent(eventType, eventData, freeCallback, /* isLowPriority = */ false,
            kSystemInstanceId, targetInstanceId, targetGroupMask);
}

bool EventLoopManager::postSystemEvent(uint16_t eventType, void *eventData,
                                       SystemEventCallbackFunction *callback,
                                       void *extraData, EventLoop *eventLoop) {
  if (eventLoop == nullptr) {
    eventLoop = &getEventLoop();
  }
  if (!eventLoop->isRunning()) {
    return false;
  }
  Event *event = mEventPool.allocate(eventType, eventData, callback, extraData);
  if (!eventLoop->postEvent(event)) {
    FATAL_ERROR("Failed to post critical system event 0x%" PRIx16,
                event->eventType);
  }
  return true;
}

bool EventLoopManager::postLowPriorityEventOrFree(
    uint16_t eventType, void *eventData,
    chreEventCompleteFunction *freeCallback, uint16_t senderInstanceId,
    uint16_t targetInstanceId, uint16_t targetGroupMask) {
  return postEvent(eventType, eventData, freeCallback,
                   /* isLowPriority = */ true, senderInstanceId,
                   targetInstanceId, targetGroupMask);
}

bool EventLoopManager::postEvent(uint16_t eventType, void *eventData,
                                 chreEventCompleteFunction *freeCallback,
                                 bool isLowPriority, uint16_t senderInstanceId,
                                 uint16_t targetInstanceId,
                                 uint16_t targetGroupMask) {
  bool eventPosted = false;
  bool isBroadcast = targetInstanceId == kBroadcastInstanceId;
  uint8_t initialRefCount =
      isBroadcast ? static_cast<uint8_t>(mEventLoops.size()) : 1;
  Event *event = mEventPool.allocate(
      eventType, eventData, freeCallback, isLowPriority, senderInstanceId,
      targetInstanceId, targetGroupMask, initialRefCount);
  for (auto &loop : mEventLoops) {
    // TODO(b/435246073): This lookup can be a hot path. Consider optimizing
    // this by encoding the event loop ID within the instance ID to avoid
    // iterating through all event loops.
    if (isBroadcast ||
        loop.findNanoappByInstanceId(targetInstanceId) != nullptr) {
      if (!loop.isRunning()) continue;
      bool success = loop.postEvent(event, isLowPriority);
      if (!success && !isLowPriority) {
        FATAL_ERROR("Failed to post critical system event 0x%" PRIx16,
                    event->eventType);
      }
      if (!success && event != nullptr) {
        event->decrementRefCount();
      }
      eventPosted |= success;
    }
  }

  if (!eventPosted) {
    if (freeCallback != nullptr) {
      freeCallback(eventType, eventData);
    }
    if (event != nullptr) {
      mEventPool.deallocate(event);
    }
    return false;
  }

  return true;
}

void EventLoopManager::lateInit() {
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  mSensorRequestManager.init();
#endif  // CHRE_SENSORS_SUPPORT_ENABLED

#ifdef CHRE_GNSS_SUPPORT_ENABLED
  mGnssManager->init();
#endif  // CHRE_GNSS_SUPPORT_ENABLED

#ifdef CHRE_WIFI_SUPPORT_ENABLED
  mWifiRequestManager->init();
#endif  // CHRE_WIFI_SUPPORT_ENABLED

#ifdef CHRE_WWAN_SUPPORT_ENABLED
  mWwanRequestManager->init();
#endif  // CHRE_WWAN_SUPPORT_ENABLED

#ifdef CHRE_AUDIO_SUPPORT_ENABLED
  mAudioRequestManager.init();
#endif  // CHRE_AUDIO_SUPPORT_ENABLED

#ifdef CHRE_BLE_SUPPORT_ENABLED
  mBleRequestManager.init();
#endif  // CHRE_BLE_SUPPORT_ENABLED

#ifdef CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
  mChreMessageHubManager->init();
#endif  // CHRE_MESSAGE_ROUTER_SUPPORT_ENABLED
}

// Explicitly instantiate the EventLoopManagerSingleton to reduce codesize.
template class Singleton<EventLoopManager>;

}  // namespace chre
