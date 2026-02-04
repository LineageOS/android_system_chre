/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "chre/core/event_loop.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/multi_threading_api_mutex.h"
#include "chre/platform/context.h"
#include "chre/platform/shared/host_protocol_chre.h"
#include "chre/platform/shared/nanoapp_load_manager.h"
#include "chre/variant/config.h"

namespace chre {

NanoappLoadManager gLoadManager;

inline NanoappLoadManager &getLoadManager() {
  return gLoadManager;
}

void HostMessageHandlers::handleDebugConfiguration(
    const fbs::DebugConfiguration *debugConfiguration) {
  EventLoopManagerSingleton::get()
      ->getSystemHealthMonitor()
      .setFatalErrorOnCheckFailure(
          debugConfiguration->health_monitor_failure_crash());
}

void HostMessageHandlers::startNanoappCallback(
    SystemCallbackType /*type*/,
    UniquePtr<HostMessageHandlers::LoadNanoappCallbackData> &&cbData) {
  EventLoop *eventLoop = getCurrentEventLoop();
  CHRE_ASSERT(eventLoop != nullptr);

  // The global API mutex must always be unlocked prior to calling into
  // nanoapp entrypoint functions to prevent deadlocks.
  auto *mutex = getMultiThreadingApiMutex();
  mutex->unlock();
  bool success = eventLoop->startNanoapp(std::move(cbData->nanoapp));
  mutex->lock();

  if (cbData->sendFragmentResponse) {
    HostMessageHandlers::sendFragmentResponse(cbData->hostClientId,
                                              cbData->transactionId,
                                              cbData->fragmentId, success);
  }
}

#if CHRE_PLATFORM_OPEN_NANOAPP_ENABLED
void HostMessageHandlers::finishLoadingNanoappCallback(
    SystemCallbackType type, UniquePtr<LoadNanoappCallbackData> &&cbData) {
  EventLoop *currentEventLoop = getCurrentEventLoop();
  CHRE_ASSERT(currentEventLoop != nullptr);
  CHRE_ASSERT(cbData != nullptr);
  if (!cbData->nanoapp->isLoaded() || !cbData->nanoapp->openNanoapp()) {
    LOGE("Nanoapp is not loaded or failed to open");
    if (cbData->sendFragmentResponse) {
      HostMessageHandlers::sendFragmentResponse(
          cbData->hostClientId, cbData->transactionId, cbData->fragmentId,
          /* success= */ false);
    }
  } else {
    EventLoop *eventLoop = cbData->getEventLoopFn(cbData->nanoapp.get());
    if (currentEventLoop == eventLoop) {
      startNanoappCallback(static_cast<SystemCallbackType>(type),
                           std::move(cbData));
    } else {
      EventLoopManagerSingleton::get()->deferCallback(
          SystemCallbackType::FinishLoadingNanoapp, std::move(cbData),
          startNanoappCallback, eventLoop);
    }
  }
}
#else
// TODO(b/475537998): Optimize callbacks to avoid unnecessary global mutex locks
void HostMessageHandlers::finishLoadingNanoappCallback(
    SystemCallbackType type, UniquePtr<LoadNanoappCallbackData> &&cbData) {
  constexpr size_t kInitialBufferSize = 48;
  ChreFlatBufferBuilder builder(kInitialBufferSize);

  CHRE_ASSERT(cbData != nullptr);
  if (cbData->nanoapp->isLoaded()) {
    startNanoappCallback(type, std::move(cbData));
  } else {
    LOGE("Nanoapp is not loaded");
    if (cbData->sendFragmentResponse) {
      sendFragmentResponse(cbData->hostClientId, cbData->transactionId,
                           cbData->fragmentId, /* success= */ false);
    }
  }
}
#endif  // CHRE_PLATFORM_OPEN_NANOAPP_ENABLED

void HostMessageHandlers::loadNanoappData(
    uint16_t hostClientId, uint32_t transactionId, uint64_t appId,
    uint32_t appVersion, uint32_t appFlags, uint32_t targetApiVersion,
    const void *buffer, size_t bufferLen, uint32_t fragmentId,
    size_t appBinaryLen, bool respondBeforeStart,
    pw::Function<EventLoop *(Nanoapp *)> getEventLoopFn) {
  bool success = true;

  if (fragmentId == 0 || fragmentId == 1) {
    size_t totalAppBinaryLen = (fragmentId == 0) ? bufferLen : appBinaryLen;
    LOGD("Load nanoapp request for app ID 0x%016" PRIx64 " ver 0x%" PRIx32
         " flags 0x%" PRIx32 " target API 0x%08" PRIx32
         " size %zu (txnId %" PRIu32 " client %" PRIu16 ")",
         appId, appVersion, appFlags, targetApiVersion, totalAppBinaryLen,
         transactionId, hostClientId);

    if (getLoadManager().hasPendingLoadTransaction()) {
      FragmentedLoadInfo info = getLoadManager().getTransactionInfo();
      LOGW("A pending load transaction already exists (clientId=%" PRIu16
           ", txnId=%" PRIu32 ", nextFragmentId=%" PRIu32 "). Overriding it",
           info.hostClientId, info.transactionId, info.nextFragmentId);
      // Send a failure response to host where nextFragmentId is either current
      // or future to the host.
      sendFragmentResponse(info.hostClientId, info.transactionId,
                           info.nextFragmentId, /* success= */ false);
      getLoadManager().markFailure();
    }

    success = getLoadManager().prepareForLoad(
        hostClientId, transactionId, appId, appVersion, appFlags,
        totalAppBinaryLen, targetApiVersion);
  }

  if (success) {
    success = getLoadManager().copyNanoappFragment(
        hostClientId, transactionId, (fragmentId == 0) ? 1 : fragmentId, buffer,
        bufferLen);
  } else {
    LOGE("Failed to prepare for load");
  }

  if (getLoadManager().isLoadComplete()) {
    LOGD("Load manager load complete...");
    auto cbData = MakeUnique<LoadNanoappCallbackData>();
    if (cbData.isNull()) {
      LOG_OOM();
    } else {
      cbData->transactionId = transactionId;
      cbData->hostClientId = hostClientId;
      cbData->appId = appId;
      cbData->fragmentId = fragmentId;
      cbData->nanoapp = getLoadManager().releaseNanoapp();
      cbData->sendFragmentResponse = !respondBeforeStart;
      cbData->getEventLoopFn = std::move(getEventLoopFn);

      // Note that if this fails, we'll generate the error response in
      // the normal deferred callback
      EventLoopManagerSingleton::get()->deferCallback(
          SystemCallbackType::FinishLoadingNanoapp, std::move(cbData),
          finishLoadingNanoappCallback);
      if (respondBeforeStart) {
        sendFragmentResponse(hostClientId, transactionId, fragmentId, success);
      }  // else the response will be sent in finishLoadingNanoappCallback
    }
  } else {
    // send a response for this fragment
    sendFragmentResponse(hostClientId, transactionId, fragmentId, success);
  }
}

void HostMessageHandlers::loadNanoappData(
    uint16_t hostClientId, uint32_t transactionId, uint64_t appId,
    uint32_t appVersion, uint32_t appFlags, uint32_t targetApiVersion,
    const void *buffer, size_t bufferLen, uint32_t fragmentId,
    size_t appBinaryLen, bool respondBeforeStart) {
  auto getEventLoopFn = [](Nanoapp *) -> EventLoop * {
    return &EventLoopManagerSingleton::get()->getEventLoop();
  };
  loadNanoappData(hostClientId, transactionId, appId, appVersion, appFlags,
                  targetApiVersion, buffer, bufferLen, fragmentId, appBinaryLen,
                  respondBeforeStart, getEventLoopFn);
}

}  // namespace chre
