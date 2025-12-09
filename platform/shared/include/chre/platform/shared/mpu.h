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

#ifndef CHRE_PLATFORM_SHARED_MPU_H_
#define CHRE_PLATFORM_SHARED_MPU_H_

#include <cstddef>

#include "chre/platform/shared/nanoapp_loader.h"

namespace chre {

/**
 * Configures the MPU to grant memory permissions for a nanoapp.
 *
 * This function is used to set the memory protection for a nanoapp's code and
 * data segments according to the permissions defined in its ELF binary. It
 * should be called right before executing code within the nanoapp, for
 * example, when invoking one of its entry points. This is typically managed
 * by the NanoappMemoryGuard RAII class.
 *
 * @param pSegments A pointer to an array of the nanoapp's loadable segments.
 * @param size The number of segments in the pSegments array.
 * @return 0 on success, non-zero on failure, which can vary for different
 * platforms.
 */
int setNanoappMemoryPermissions(
    const struct NanoappLoader::LoadableSegment *pSegments, size_t size);

/**
 * Resets a nanoapp's memory permissions to a secure default.
 *
 * This function is used to reconfigure the memory permissions for a nanoapp's
 * memory segments, typically setting them to a read-only state. This is a
 * security measure to prevent illegal access to the nanoapp's memory when it is
 * not actively executing. It should typically be called after execution and by
 * the NanoappMemoryGuard RAII class.
 *
 * @param pSegments A pointer to an array of the nanoapp's loadable segments.
 * @param size The number of segments in the pSegments array.
 * @return 0 on success, non-zero on failure, which can vary for different
 * platforms.
 */
int resetNanoappMemoryPermissions(
    const struct NanoappLoader::LoadableSegment *pSegments, size_t size);

}  // namespace chre

#endif