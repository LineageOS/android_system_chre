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

#include "chre/pal/wwan.h"

#include "chre/util/memory.h"
#include "chre/util/unique_ptr.h"

#include <cinttypes>

/**
 * A simulated implementation of the WWAN PAL for the CHRE AP platform.
 */
namespace {

uint32_t chrePalWwanGetCapabilities() {
  // TODO(b/445584823): implement this
  return CHRE_WWAN_GET_CELL_INFO;
}

bool chrePalWwanRequestCellInfo() {
  // TODO(b/445584823): implement this
  return true;
}

void chrePalWwanReleaseCellInfoResult(
    struct chreWwanCellInfoResult * /*result*/) {
  // TODO(b/445584823): implement this
}

void chrePalWwanApiClose() {
  // TODO(b/445584823): implement this
}

bool chrePalWwanApiOpen(const struct chrePalSystemApi * /*systemApi*/,
                        const struct chrePalWwanCallbacks * /*callbacks*/) {
  // TODO(b/445584823): implement this
  return true;
}

}  // anonymous namespace

const struct chrePalWwanApi *chrePalWwanGetApi(uint32_t requestedApiVersion) {
  static const struct chrePalWwanApi kApi = {
      .moduleVersion = CHRE_PAL_WWAN_API_CURRENT_VERSION,
      .open = chrePalWwanApiOpen,
      .close = chrePalWwanApiClose,
      .getCapabilities = chrePalWwanGetCapabilities,
      .requestCellInfo = chrePalWwanRequestCellInfo,
      .releaseCellInfoResult = chrePalWwanReleaseCellInfoResult,
  };

  if (!CHRE_PAL_VERSIONS_ARE_COMPATIBLE(kApi.moduleVersion,
                                        requestedApiVersion)) {
    return nullptr;
  } else {
    return &kApi;
  }
}
