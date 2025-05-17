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

/**
 * @file
 * Defines the NanoappMemoryGuard class, an RAII helper for managing nanoapp
 * memory permissions.
 *
 * This class provides an interface for the platform-specific implementation
 * provided by NanoappMemoryGuardBase. This abstraction allows for
 * PlatformNanoapp code to be shared across devices with different MPU/MMU
 * characteristics.
 *
 * If hardware-based memory protection of nanoapp code is not intrinsically
 * provided by the system, or there is a desire to provide stricter protection
 * (for example, making a nanoapp's memory inaccessible unless it is currently
 * running via the expected call flow), then the platform implementer should
 * provide an implementation of NanoappMemoryGuardBase accessible via the
 * chre/target_platform/nanoapp_memory_guard_base.h include path which enables
 * and disables access to the nanoapp's memory.
 *
 * If no additional protection is needed, the include path for the no-op base
 * class implementation (found in
 * platform/shared/nanoapp_memory_guard_no_op/include/...) should be used by
 * adding it to the platform's build includes.
 */

#ifndef CHRE_PLATFORM_NANOAPP_MEMORY_GUARD_H_
#define CHRE_PLATFORM_NANOAPP_MEMORY_GUARD_H_

#include "chre/platform/platform_nanoapp.h"
#include "chre/platform/shared/nanoapp_loader.h"
#include "chre/target_platform/nanoapp_memory_guard_base.h"

namespace chre {

/**
 * @brief An RAII helper class to manage nanoapp memory permissions.
 *
 * Instantiating this class grants memory permissions for the associated
 * nanoapp (via the base class constructor). When the instance goes out of
 * scope, its destructor ensures that the permissions are revoked (via the base
 * class destructor).
 */
class NanoappMemoryGuard : public NanoappMemoryGuardBase {
 public:
  /**
   * Constructs the guard and grants memory permissions for the given nanoapp.
   *
   * @param nanoapp The nanoapp instance for which to manage memory permissions.
   */
  explicit NanoappMemoryGuard(const PlatformNanoapp &nanoapp)
      : NanoappMemoryGuardBase(nanoapp) {}

  /**
   * Constructs the guard and grants memory permissions based on the permission
   * settings in the loadable segments.
   *
   * @param loadableSegments The loadable segments of the nanoapp binary
   * @param numSegments The number of loadable segments
   */
  NanoappMemoryGuard(const NanoappLoader::LoadableSegment *loadableSegments,
                     size_t numSegments)
      : NanoappMemoryGuardBase(loadableSegments, numSegments) {}
};
}  // namespace chre

#endif  // CHRE_PLATFORM_NANOAPP_MEMORY_GUARD_H_