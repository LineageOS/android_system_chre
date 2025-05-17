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

#include "chre/platform/shared/nanoapp_memory_guard.h"
#include "chre/platform/platform_nanoapp.h"

namespace chre {
NanoappMemoryGuardBase::NanoappMemoryGuardBase(const PlatformNanoapp &nanoapp) {
  if (nanoapp.isStatic()) {
    return;
  }

  auto *loader = static_cast<NanoappLoader *>(nanoapp.getDsoHandle());
  mLoadableSegments = loader->getLoadableSegments().data();
  mNumSegments = loader->getLoadableSegments().size();
  grantMemoryPermissions();
}

NanoappMemoryGuardBase::NanoappMemoryGuardBase(
    const NanoappLoader::LoadableSegment *loadableSegments,
    const size_t numSegments)
    : mLoadableSegments(loadableSegments), mNumSegments(numSegments) {
  grantMemoryPermissions();
}

NanoappMemoryGuardBase::~NanoappMemoryGuardBase() {
  revokeMemoryPermissions();
}

void NanoappMemoryGuardBase::grantMemoryPermissions() const {
  if (mLoadableSegments == nullptr || mNumSegments == 0) {
    return;
  }
  // TODO(b/394483221) - grant permissions based on mLoadableSegments.
}

void NanoappMemoryGuardBase::revokeMemoryPermissions() const {
  if (mLoadableSegments == nullptr || mNumSegments == 0) {
    return;
  }
  // TODO(b/394483221) - revoke permissions.
}
}  // namespace chre