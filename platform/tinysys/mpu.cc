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

#include <cstddef>

#include "chre/platform/shared/mpu.h"

extern "C" {
// TODO(b/394483221) - Place holders. A header file from tinysys should be used.
int elf_set_permission(const void *pSegments, size_t size);
int elf_set_permission_default_ro(const void *pSegments, size_t size);
}

namespace chre {
int setNanoappMemoryPermissions(
    const struct NanoappLoader::LoadableSegment *pSegments, size_t size) {
  return elf_set_permission(pSegments, size);
}

int resetNanoappMemoryPermissions(
    const struct NanoappLoader::LoadableSegment *pSegments, size_t size) {
  return elf_set_permission_default_ro(pSegments, size);
}
}  // namespace chre