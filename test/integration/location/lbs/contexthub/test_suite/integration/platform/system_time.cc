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

#include "chre/platform/system_time.h"

#include <cstdint>

#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"
#include "third_party/contexthub/chre/util/include/chre/util/time.h"

using lbs::contexthub::testing::Simulator;

namespace chre {

Nanoseconds SystemTime::getMonotonicTime() {
  return Nanoseconds(Simulator::GetInstance()->GetCurrentTime());
}

int64_t SystemTime::getEstimatedHostTimeOffset() {
  return 0;
}

}  // namespace chre