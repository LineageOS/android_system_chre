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

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>

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
    auto result = DataFlowEpollWaiterReal::create(mCallback);
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
  DataFlowId dataFlowId{.hubId = 1, .id = 1};
  EndpointId endpointId{.id = 1, .hubId = 1};
  DataFlowAlertFds alertFds;
  alertFds.halAck.set(eventfd(0, 0));

  EXPECT_EQ(mWaiter->addTriggers(dataFlowId, endpointId, alertFds),
            pw::OkStatus());
}

TEST_F(DataFlowEpollWaiterTest, AddTriggersEmbeddedEndpoint) {
  DataFlowId dataFlowId{.hubId = 1, .id = 1};
  EndpointId endpointId{.id = 1, .hubId = 1};
  DataFlowAlertFds alertFds;
  alertFds.waking.set(eventfd(0, 0));
  alertFds.nonWaking.set(eventfd(0, 0));

  EXPECT_EQ(mWaiter->addTriggers(dataFlowId, endpointId, alertFds),
            pw::OkStatus());
}

TEST_F(DataFlowEpollWaiterTest, AddTriggersAlreadyExists) {
  DataFlowId dataFlowId{.hubId = 1, .id = 1};
  EndpointId endpointId{.id = 1, .hubId = 1};
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
  DataFlowId dataFlowId{.hubId = 1, .id = 1};
  EndpointId endpointId{.id = 1, .hubId = 1};
  DataFlowAlertFds alertFds;  // Default constructed -> invalid FDs (-1)

  EXPECT_EQ(mWaiter->addTriggers(dataFlowId, endpointId, alertFds),
            pw::Status::InvalidArgument());
}

TEST_F(DataFlowEpollWaiterTest, RemoveTriggersByDataFlowId) {
  DataFlowId df1{.hubId = 1, .id = 1};
  DataFlowId df2{.hubId = 1, .id = 2};
  EndpointId ep1{.id = 1, .hubId = 1};
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
  DataFlowId df1{.hubId = 1, .id = 1};
  EndpointId ep1{.id = 1, .hubId = 1};
  EndpointId ep2{.id = 2, .hubId = 1};
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
  DataFlowId df1{.hubId = 1, .id = 1};
  DataFlowId df2{.hubId = 1, .id = 2};
  EndpointId ep1{.id = 1, .hubId = 1};
  EndpointId ep2{.id = 2, .hubId = 1};

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
  DataFlowId df1{.hubId = 1, .id = 1};
  EndpointId ep1{.id = 1, .hubId = 1};
  EXPECT_EQ(mWaiter->removeTriggers(df1, ep1), pw::Status::NotFound());
}

TEST_F(DataFlowEpollWaiterTest, RemoveTriggersInvalidArgument) {
  EXPECT_EQ(mWaiter->removeTriggers(std::nullopt, std::nullopt),
            pw::Status::InvalidArgument());
}

