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
  EventLoop *eventLoop = getCurrentEventLoop();
  if (eventLoop == nullptr) {
    FATAL_ERROR("No event loop found!");
  }

  CHRE_ASSERT(mEventLoops.size() < NanoappInstanceId::kMaxEventLoopIndex);
  for (size_t i = 0; i < mEventLoops.size(); ++i) {
    if (&mEventLoops[i] == eventLoop) {
      NanoappInstanceId nanoappInstanceId;
      nanoappInstanceId.instanceId = eventLoop->getNextNanoappInstanceId();
      nanoappInstanceId.eventLoopIndex = static_cast<uint16_t>(i);
      return nanoappInstanceId.instanceIdAndEventLoopIndex;
    }
  }

  FATAL_ERROR("Invalid event loop");
  return 0;
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

  if (isBroadcast) {
    for (auto &loop : mEventLoops) {
      eventPosted |= postEventToLoop(loop, event, isLowPriority);
    }
  } else {
    NanoappInstanceId nanoappInstanceId;
    nanoappInstanceId.instanceIdAndEventLoopIndex = targetInstanceId;
    size_t eventLoopIndex = nanoappInstanceId.eventLoopIndex;

    if (eventLoopIndex < mEventLoops.size()) {
      auto &loop = mEventLoops[eventLoopIndex];
      eventPosted = postEventToLoop(loop, event, isLowPriority);
    } else {
      LOGE("Invalid event loop index %zu for instanceId 0x%" PRIx16,
           eventLoopIndex, targetInstanceId);
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

bool EventLoopManager::postEventToLoop(EventLoop &loop, Event *event,
                                       bool isLowPriority) {
  bool eventPosted = loop.postEvent(event, isLowPriority);
  if (!eventPosted && event != nullptr) {
    event->decrementRefCount();
  }
  return eventPosted;
}

// Explicitly instantiate the EventLoopManagerSingleton to reduce codesize.
template class Singleton<EventLoopManager>;

}  // namespace chre
