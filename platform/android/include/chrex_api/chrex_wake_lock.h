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

#ifndef _CHREX_WAKE_CLOCK_H_
#define _CHREX_WAKE_CLOCK_H_

#include <cstddef>
#include <cstdint>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * @file
 * API related to CHRE AP wake lock. Should not be used in other environments.
 */

/**
 * @brief Acquire a wake lock from AP with the lock name and timeout
 * @param name Name of the lock
 * @param timeout_millis Timeout in milliseconds
 */
void chrexApWakeLockAcquire(const std::string &lock_name,
                            uint64_t timeout_millis);

/**
 * @brief Release a wake lock from AP with the given lock name
 * @param name Name of the lock
 */
void chrexApWakeLockRelease(const std::string &lock_name);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // _CHREX_WAKE_CLOCK_H_