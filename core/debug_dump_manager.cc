/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "chre/core/debug_dump_manager.h"

#include <cinttypes>
#include <cstring>

#include "chre/core/event_loop_manager.h"
#include "chre/core/multi_threading_api_mutex.h"
#include "chre/core/settings.h"
#include "chre/platform/context.h"
#include "chre/platform/system_time.h"
#include "chre/util/thread_annotations.h"
#include "chre_api/chre.h"

namespace chre {

void DebugDumpManager::trigger() {
  auto callback = [](uint16_t /*type*/, void * /*data*/, void * /*extraData*/)
                      CHRE_REQUIRES(getMultiThreadingApiMutex()) {
                        EventLoopManagerSingleton::get()
                            ->getDebugDumpManager()
                            .startFullDebugDumpCollection();
                      };

  // Collect CHRE/nanoapp framework debug dumps.
  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::PerformFullDebugDump, nullptr /*data*/, callback);
}

void DebugDumpManager::appendNanoappLog(const Nanoapp &nanoapp,
                                        const char *formatStr, va_list args) {
  uint16_t instanceId = nanoapp.getInstanceId();

  // Note this check isn't exact as it's possible that the nanoapp isn't
  // handling CHRE_EVENT_DEBUG_DUMP. This approximate check is used for its low
  // complexity as it doesn't introduce any real harms.
  if (!mCollectingDebugDumps) {
    LOGW("Nanoapp instance 0x%" PRIx16
         " logging debug data while not in an active debug dump session",
         instanceId);
  } else if (formatStr != nullptr) {
    // Log nanoapp info the first time it adds debug data in this session.
    if (!mLastNanoappId.has_value() || mLastNanoappId.value() != instanceId) {
      mLastNanoappId = instanceId;
      mDebugDump.print("\n\n %s 0x%016" PRIx64 ":\n", nanoapp.getAppName(),
                       nanoapp.getAppId());
    }

    mDebugDump.printVaList(formatStr, args);
  }
}

void DebugDumpManager::collectFrameworkDebugDumps() {
  auto *eventLoopManager = EventLoopManagerSingleton::get();
  mDebugDump.print("--- CHRE Framework Debug Dump ---\n");
  mDebugDump.print("CHRE debug dump started @ ts=%" PRIu64
                   ", estimatedHostTimeOffset=%" PRId64 "\n",
                   SystemTime::getMonotonicTime().toRawNanoseconds(),
                   SystemTime::getEstimatedHostTimeOffset());
  eventLoopManager->getMemoryManager().logStateToBuffer(mDebugDump);
#ifdef CHRE_SENSORS_SUPPORT_ENABLED
  eventLoopManager->getSensorRequestManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_SENSORS_SUPPORT_ENABLED
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  eventLoopManager->getGnssManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_GNSS_SUPPORT_ENABLED
#ifdef CHRE_WIFI_SUPPORT_ENABLED
  eventLoopManager->getWifiRequestManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_WIFI_SUPPORT_ENABLED
#ifdef CHRE_WWAN_SUPPORT_ENABLED
  eventLoopManager->getWwanRequestManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_WWAN_SUPPORT_ENABLED
#ifdef CHRE_AUDIO_SUPPORT_ENABLED
  eventLoopManager->getAudioRequestManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_AUDIO_SUPPORT_ENABLED
#ifdef CHRE_BLE_SUPPORT_ENABLED
  eventLoopManager->getBleRequestManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_BLE_SUPPORT_ENABLED
  eventLoopManager->getSettingManager().logStateToBuffer(mDebugDump);
  logStateToBuffer(mDebugDump);

  appendCapabilities();
}

