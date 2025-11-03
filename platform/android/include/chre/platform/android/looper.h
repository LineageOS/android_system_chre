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

#ifndef CHRE_PLATFORM_ANDROID_PLATFORM_LOOPER_H_
#define CHRE_PLATFORM_ANDROID_PLATFORM_LOOPER_H_

#include <android/sensor.h>

namespace chre {

/**
 * The CHRE AP NDK sensor looper class.
 */
class Looper {
 public:
  Looper() = default;
  ~Looper() = default;

  /**
   * Init a global looper, returns the looper.
   */
  static ALooper *init();

  /**
   * Deinit the global looper, returns the looper.
   */
  static void deinit();
};
}  // namespace chre

#endif  // CHRE_PLATFORM_ANDROID_PLATFORM_LOG_H_