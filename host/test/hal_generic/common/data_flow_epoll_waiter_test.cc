/*
 * Copyright (C) 2026 The Android Open Source Project
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

#include "data_flow_epoll_waiter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <aidl/android/hardware/contexthub/DataFlowId.h>
#include <aidl/android/hardware/contexthub/EndpointId.h>

namespace android::hardware::contexthub::common::implementation {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::EndpointId;

class MockCallback : public DataFlowEpollWaiter::Callback {
 public:
  MOCK_METHOD(void, onAlert, (DataFlowId, EndpointId, bool), (override));
  MOCK_METHOD(void, onWakingAck, (DataFlowId, EndpointId, uint64_t),
              (override));
};

TEST(DataFlowEpollWaiterTest, CreateAndDestroy) {
  MockCallback callback;
  auto result = DataFlowEpollWaiter::create(callback);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(*result, nullptr);
}

}  // namespace
}  // namespace android::hardware::contexthub::common::implementation
