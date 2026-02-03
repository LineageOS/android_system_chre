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

#include <sys/eventfd.h>
#include <unistd.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <aidl/android/hardware/contexthub/DataFlowAlertFds.h>
#include <aidl/android/hardware/contexthub/DataFlowId.h>
#include <aidl/android/hardware/contexthub/EndpointId.h>

namespace android::hardware::contexthub::common::implementation {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowAlertFds;
using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::EndpointId;

class MockCallback : public DataFlowEpollWaiter::Callback {
 public:
  MOCK_METHOD(void, onAlert, (DataFlowId, EndpointId, bool), (override));
  MOCK_METHOD(void, onWakingAck, (DataFlowId, EndpointId, uint64_t),
              (override));
};

class DataFlowEpollWaiterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto result = DataFlowEpollWaiter::create(mCallback);
    ASSERT_TRUE(result.ok());
    mWaiter = std::move(*result);
  }

  MockCallback mCallback;
  std::unique_ptr<DataFlowEpollWaiter> mWaiter;
};

TEST_F(DataFlowEpollWaiterTest, CreateAndDestroy) {
  // Setup creates the waiter, TearDown destroys it (via unique_ptr)
  EXPECT_NE(mWaiter, nullptr);
}

TEST_F(DataFlowEpollWaiterTest, AddTriggersHostEndpoint) {
  DataFlowId dataFlowId;
  dataFlowId.id = 1;
  EndpointId endpointId;
  endpointId.id = 1;
  endpointId.hubId = 1;
  DataFlowAlertFds alertFds;
  alertFds.halAck.set(eventfd(0, 0));

  EXPECT_EQ(mWaiter->addTriggers(dataFlowId, endpointId, alertFds),
            pw::OkStatus());
}

TEST_F(DataFlowEpollWaiterTest, AddTriggersEmbeddedEndpoint) {
  DataFlowId dataFlowId;
  dataFlowId.id = 1;
  EndpointId endpointId;
  endpointId.id = 1;
  endpointId.hubId = 1;
  DataFlowAlertFds alertFds;
  alertFds.waking.set(eventfd(0, 0));
  alertFds.nonWaking.set(eventfd(0, 0));

  EXPECT_EQ(mWaiter->addTriggers(dataFlowId, endpointId, alertFds),
            pw::OkStatus());
}

TEST_F(DataFlowEpollWaiterTest, AddTriggersAlreadyExists) {
  DataFlowId dataFlowId;
  dataFlowId.id = 1;
  EndpointId endpointId;
  endpointId.id = 1;
  endpointId.hubId = 1;
  DataFlowAlertFds alertFds;
  alertFds.halAck.set(eventfd(0, 0));

  ASSERT_EQ(mWaiter->addTriggers(dataFlowId, endpointId, alertFds),
            pw::OkStatus());
  DataFlowAlertFds alertFds2;
  alertFds2.halAck.set(eventfd(0, 0));
  EXPECT_EQ(mWaiter->addTriggers(dataFlowId, endpointId, alertFds2),
            pw::Status::AlreadyExists());
}

TEST_F(DataFlowEpollWaiterTest, AddTriggersEmbeddedInvalidFds) {
  DataFlowId dataFlowId;
  dataFlowId.id = 1;
  EndpointId endpointId;
  endpointId.id = 1;
  endpointId.hubId = 1;
  DataFlowAlertFds alertFds;  // Default constructed -> invalid FDs (-1)

  EXPECT_EQ(mWaiter->addTriggers(dataFlowId, endpointId, alertFds),
            pw::Status::InvalidArgument());
}

