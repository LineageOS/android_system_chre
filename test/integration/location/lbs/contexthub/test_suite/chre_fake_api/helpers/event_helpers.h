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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_EVENT_HELPERS_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_EVENT_HELPERS_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"

// This file provides helper functions that overrides event CHRE API calls with
// their common uses in unit tests.
namespace lbs::contexthub::helpers {

constexpr uint32_t INSTANCE_ID = 100;

// SetEmptyEvent is only intended to be used during shutdown of the nanoapp to
// bypass the loss of context. All functions are empty in this class.
void SetEmptyEvent();

// This function makes the IsHostAwake() API return true for all calls.
void SetEventHostIsAwake();

// This function makes the IsHostAsleep() API return false for all calls.
void SetEventHostIsAsleep();

// This function makes the GetNanoappInfoByAppId() API return true for all
// calls.
void SetEventGetNanoappInfoByAppId();

// This function will override the setting state of given settings.
// The map is from setting id to setting state. The default is defined by
// ChreApiEventFunctionsImpl::UserSettingGetState.
void SetEventSettingState(absl::flat_hash_map<uint8_t, int8_t> overrides);

}  // namespace lbs::contexthub::helpers

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_EVENT_HELPERS_H_