TEST_F(DataFlowEpollWaiterTest, ReceiveWakingEvent) {
  DataFlowId df1{.hubId = 1, .id = 1};
  EndpointId ep1{.id = 1, .hubId = 1};
  DataFlowAlertFds fds;
  fds.waking.set(eventfd(0, EFD_NONBLOCK));
  fds.nonWaking.set(eventfd(0, EFD_NONBLOCK));

  ASSERT_EQ(mWaiter->addTriggers(df1, ep1, fds), pw::OkStatus());

  std::promise<void> promise;
  auto future = promise.get_future();

  EXPECT_CALL(mCallback, onAlert(df1, ep1, true))
      .WillOnce(::testing::InvokeWithoutArgs([&] {
        eventfd_t val;
        eventfd_read(fds.waking.get(), &val);
        promise.set_value();
      }));

  uint64_t val = 1;
  ASSERT_EQ(write(fds.waking.get(), &val, sizeof(val)), sizeof(val));

  EXPECT_EQ(future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
}

TEST_F(DataFlowEpollWaiterTest, ReceiveNonWakingEvent) {
  DataFlowId df1{.hubId = 1, .id = 1};
  EndpointId ep1{.id = 1, .hubId = 1};
  DataFlowAlertFds fds;
  fds.waking.set(eventfd(0, EFD_NONBLOCK));
  fds.nonWaking.set(eventfd(0, EFD_NONBLOCK));

  ASSERT_EQ(mWaiter->addTriggers(df1, ep1, fds), pw::OkStatus());

  std::promise<void> promise;
  auto future = promise.get_future();

  EXPECT_CALL(mCallback, onAlert(df1, ep1, false))
      .WillOnce(::testing::InvokeWithoutArgs([&] {
        eventfd_t val;
        eventfd_read(fds.nonWaking.get(), &val);
        promise.set_value();
      }));

  uint64_t val = 1;
  ASSERT_EQ(write(fds.nonWaking.get(), &val, sizeof(val)), sizeof(val));

  EXPECT_EQ(future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
}

TEST_F(DataFlowEpollWaiterTest, ReceiveHalAckEvent) {
  DataFlowId df1{.hubId = 1, .id = 1};
  EndpointId ep1{.id = 1, .hubId = 1};
  DataFlowAlertFds fds;
  fds.halAck.set(eventfd(0, EFD_NONBLOCK));

  ASSERT_EQ(mWaiter->addTriggers(df1, ep1, fds), pw::OkStatus());

  std::promise<void> promise;
  auto future = promise.get_future();
  uint64_t expectedWakeCount = 5;

  EXPECT_CALL(mCallback, onWakingAck(df1, ep1, expectedWakeCount))
      .WillOnce(
          ::testing::InvokeWithoutArgs([&promise] { promise.set_value(); }));

  ASSERT_EQ(
      write(fds.halAck.get(), &expectedWakeCount, sizeof(expectedWakeCount)),
      sizeof(expectedWakeCount));

  EXPECT_EQ(future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
}

TEST_F(DataFlowEpollWaiterTest, EventNotReceivedAfterTriggerRemoval) {
  DataFlowId df1{.hubId = 1, .id = 1};
  EndpointId ep1{.id = 1, .hubId = 1};
  DataFlowAlertFds fds;
  fds.waking.set(eventfd(0, EFD_NONBLOCK));
  fds.nonWaking.set(eventfd(0, EFD_NONBLOCK));

  ASSERT_EQ(mWaiter->addTriggers(df1, ep1, fds), pw::OkStatus());

  // 1. Verify callback works initially
  {
    std::promise<void> promise;
    auto future = promise.get_future();
    EXPECT_CALL(mCallback, onAlert(df1, ep1, true))
        .WillOnce(::testing::InvokeWithoutArgs([&] {
          eventfd_t val;
          eventfd_read(fds.waking.get(), &val);
          promise.set_value();
        }));
    uint64_t val = 1;
    ASSERT_EQ(write(fds.waking.get(), &val, sizeof(val)), sizeof(val));
    ASSERT_EQ(future.wait_for(std::chrono::seconds(1)),
              std::future_status::ready);
  }

  // 2. Remove triggers
  ASSERT_EQ(mWaiter->removeTriggers(df1, ep1), pw::OkStatus());

  // 3. Verify callback NOT invoked
  {
    EXPECT_CALL(mCallback, onAlert).Times(0);
    uint64_t val = 1;
    ASSERT_EQ(write(fds.waking.get(), &val, sizeof(val)), sizeof(val));

    // Wait a bit to ensure no callback is received.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

TEST_F(DataFlowEpollWaiterTest, ReceiveMultipleEvents) {
  DataFlowId df1{.hubId = 1, .id = 1};
  EndpointId ep1{.id = 1, .hubId = 1};
  EndpointId ep2{.id = 1, .hubId = 2};

  DataFlowAlertFds fds1;  // Embedded
  fds1.waking.set(eventfd(0, EFD_NONBLOCK));
  fds1.nonWaking.set(eventfd(0, EFD_NONBLOCK));

  DataFlowAlertFds fds2;  // Host
  fds2.halAck.set(eventfd(0, EFD_NONBLOCK));

  ASSERT_EQ(mWaiter->addTriggers(df1, ep1, fds1), pw::OkStatus());
  ASSERT_EQ(mWaiter->addTriggers(df1, ep2, fds2), pw::OkStatus());

  struct TestState {
    std::mutex mutex;
    std::condition_variable cv;
    int wakingCount = 0;
    int nonWakingCount = 0;
    int halAckCount = 0;
  } state;

  EXPECT_CALL(mCallback, onAlert(df1, ep1, true))
      .Times(::testing::AtLeast(1))
      .WillRepeatedly(::testing::InvokeWithoutArgs([&] {
        eventfd_t val;
        eventfd_read(fds1.waking.get(), &val);
        std::lock_guard lock(state.mutex);
        state.wakingCount++;
        state.cv.notify_one();
      }));

  EXPECT_CALL(mCallback, onAlert(df1, ep1, false))
      .Times(::testing::AtLeast(1))
      .WillRepeatedly(::testing::InvokeWithoutArgs([&] {
        eventfd_t val;
        eventfd_read(fds1.nonWaking.get(), &val);
        std::lock_guard lock(state.mutex);
        state.nonWakingCount++;
        state.cv.notify_one();
      }));

  EXPECT_CALL(mCallback, onWakingAck(df1, ep2, ::testing::_))
      .Times(::testing::AtLeast(1))
      .WillRepeatedly(::testing::InvokeWithoutArgs([&] {
        std::lock_guard lock(state.mutex);
        state.halAckCount++;
        state.cv.notify_one();
      }));

  uint64_t val = 1;
  // Trigger events
  ASSERT_EQ(write(fds1.waking.get(), &val, sizeof(val)), sizeof(val));
  ASSERT_EQ(write(fds1.nonWaking.get(), &val, sizeof(val)), sizeof(val));
  ASSERT_EQ(write(fds2.halAck.get(), &val, sizeof(val)), sizeof(val));
  // Trigger again
  ASSERT_EQ(write(fds1.waking.get(), &val, sizeof(val)), sizeof(val));
  ASSERT_EQ(write(fds2.halAck.get(), &val, sizeof(val)), sizeof(val));

  std::unique_lock lock(state.mutex);
  state.cv.wait_for(lock, std::chrono::seconds(1), [&] {
    return state.wakingCount > 0 && state.nonWakingCount > 0 &&
           state.halAckCount > 0;
  });

  EXPECT_GT(state.wakingCount, 0);
  EXPECT_GT(state.nonWakingCount, 0);
  EXPECT_GT(state.halAckCount, 0);
}

}  // namespace
}  // namespace android::hardware::contexthub::common::implementation
