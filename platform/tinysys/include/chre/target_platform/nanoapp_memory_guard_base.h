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

#ifndef CHRE_PLATFORM_NANOAPP_MEMORY_GUARD_BASE_H_
#define CHRE_PLATFORM_NANOAPP_MEMORY_GUARD_BASE_H_

#include "chre/platform/platform_nanoapp.h"
#include "chre/platform/shared/nanoapp_loader.h"
#include "chre/util/non_copyable.h"

namespace chre {

/** Base class for platform-specific nanoapp memory protection. */
class NanoappMemoryGuardBase : public NonCopyable {
 public:
  explicit NanoappMemoryGuardBase(const PlatformNanoapp &nanoapp);

  NanoappMemoryGuardBase(const NanoappLoader::LoadableSegment *loadableSegments,
                         size_t numSegments);

  virtual ~NanoappMemoryGuardBase();

 private:
  /**
   * Applies permissions to the nanoapp's memory segments based on the
   * permission settings in NanoappLoader::LoadableSegment.
   */
  void grantMemoryPermissions() const;

  /** Removes the permissions applied to the nanoapp's memory segments. */
  void revokeMemoryPermissions() const;

  const NanoappLoader::LoadableSegment *mLoadableSegments = nullptr;
  size_t mNumSegments = 0;
};
}  // namespace chre
#endif  // CHRE_PLATFORM_NANOAPP_MEMORY_GUARD_BASE_H_