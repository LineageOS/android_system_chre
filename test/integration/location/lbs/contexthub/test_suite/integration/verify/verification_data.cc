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

#include "location/lbs/contexthub/test_suite/integration/verify/verification_data.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "location/lbs/contexthub/test_suite/integration/data_feed/safe_chre_structs.h"
#include "location/lbs/contexthub/test_suite/integration/platform/simulator.h"

namespace lbs::contexthub::testing::verify {

std::vector<std::pair<uint64_t, chreMessageToHostData>> GetHostMessages() {
  return Simulator::GetInstance()->GetReceivedHostMessages();
}

std::vector<std::pair<uint64_t, NanoappRequestType>>
GetReceivedNanoappRequests() {
  return Simulator::GetInstance()->GetNanoappPlatformRequests();
}

}  // namespace lbs::contexthub::testing::verify