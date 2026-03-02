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

#include "location/lbs/contexthub/test_suite/integration/examples/codelab/find_2x/find_2x_static_nanoapp.h"

#include <cstddef>

#include "chre/core/event.h"
#include "chre/core/nanoapp.h"
#include "chre/core/static_nanoapps.h"
#include "chre/util/unique_ptr.h"

namespace chre {

const StaticNanoappInitFunction kStaticNanoappList[] = {
    initializeStaticNanoappFind2x,
};
const size_t kStaticNanoappCount = ARRAY_SIZE(kStaticNanoappList);
}  // namespace chre