void DebugDumpManager::appendCapabilities() {
  mDebugDump.print("\nCHRE Capabilities: \n");
  mDebugDump.print("\tVersion: v%" PRIu32 ".%" PRIu32 ".%" PRIu32 "\n",
                   CHRE_EXTRACT_MAJOR_VERSION(chreGetVersion()),
                   CHRE_EXTRACT_MINOR_VERSION(chreGetVersion()),
                   CHRE_EXTRACT_PATCH_VERSION(chreGetVersion()));
  mDebugDump.print("\tCHRE: 0x%" PRIx32 "\n", chreGetCapabilities());
  mDebugDump.print(
      "\tBLE: 0x%" PRIx32 "\n",
      EventLoopManagerSingleton::get()->getBleCapabilitiesLocked());
  mDebugDump.print(
      "\tBLE Filter: 0x%" PRIx32 "\n",
      EventLoopManagerSingleton::get()->getBleFilterCapabilitiesLocked());
  mDebugDump.print(
      "\tWIFI: 0x%" PRIx32 "\n",
      EventLoopManagerSingleton::get()->getWifiCapabilitiesLocked());
  mDebugDump.print(
      "\tGNSS: 0x%" PRIx32 "\n",
      EventLoopManagerSingleton::get()->getGnssCapabilitiesLocked());
  mDebugDump.print(
      "\tWWAN: 0x%" PRIx32 "\n",
      EventLoopManagerSingleton::get()->getWwanCapabilitiesLocked());
}

void DebugDumpManager::startFullDebugDumpCollection() {
  if (getCurrentEventLoop() !=
      &EventLoopManagerSingleton::get()->getEventLoop()) {
    LOGE("Debug dump trigger must initiate from the main event loop");
  } else if (mCollectingDebugDumps) {
    LOGE("Cannot start debug dump while one is pending");
  } else {
    mCollectingDebugDumps = true;
    collectFrameworkDebugDumps();
    mDebugDump.print("--- End CHRE Framework Debug Dump ---\n");

    mCurrentDebugDumpEventLoopIndex = 0;
    handleEventLoopAndNanoappDebugDump();
  }
}

void DebugDumpManager::sendDebugDumps() {
  // Avoid buffer underflow when mDebugDump failed to allocate buffers.
  size_t numBuffers = mDebugDump.getBuffers().size();
  if (numBuffers > 0) {
    for (size_t i = 0; i < numBuffers - 1; i++) {
      const auto &buff = mDebugDump.getBuffers()[i];
      sendDebugDump(buff.get(), false /*complete*/);
    }
  }

  const char *debugStr =
      (numBuffers > 0) ? mDebugDump.getBuffers().back().get() : "";
  sendDebugDump(debugStr, true /*complete*/);

  // Clear current session debug dumps and release memory.
  mDebugDump.clear();
  mLastNanoappId.reset();
  mCollectingDebugDumps = false;
}

void DebugDumpManager::handleEventLoopAndNanoappDebugDump() {
  EventLoop *eventLoop = getCurrentEventLoop();
  CHRE_ASSERT(eventLoop != nullptr);

  mDebugDump.print(kEventLoopDebugDumpFormatString,
                   mCurrentDebugDumpEventLoopIndex);
  eventLoop->logStateToBuffer(mDebugDump);

  mDebugDump.print("\n--- Nanoapps on Event Loop %" PRIu32 " ---",
                   mCurrentDebugDumpEventLoopIndex);
  mLastNanoappId.reset();
  eventLoop->distributeEventSync(CHRE_EVENT_DEBUG_DUMP, nullptr /*eventData*/);

  EventLoop *nextEventLoop =
      EventLoopManagerSingleton::get()->getNextEventLoop(eventLoop);
  if (nextEventLoop != nullptr) {
    mCurrentDebugDumpEventLoopIndex++;
    auto callback =
        [](uint16_t /*type*/, void * /* data */, void * /*extraData*/)
            CHRE_REQUIRES(getMultiThreadingApiMutex()) {
              EventLoopManagerSingleton::get()
                  ->getDebugDumpManager()
                  .handleEventLoopAndNanoappDebugDump();
            };
    EventLoopManagerSingleton::get()->deferCallback(
        SystemCallbackType::PerformEventLoopAndNanoappDebugDump,
        /*data=*/nullptr, callback, nullptr /* extraData */, nextEventLoop);
  } else {  // we are done iterating through all event loops
    sendDebugDumps();
    mCurrentDebugDumpEventLoopIndex = 0;
  }
}

}  // namespace chre