TEST_F(DataFlowEpollWaiterTest, RemoveTriggersByDataFlowId) {
  DataFlowId df1;
  df1.id = 1;
  DataFlowId df2;
  df2.id = 2;
  EndpointId ep1;
  ep1.id = 1;
  ep1.hubId = 1;
  DataFlowAlertFds fds1, fds2;
  fds1.halAck.set(eventfd(0, 0));
  fds2.halAck.set(eventfd(0, 0));

  ASSERT_EQ(mWaiter->addTriggers(df1, ep1, fds1), pw::OkStatus());
  ASSERT_EQ(mWaiter->addTriggers(df2, ep1, fds2), pw::OkStatus());

  EXPECT_EQ(mWaiter->removeTriggers(df1, std::nullopt), pw::OkStatus());
  EXPECT_EQ(mWaiter->removeTriggers(df1, std::nullopt), pw::Status::NotFound());
  EXPECT_EQ(mWaiter->removeTriggers(df2, std::nullopt), pw::OkStatus());
}

TEST_F(DataFlowEpollWaiterTest, RemoveTriggersByEndpointId) {
  DataFlowId df1;
  df1.id = 1;
  EndpointId ep1;
  ep1.id = 1;
  ep1.hubId = 1;
  EndpointId ep2;
  ep2.id = 2;
  ep2.hubId = 1;
  DataFlowAlertFds fds1, fds2;
  fds1.halAck.set(eventfd(0, 0));
  fds2.halAck.set(eventfd(0, 0));

  ASSERT_EQ(mWaiter->addTriggers(df1, ep1, fds1), pw::OkStatus());
  ASSERT_EQ(mWaiter->addTriggers(df1, ep2, fds2), pw::OkStatus());

  EXPECT_EQ(mWaiter->removeTriggers(std::nullopt, ep1), pw::OkStatus());
  EXPECT_EQ(mWaiter->removeTriggers(std::nullopt, ep1), pw::Status::NotFound());
  EXPECT_EQ(mWaiter->removeTriggers(std::nullopt, ep2), pw::OkStatus());
}

TEST_F(DataFlowEpollWaiterTest, RemoveTriggersIntersection) {
  DataFlowId df1;
  df1.id = 1;
  DataFlowId df2;
  df2.id = 2;
  EndpointId ep1;
  ep1.id = 1;
  ep1.hubId = 1;
  EndpointId ep2;
  ep2.id = 2;
  ep2.hubId = 1;

  // (DF1, EP1)
  DataFlowAlertFds fds1;
  fds1.halAck.set(eventfd(0, 0));
  ASSERT_EQ(mWaiter->addTriggers(df1, ep1, fds1), pw::OkStatus());

  // (DF1, EP2)
  DataFlowAlertFds fds2;
  fds2.halAck.set(eventfd(0, 0));
  ASSERT_EQ(mWaiter->addTriggers(df1, ep2, fds2), pw::OkStatus());

  // (DF2, EP1)
  DataFlowAlertFds fds3;
  fds3.halAck.set(eventfd(0, 0));
  ASSERT_EQ(mWaiter->addTriggers(df2, ep1, fds3), pw::OkStatus());

  // Remove (DF1, EP1) - should remove only the first one
  EXPECT_EQ(mWaiter->removeTriggers(df1, ep1), pw::OkStatus());

  // Verify (DF1, EP2) still exists
  EXPECT_EQ(mWaiter->removeTriggers(df1, ep2), pw::OkStatus());

  // Verify (DF2, EP1) still exists
  EXPECT_EQ(mWaiter->removeTriggers(df2, ep1), pw::OkStatus());
}

TEST_F(DataFlowEpollWaiterTest, RemoveTriggersNotFound) {
  DataFlowId df1;
  df1.id = 1;
  EndpointId ep1;
  ep1.id = 1;
  ep1.hubId = 1;
  EXPECT_EQ(mWaiter->removeTriggers(df1, ep1), pw::Status::NotFound());
}

TEST_F(DataFlowEpollWaiterTest, RemoveTriggersInvalidArgument) {
  EXPECT_EQ(mWaiter->removeTriggers(std::nullopt, std::nullopt),
            pw::Status::InvalidArgument());
}

}  // namespace
}  // namespace android::hardware::contexthub::common::implementation
