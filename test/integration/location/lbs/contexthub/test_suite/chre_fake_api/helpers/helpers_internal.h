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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_HELPERS_INTERNAL_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_HELPERS_INTERNAL_H_

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_event_fake.h"

namespace lbs::contexthub::helpers::internal {

// This function is used to add newly overwritten API implementations to the
// current live functions class. It maintains the implementation of any
// non-overwritten API.
template <typename NewImpl>
void ExtendHelperMethods(ChreApiEventFunctions *live) {
  typedef ChreApiEventFunctionsImpl BaseImpl;
  if (&NewImpl::SendMessageToHost != &BaseImpl::SendMessageToHost) {
    live->SendMessageToHost = NewImpl::SendMessageToHost;
  }
  if (&NewImpl::SendMessageToHostEndpoint !=
      &BaseImpl::SendMessageToHostEndpoint) {
    live->SendMessageToHostEndpoint = NewImpl::SendMessageToHostEndpoint;
  }
  if (&NewImpl::GetNanoappInfoByAppId != &BaseImpl::GetNanoappInfoByAppId) {
    live->GetNanoappInfoByAppId = NewImpl::GetNanoappInfoByAppId;
  }
  if (&NewImpl::GetNanoappInfoByInstanceId !=
      &BaseImpl::GetNanoappInfoByInstanceId) {
    live->GetNanoappInfoByInstanceId = NewImpl::GetNanoappInfoByInstanceId;
  }
  if (&NewImpl::ConfigureNanoappInfoEvents !=
      &BaseImpl::ConfigureNanoappInfoEvents) {
    live->ConfigureNanoappInfoEvents = NewImpl::ConfigureNanoappInfoEvents;
  }
  if (&NewImpl::ConfigureHostSleepStateEvents !=
      &BaseImpl::ConfigureHostSleepStateEvents) {
    live->ConfigureHostSleepStateEvents =
        NewImpl::ConfigureHostSleepStateEvents;
  }
  if (&NewImpl::IsHostAwake != &BaseImpl::IsHostAwake) {
    live->IsHostAwake = NewImpl::IsHostAwake;
  }
}

}  // namespace lbs::contexthub::helpers::internal

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_HELPERS_HELPERS_INTERNAL_H_