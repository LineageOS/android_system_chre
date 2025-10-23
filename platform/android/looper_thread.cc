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

#include "chre/platform/android/looper.h"
#include "chre/platform/log.h"

#include <android/looper.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace chre {
namespace {
ALooper *gLooper = nullptr;
std::thread gLooperThread;
std::atomic<bool> gIsLooperRunning{false};
std::mutex gLooperMutex;
std::condition_variable gLooperCondVar;
bool gLooperReady = false;
std::chrono::duration kLooperInitTimeout = std::chrono::seconds(10);
}  // namespace

// This implementation provides the looper in a new thread.

ALooper *Looper::init() {
  // Starts looper thread.
  gIsLooperRunning.store(true, std::memory_order_relaxed);
  gLooperReady = false;
  gLooperThread = std::thread([]() {
    gLooper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

    // Notify init() Looper initialization is ready.
    {
      std::lock_guard<std::mutex> lock(gLooperMutex);
      gLooperReady = true;
    }
    gLooperCondVar.notify_one();
    // Returns early if gLooper initialization failed.
    if (gLooper == nullptr) {
      return;
    }

    LOGI("Sensor Looper thread started and polling.");
    while (gIsLooperRunning.load(std::memory_order_relaxed)) {
      int result =
          ALooper_pollOnce(-1 /* timeoutMillis */, nullptr /* outFd */,
                           nullptr /* outEvents */, nullptr /* outData */);

      if (result == ALOOPER_POLL_WAKE) {
        continue;  // Awaked by ALooper_wake()，loop checking mIsLooperRunning
      }

      if (result == ALOOPER_POLL_ERROR) {
        LOGE("Sensor PAL: ALooper_pollOnce returned an error.");
      }
    }

    LOGI("Sensor PAL Looper thread stopped.");
  });

  // Waits for mLooper thread ready.
  {
    std::unique_lock<std::mutex> lock(gLooperMutex);
    if (!gLooperCondVar.wait_for(lock, kLooperInitTimeout,
                                 [] { return gLooperReady; })) {
      LOGE("Time out waiting for gLooperThread init.");
      return nullptr;
    }
  }

  // Checks gLooper initialization states.
  if (gLooper == nullptr) {
    LOGE("Failed to prepare ALooper in worker thread.");
    if (gLooperThread.joinable()) {
      gLooperThread.join();
    }
  }
  return gLooper;
}

void Looper::deinit() {
  // Stop mLooper thread.
  if (gIsLooperRunning.exchange(false, std::memory_order_relaxed)) {
    if (gLooper != nullptr) {
      // Awak the mLooper to exit it.
      ALooper_wake(gLooper);
    }
    if (gLooperThread.joinable()) {
      gLooperThread.join();
    }
  }
  gLooper = nullptr;
  gLooperReady = false;
}

}  // namespace chre
