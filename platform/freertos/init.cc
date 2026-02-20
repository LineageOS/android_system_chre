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

#include "chre/target_platform/init.h"

#ifdef CHRE_ENABLE_CHPP
#include "chpp/platform/chpp_init.h"
#endif  // CHRE_ENABLE_CHPP

#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED
#include "chre/core/ble_socket_manager.h"
#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED

#include "chre/core/event_loop_manager.h"
#include "chre/core/static_nanoapps.h"
#include "chre/platform/condition_variable.h"
#include "chre/platform/context.h"
#include "chre/platform/shared/dram_vote_client.h"
#include "chre/platform/shared/init.h"
#include "chre/util/macros.h"
#include "chre/variant/config.h"

#ifdef CHRE_USE_BUFFERED_LOGGING
#include "chre/platform/shared/log_buffer_manager.h"
#include "chre/target_platform/macros.h"
#endif  // CHRE_USE_BUFFERED_LOGGING

#include "FreeRTOS.h"
#include "task.h"

#include <array>
#include <optional>

namespace chre {
namespace freertos {
namespace {

#ifdef CHRE_FREERTOS_TASK_PRIORITY
constexpr UBaseType_t kChreTaskPriority =
    tskIDLE_PRIORITY + CHRE_FREERTOS_TASK_PRIORITY;
#else
constexpr UBaseType_t kChreTaskPriority = tskIDLE_PRIORITY + 1;
#endif  // CHRE_FREERTOS_TASK_PRIORITY

#ifdef CHRE_FREERTOS_STACK_DEPTH_IN_WORDS
constexpr configSTACK_DEPTH_TYPE kChreTaskStackDepthWords =
    CHRE_FREERTOS_STACK_DEPTH_IN_WORDS;
#else
constexpr configSTACK_DEPTH_TYPE kChreTaskStackDepthWords = 0x800;
#endif  // CHRE_FREERTOS_STACK_DEPTH_IN_WORDS

#if CHRE_MULTI_THREADING_ENABLED
constexpr size_t kNumEventLoops = 2;

TaskHandle_t gChreTaskHandles[kNumEventLoops];
std::optional<std::array<EventLoop, kNumEventLoops>> gEventLoops;
#else
TaskHandle_t gChreTaskHandle;
#endif

#ifndef CHRE_HIGH_POWER_BSS_ATTRIBUTE
#define CHRE_HIGH_POWER_BSS_ATTRIBUTE
#endif  // CHRE_HIGH_POWER_BSS_ATTRIBUTE

#ifdef CHRE_USE_BUFFERED_LOGGING
TaskHandle_t gChreFlushTaskHandle;

CHRE_HIGH_POWER_BSS_ATTRIBUTE
uint8_t gSecondaryLogBufferData[CHRE_LOG_BUFFER_DATA_SIZE];

uint8_t gPrimaryLogBufferData[CHRE_LOG_BUFFER_DATA_SIZE];
#endif  // CHRE_USE_BUFFERED_LOGGING

// TODO(b/485889897): Make the multi-threading setting more configurable.
#if CHRE_MULTI_THREADING_ENABLED
Mutex gInitMutex;
ConditionVariable gInitCond;
bool gChreInitializationComplete = false;

Mutex gDeinitMutex;
ConditionVariable gDeinitCond;
bool gBackgroundThreadActive = true;

constexpr size_t kForegroundEventLoopIndex = 0;
constexpr size_t kBackgroundEventLoopIndex = 1;

// Foreground
StackType_t gForegroundChreWorkerStack[kChreTaskStackDepthWords];
StaticTask_t gForegroundChreWorkerTcb;

// Background
StackType_t gBackgroundChreWorkerStack[kChreTaskStackDepthWords];
StaticTask_t gBackgroundChreWorkerTcb;

void chreForegroundThreadEntry(void * /*context*/) {
  CHRE_ASSERT(gEventLoops.has_value());
  EventLoop *eventLoop = &gEventLoops.value()[kForegroundEventLoopIndex];
  vTaskSetThreadLocalStoragePointer(/* xTaskToSet= */ nullptr, /* xIndex= */ 0,
                                    eventLoop);

  chre::DramVoteClientSingleton::get()->incrementDramVoteCount();
  chre::initCommon(
      pw::span(gEventLoops.value().data(), gEventLoops.value().size()));
  chre::EventLoopManagerSingleton::get()->lateInit();
  chre::DramVoteClientSingleton::get()->decrementDramVoteCount();
  chre::loadStaticNanoapps();

  {
    LockGuard<Mutex> lock(gInitMutex);
    gChreInitializationComplete = true;
    gInitCond.notify_one();
  }
  eventLoop->run();

  {
    LockGuard<Mutex> lock(gDeinitMutex);
    while (gBackgroundThreadActive) {
      gDeinitCond.wait(gDeinitMutex);
    }
  }

  chre::DramVoteClientSingleton::get()->incrementDramVoteCount();
  chre::deinitCommon();
  chre::DramVoteClientSingleton::get()->decrementDramVoteCount();

  DramVoteClientSingleton::deinit();
  gEventLoops.reset();
  vTaskDelete(nullptr);
  gChreTaskHandles[0] = nullptr;
}

void chreBackgroundThreadEntry(void * /*context*/) {
  {
    LockGuard<Mutex> lock(gDeinitMutex);
    gBackgroundThreadActive = true;
  }

  CHRE_ASSERT(gEventLoops.has_value());
  EventLoop *eventLoop = &gEventLoops.value()[kBackgroundEventLoopIndex];
  vTaskSetThreadLocalStoragePointer(/* xTaskToSet= */ nullptr, /* xIndex= */ 0,
                                    eventLoop);

  {
    LockGuard<Mutex> lock(gInitMutex);
    while (!gChreInitializationComplete) {
      gInitCond.wait(gInitMutex);
    }
  }

  eventLoop->run();

  {
    LockGuard<Mutex> lock(gDeinitMutex);
    gBackgroundThreadActive = false;
    gDeinitCond.notify_one();
  }

  vTaskDelete(nullptr);
  gChreTaskHandles[1] = nullptr;
}
#else
// This function is intended to be the task action function for FreeRTOS.
// It Initializes CHRE, runs the event loop, and only exits if it receives
// a message to shutdown. Note that depending on the hardware platform this
// runs on, CHRE might create additional threads, which are cleaned up when
// CHRE exits.
void chreThreadEntry(void * /* context */) {
  chre::DramVoteClientSingleton::get()->incrementDramVoteCount();
  chre::initCommon();
  chre::EventLoopManagerSingleton::get()->lateInit();
  chre::DramVoteClientSingleton::get()->decrementDramVoteCount();
  chre::loadStaticNanoapps();
  chre::EventLoopManagerSingleton::get()->getEventLoop().run();

  // we only get here if the CHRE EventLoop exited
  chre::DramVoteClientSingleton::get()->incrementDramVoteCount();
  chre::deinitCommon();
  chre::DramVoteClientSingleton::get()->decrementDramVoteCount();

  DramVoteClientSingleton::deinit();

  vTaskDelete(nullptr);
  gChreTaskHandle = nullptr;
  // TODO(b/425748478): Determine if this should crash if the CHRE EventLoop
  // thread exits
}
#endif  // CHRE_MULTI_THREADING_ENABLED

#ifdef CHRE_USE_BUFFERED_LOGGING
void chreFlushLogsToHostThreadEntry(void *context) {
  UNUSED_VAR(context);

  // Never exits
  chre::LogBufferManagerSingleton::get()->startSendLogsToHostLoop();
}
#endif  // CHRE_USE_BUFFERED_LOGGING

}  // namespace

#ifdef CHRE_USE_BUFFERED_LOGGING
const char *getChreFlushTaskName();
#endif  // CHRE_USE_BUFFERED_LOGGING

BaseType_t init() {
#if CHRE_MULTI_THREADING_ENABLED
  gEventLoops.emplace();
  {
    LockGuard<Mutex> lock(gInitMutex);
    gChreInitializationComplete = false;
  }
  gChreTaskHandles[0] = xTaskCreateStatic(
      chreForegroundThreadEntry, "CHRE_fg", kChreTaskStackDepthWords,
      nullptr /* args */, kChreTaskPriority + 1, gForegroundChreWorkerStack,
      &gForegroundChreWorkerTcb);
  gChreTaskHandles[1] = xTaskCreateStatic(
      chreBackgroundThreadEntry, "CHRE", kChreTaskStackDepthWords,
      nullptr /* args */, kChreTaskPriority, gBackgroundChreWorkerStack,
      &gBackgroundChreWorkerTcb);
  BaseType_t rc = pdPASS;
#else
  BaseType_t rc =
      xTaskCreate(chreThreadEntry, getChreTaskName(), kChreTaskStackDepthWords,
                  nullptr /* args */, kChreTaskPriority, &gChreTaskHandle);
  CHRE_ASSERT(rc == pdPASS);
#endif

#ifdef CHRE_ENABLE_CHPP
  chpp::init();
#endif  // CHRE_ENABLE_CHPP

  return rc;
}

BaseType_t initLogger() {
  BaseType_t rc = pdPASS;
#ifdef CHRE_USE_BUFFERED_LOGGING
  if (!chre::LogBufferManagerSingleton::isInitialized()) {
    chre::LogBufferManagerSingleton::init(gPrimaryLogBufferData,
                                          gSecondaryLogBufferData,
                                          sizeof(gPrimaryLogBufferData));

    rc = xTaskCreate(chreFlushLogsToHostThreadEntry, getChreFlushTaskName(),
                     kChreTaskStackDepthWords, nullptr /* args */,
                     kChreTaskPriority, &gChreFlushTaskHandle);
  }
#endif  // CHRE_USE_BUFFERED_LOGGING
  return rc;
}

void deinit() {
  // On a deinit call, we just stop the CHRE event loop. This causes the 'run'
  // method in the task function exit, and move on to handle task cleanup
#if CHRE_MULTI_THREADING_ENABLED
  for (size_t i = 0; i < kNumEventLoops; ++i) {
    if (gChreTaskHandles[i] != nullptr) {
      gEventLoops.value()[i].stop();
      gChreTaskHandles[i] = nullptr;
    }
  }
#else
  if (gChreTaskHandle != nullptr) {
    chre::EventLoopManagerSingleton::get()->getEventLoop().stop();
  }
#endif

#ifdef CHRE_ENABLE_CHPP
  chpp::deinit();
#endif  // CHRE_ENABLE_CHPP
}

const char *getChreTaskName() {
  static constexpr char kChreTaskName[] = "CHRE";
  return kChreTaskName;
}

#ifdef CHRE_USE_BUFFERED_LOGGING
const char *getChreFlushTaskName() {
  static constexpr char kChreFlushTaskName[] = "CHRELogs";
  return kChreFlushTaskName;
}
#endif  // CHRE_USE_BUFFERED_LOGGING

}  // namespace freertos

BaseType_t getChreTaskPriority() {
  return freertos::kChreTaskPriority;
}

EventLoop *getEventLoopForNanoapp(Nanoapp *nanoapp) {
#if CHRE_MULTI_THREADING_ENABLED
  CHRE_ASSERT(nanoapp->isOpen());
  CHRE_ASSERT(gEventLoops.has_value());
  if (nanoapp->getRequestedThreadPriority() ==
      NANOAPP_REQUESTED_THREAD_PRIORITY_FOREGROUND) {
    return &freertos::gEventLoops.value()[freertos::kForegroundEventLoopIndex];
  } else {
    return &freertos::gEventLoops.value()[freertos::kBackgroundEventLoopIndex];
  }
#else
  UNUSED_VAR(nanoapp);
  return &EventLoopManagerSingleton::get()->getEventLoop();
#endif  // CHRE_MULTI_THREADING_ENABLED
}

bool inEventLoopThread() {
#if CHRE_MULTI_THREADING_ENABLED
  for (size_t i = 0; i < freertos::kNumEventLoops; ++i) {
    if (xTaskGetCurrentTaskHandle() == freertos::gChreTaskHandles[i]) {
      return true;
    }
  }
  return false;
#else
  return (xTaskGetCurrentTaskHandle() == freertos::gChreTaskHandle);
#endif
}

EventLoop *getCurrentEventLoop() {
#if CHRE_MULTI_THREADING_ENABLED
  return static_cast<EventLoop *>(pvTaskGetThreadLocalStoragePointer(
      /* xTaskToQuery= */ nullptr, /* xIndex= */ 0));
#else
  return inEventLoopThread() ? &EventLoopManagerSingleton::get()->getEventLoop()
                             : nullptr;
#endif
}

}  // namespace chre
