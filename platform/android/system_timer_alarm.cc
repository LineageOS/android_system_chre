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

#include "chre/platform/system_timer.h"

#include <signal.h>
#include <mutex>

#include "chre/platform/log.h"
#include "chre/util/time.h"

extern "C" {
void chreApSetAlarm(uint64_t timerId, uint64_t delayNs);
void chreApCancelAlarm(uint64_t timerId);
}

namespace chre {

void SystemTimerBase::systemTimerNotifyCallback(union sigval cookie) {
  SystemTimer *sysTimer = static_cast<SystemTimer *>(cookie.sival_ptr);
  std::lock_guard<std::mutex> lock(sysTimer->mMutex);
  sysTimer->mCallback(sysTimer->mData);
}

SystemTimer::SystemTimer() {
  mInitialized = true;
}

SystemTimer::~SystemTimer() {
  if (mIsActive) {
    cancel();
  }
  mInitialized = false;
}

bool SystemTimer::init() {
  mInitialized = true;
  return mInitialized;
}

bool SystemTimer::set(SystemTimerCallback *callback, void *data,
                      Nanoseconds delay) {
  // 0 has a special meaning in POSIX, i.e. cancel the timer. In our API, a
  // value of 0 just means fire right away.
  if (delay.toRawNanoseconds() == 0) {
    delay = Nanoseconds(1);
  }

  {
    std::lock_guard<std::mutex> lock(mMutex);
    mCallback = callback;
    mData = data;
  }

  uint64_t timerId = reinterpret_cast<uint64_t>(this);
  chreApSetAlarm(timerId, delay.toRawNanoseconds());
  mIsActive = true;

  return true;
}

bool SystemTimer::cancel() {
  uint64_t timerId = reinterpret_cast<uint64_t>(this);
  chreApCancelAlarm(timerId);
  mIsActive = false;
  return true;
}

bool SystemTimer::isActive() {
  return mIsActive;
}

}  // namespace chre
