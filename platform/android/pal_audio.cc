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

#include "chre/pal/audio.h"

#include "chre/platform/memory.h"
#include "chre/util/memory.h"
#include "chre/util/unique_ptr.h"

#include <cstdint>

/**
 * A simulated implementation of the audio PAL for the CHRE AP platform.
 */
namespace {

void chrePalAudioApiClose(void) {
  // TODO(b/445584823): implement this
}

bool chrePalAudioApiOpen(
    const struct chrePalSystemApi * /*systemApi*/,
    const struct chrePalAudioCallbacks * /*systemcallbacksApi*/) {
  // TODO(b/445584823): implement this
  return true;
}

bool chrePalAudioApiRequestAudioDataEvent(uint32_t /*handle*/,
                                          uint32_t /*numSamples*/,
                                          uint64_t /*eventDelayNs*/) {
  // TODO(b/445584823): implement this
  return true;
}

void chrePalAudioApiCancelAudioDataEvent(uint32_t /*handle*/) {
  // TODO(b/445584823): implement this
}

void chrePalAudioApiReleaseAudioDataEvent(
    struct chreAudioDataEvent * /*event*/) {
  // TODO(b/445584823): implement this
}

uint32_t chrePalAudioApiGetSourceCount(void) {
  // TODO(b/445584823): implement this
  return 1;
}

bool chrePalAudioApiGetAudioSource(uint32_t /*handle*/,
                                   struct chreAudioSource * /*audioSource*/) {
  // TODO(b/445584823): implement this
  return true;
}

}  // namespace

const chrePalAudioApi *chrePalAudioGetApi(uint32_t requestedApiVersion) {
  static const struct chrePalAudioApi kApi = {
      .moduleVersion = CHRE_PAL_AUDIO_API_CURRENT_VERSION,
      .open = chrePalAudioApiOpen,
      .close = chrePalAudioApiClose,
      .requestAudioDataEvent = chrePalAudioApiRequestAudioDataEvent,
      .cancelAudioDataEvent = chrePalAudioApiCancelAudioDataEvent,
      .releaseAudioDataEvent = chrePalAudioApiReleaseAudioDataEvent,
      .getSourceCount = chrePalAudioApiGetSourceCount,
      .getAudioSource = chrePalAudioApiGetAudioSource,
  };

  if (!CHRE_PAL_VERSIONS_ARE_COMPATIBLE(kApi.moduleVersion,
                                        requestedApiVersion)) {
    return nullptr;
  } else {
    return &kApi;
  }
}