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

#include "data_flow_manager.h"

#include <sys/eventfd.h>

#include <cstdint>
#include <memory>
#include <optional>

#include <aidl/android/hardware/contexthub/DataFlowAlertFds.h>
#include <aidl/android/hardware/contexthub/DataFlowInfo.h>
#include <aidl/android/hardware/contexthub/DataFlowSinkRegistrationParams.h>
#include <aidl/android/hardware/contexthub/EndpointId.h>
#include <aidl/android/hardware/contexthub/SharedDataRegionRequirements.h>
#include <android-base/unique_fd.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "data_flow_epoll_waiter.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "region_allocator.h"
#include "wakelock_manager.h"

namespace android::hardware::contexthub::common::implementation {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowAlertFds;
using ::aidl::android::hardware::contexthub::DataFlowInfo;
using ::aidl::android::hardware::contexthub::DataFlowSinkRegistrationParams;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::aidl::android::hardware::contexthub::SharedDataRegionRequirements;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

constexpr int64_t kHubId = 123;
constexpr int32_t kRegionId = 1;
constexpr int64_t kSinkHubId = 456;

class MockRegionAllocator : public RegionAllocator {
 public:
  MOCK_METHOD(pw::Result<SharedDataRegion>, allocateRegion,
              (const SharedDataRegionRequirements &requirements), (override));
  MOCK_METHOD(pw::Status, releaseRegion, (int32_t id), (override));
  MOCK_METHOD(pw::Result<SharedDataRegion>, getRegionInfo, (int32_t id),
              (override));
  MOCK_METHOD(bool, consumerRequiresSeparateRegion, (int64_t hubId),
              (override));
};

class MockWakelockManager : public WakelockManager {
 public:
  MOCK_METHOD(pw::Status, increaseWakeCount, (Usage usage, size_t count),
              (override));
  MOCK_METHOD(pw::Status, decreaseWakeCount, (Usage usage, size_t count),
              (override));
};

class MockDataFlowEpollWaiter : public DataFlowEpollWaiter {
 public:
  MOCK_METHOD(pw::Status, addTriggers,
              (DataFlowId dataFlowId, EndpointId endpointId,
               const DataFlowAlertFds &alertFds),
              (override));
  MOCK_METHOD(pw::Status, removeTriggers,
              (std::optional<DataFlowId> dataFlowId,
               std::optional<EndpointId> endpointId),
              (override));
};

class TestableDataFlowManager : public DataFlowManager {
 public:
  TestableDataFlowManager(
      const std::shared_ptr<RegionAllocator> &regionAllocator,
      const std::shared_ptr<WakelockManager> &wakelockManager,
      SendAlertFn sendAlertFn)
      : DataFlowManager(regionAllocator, wakelockManager, sendAlertFn) {}
  virtual ~TestableDataFlowManager() = default;

  void setEpollWaiter(std::unique_ptr<DataFlowEpollWaiter> epollWaiter) {
    mEpollWaiter = std::move(epollWaiter);
  }
};

class DataFlowManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mRegionAllocator = std::make_shared<MockRegionAllocator>();
    mWakelockManager = std::make_shared<MockWakelockManager>();
    mDataFlowManager = std::make_unique<TestableDataFlowManager>(
        mRegionAllocator, mWakelockManager,
        [this](DataFlowId dataFlowId, EndpointId sender, EndpointId receiver,
               bool waking) {
          return sendAlert(dataFlowId, sender, receiver, waking);
        });
    auto epollWaiter = std::make_unique<MockDataFlowEpollWaiter>();
    mEpollWaiter = epollWaiter.get();
    mDataFlowManager->setEpollWaiter(std::move(epollWaiter));
  }

  void allocateRegion(int32_t regionId) {
    SharedDataRegionRequirements requirements;
    EXPECT_CALL(*mRegionAllocator, allocateRegion(_))
        .WillOnce(Invoke([regionId] {
          SharedDataRegion region;
          region.id = regionId;
          return region;
        }));
    auto result = mDataFlowManager->allocateRegion(kHubId, requirements);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().id, regionId);
  }

  MOCK_METHOD(pw::Status, sendAlert,
              (DataFlowId, EndpointId, EndpointId, bool));

  std::shared_ptr<MockRegionAllocator> mRegionAllocator;
  std::shared_ptr<MockWakelockManager> mWakelockManager;
  std::unique_ptr<TestableDataFlowManager> mDataFlowManager;
  MockDataFlowEpollWaiter *mEpollWaiter;
};

TEST_F(DataFlowManagerTest, AllocateRegionSuccess) {
  allocateRegion(kRegionId);
}

TEST_F(DataFlowManagerTest, AllocateRegionFailure) {
  SharedDataRegionRequirements requirements;

  EXPECT_CALL(*mRegionAllocator, allocateRegion(_))
      .WillOnce(Return(pw::Status::ResourceExhausted()));

  auto result = mDataFlowManager->allocateRegion(kHubId, requirements);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
}

TEST_F(DataFlowManagerTest, ReleaseRegionSuccess) {
  allocateRegion(kRegionId);
  EXPECT_CALL(*mRegionAllocator, releaseRegion(kRegionId))
      .WillOnce(Return(pw::OkStatus()));

  auto status = mDataFlowManager->releaseRegion(kHubId, kRegionId);
  ASSERT_EQ(status, pw::OkStatus());
}

TEST_F(DataFlowManagerTest, ReleaseRegionHubNotFound) {
  // Attempt to release a region for a hub that hasn't allocated any regions
  auto status = mDataFlowManager->releaseRegion(kHubId, kRegionId);
  ASSERT_EQ(status, pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, ReleaseRegionRegionNotFound) {
  allocateRegion(kRegionId);
  constexpr int32_t kWrongRegionId = kRegionId + 1;
  auto status = mDataFlowManager->releaseRegion(kHubId, kWrongRegionId);
  ASSERT_EQ(status, pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, ReleaseRegionInUse) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId}};

  EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
      .WillOnce(Return(pw::OkStatus()));

  auto flowId = mDataFlowManager->addHostSourceDataFlow(source, info).value();

  EXPECT_EQ(mDataFlowManager->releaseRegion(kHubId, kRegionId),
            pw::Status::FailedPrecondition());

  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->removeDataFlow(flowId);
  ASSERT_TRUE(result.ok());

  // Should be able to release region now
  EXPECT_CALL(*mRegionAllocator, releaseRegion(kRegionId))
      .WillOnce(Return(pw::OkStatus()));
  EXPECT_EQ(mDataFlowManager->releaseRegion(kHubId, kRegionId), pw::OkStatus());
}

TEST_F(DataFlowManagerTest, AddHostSourceDataFlowSuccess) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId}};

  EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->addHostSourceDataFlow(source, info);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().hubId, kHubId);
}

TEST_F(DataFlowManagerTest, AddHostSourceDataFlowRegionNotAllocated) {
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId}};

  auto result = mDataFlowManager->addHostSourceDataFlow(source, info);
  EXPECT_EQ(result.status(), pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, AddHostSourceDataFlowWrongRegionId) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId + 1}};

  auto result = mDataFlowManager->addHostSourceDataFlow(source, info);
  EXPECT_EQ(result.status(), pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, AddHostSourceDataFlowTriggerFailure) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId}};

  EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
      .WillOnce(Return(pw::Status::Internal()));

  auto result = mDataFlowManager->addHostSourceDataFlow(source, info);
  EXPECT_EQ(result.status(), pw::Status::Internal());
}

TEST_F(DataFlowManagerTest, AddOffloadSinkSuccess) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId}};
  EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
      .WillOnce(Return(pw::OkStatus()));
  auto flowId = mDataFlowManager->addHostSourceDataFlow(source, info).value();

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  DataFlowSinkRegistrationParams params{
      .context = {.id = flowId},
      .sourceId = source,
      .sinkId = sink,
  };

  EXPECT_CALL(*mRegionAllocator, consumerRequiresSeparateRegion(kSinkHubId))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kRegionId))
      .WillOnce(Invoke([](int32_t id) { return SharedDataRegion{.id = id}; }));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink, _))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->addOffloadSink(params);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().second.id, kRegionId);
}

TEST_F(DataFlowManagerTest, AddOffloadSinkSeparateRegion) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId}};
  EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
      .WillOnce(Return(pw::OkStatus()));
  auto flowId = mDataFlowManager->addHostSourceDataFlow(source, info).value();

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  DataFlowSinkRegistrationParams params{
      .context = {.id = flowId},
      .sourceId = source,
      .sinkId = sink,
  };

  constexpr int32_t kMetadataRegionId = 99;
  EXPECT_CALL(*mRegionAllocator, consumerRequiresSeparateRegion(kSinkHubId))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mRegionAllocator, allocateRegion(_)).WillOnce(Invoke([] {
    return SharedDataRegion{.id = kMetadataRegionId};
  }));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink, _))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->addOffloadSink(params);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().second.id, kMetadataRegionId);
}

TEST_F(DataFlowManagerTest, AddOffloadSinkFailures) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId}};
  EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
      .WillOnce(Return(pw::OkStatus()));
  auto flowId = mDataFlowManager->addHostSourceDataFlow(source, info).value();

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  DataFlowSinkRegistrationParams params{
      .context = {.id = flowId},
      .sourceId = source,
      .sinkId = sink,
  };

  // Data flow not found
  params.context.id.id++;
  EXPECT_EQ(mDataFlowManager->addOffloadSink(params).status(),
            pw::Status::NotFound());
  params.context.id = flowId;

  // Source mismatch
  params.sourceId.id++;
  EXPECT_EQ(mDataFlowManager->addOffloadSink(params).status(),
            pw::Status::InvalidArgument());
  params.sourceId = source;

  // Already exists
  EXPECT_CALL(*mRegionAllocator, consumerRequiresSeparateRegion(kSinkHubId))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kRegionId))
      .WillOnce(Invoke([](int32_t id) { return SharedDataRegion{.id = id}; }));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink, _))
      .WillOnce(Return(pw::OkStatus()));
  ASSERT_TRUE(mDataFlowManager->addOffloadSink(params).ok());
  EXPECT_EQ(mDataFlowManager->addOffloadSink(params).status(),
            pw::Status::AlreadyExists());
}

TEST_F(DataFlowManagerTest, RemoveDataFlowHostSource) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId}};
  EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
      .WillOnce(Return(pw::OkStatus()));
  auto flowId = mDataFlowManager->addHostSourceDataFlow(source, info).value();

  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->removeDataFlow(flowId);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(DataFlowManagerTest, RemoveDataFlowWithSinks) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId}};
  EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
      .WillOnce(Return(pw::OkStatus()));
  auto flowId = mDataFlowManager->addHostSourceDataFlow(source, info).value();

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  DataFlowSinkRegistrationParams params{
      .context = {.id = flowId},
      .sourceId = source,
      .sinkId = sink,
  };

  EXPECT_CALL(*mRegionAllocator, consumerRequiresSeparateRegion(kSinkHubId))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kRegionId))
      .WillOnce(Invoke([](int32_t id) { return SharedDataRegion{.id = id}; }));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink, _))
      .WillOnce(Return(pw::OkStatus()));

  ASSERT_TRUE(mDataFlowManager->addOffloadSink(params).ok());

  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->removeDataFlow(flowId);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(result.value(), ::testing::ElementsAre(sink));
}

TEST_F(DataFlowManagerTest, AddHostSinkSuccess) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  constexpr int32_t kPrimaryRegionId = 10;
  constexpr int32_t kSinkMetadataRegionId = 11;

  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kPrimaryRegionId))
      .WillOnce(Invoke([](int32_t) {
        return pw::Result<SharedDataRegion>(
            SharedDataRegion{.id = kPrimaryRegionId});
      }));
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kSinkMetadataRegionId))
      .WillOnce(Invoke([](int32_t) {
        return pw::Result<SharedDataRegion>(
            SharedDataRegion{.id = kSinkMetadataRegionId});
      }));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, source, _))
      .WillOnce(Return(pw::OkStatus()));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink, _))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->addHostSink(
      flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId,
      /* metadataOffset= */ 0,
      /* sinkMetadataOffset= */ 0);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().id, flowId);

  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto removeResult = mDataFlowManager->removeDataFlow(flowId);
  ASSERT_TRUE(removeResult.ok());
  EXPECT_THAT(removeResult.value(), ::testing::ElementsAre(sink));
}

TEST_F(DataFlowManagerTest, AddHostSinkExistingDataFlow) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink1{.id = 2, .hubId = kSinkHubId};
  EndpointId sink2{.id = 3, .hubId = kSinkHubId};
  constexpr int32_t kPrimaryRegionId = 10;
  constexpr int32_t kSinkMetadataRegionId = 11;

  // Add first sink
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kPrimaryRegionId))
      .WillRepeatedly(Invoke([](int32_t) {
        return pw::Result<SharedDataRegion>(
            SharedDataRegion{.id = kPrimaryRegionId});
      }));
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kSinkMetadataRegionId))
      .WillRepeatedly(Invoke([](int32_t) {
        return pw::Result<SharedDataRegion>(
            SharedDataRegion{.id = kSinkMetadataRegionId});
      }));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, source, _))
      .WillOnce(Return(pw::OkStatus()));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink1, _))
      .WillOnce(Return(pw::OkStatus()));

  ASSERT_TRUE(mDataFlowManager
                  ->addHostSink(flowId, source, sink1, kPrimaryRegionId,
                                kSinkMetadataRegionId, 0, 0)
                  .ok());

  // Add second sink
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink2, _))
      .WillOnce(Return(pw::OkStatus()));

  ASSERT_TRUE(mDataFlowManager
                  ->addHostSink(flowId, source, sink2, kPrimaryRegionId,
                                kSinkMetadataRegionId, 0, 0)
                  .ok());

  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto removeResult = mDataFlowManager->removeDataFlow(flowId);
  ASSERT_TRUE(removeResult.ok());
  EXPECT_THAT(removeResult.value(),
              ::testing::UnorderedElementsAre(sink1, sink2));
}

TEST_F(DataFlowManagerTest, AddHostSinkFailures) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  constexpr int32_t kPrimaryRegionId = 10;
  constexpr int32_t kSinkMetadataRegionId = 11;

  // Region not found
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kPrimaryRegionId))
      .WillOnce(Return(pw::Status::NotFound()));
  EXPECT_EQ(mDataFlowManager
                ->addHostSink(flowId, source, sink, kPrimaryRegionId,
                              kSinkMetadataRegionId, 0, 0)
                .status(),
            pw::Status::NotFound());

  // Existing data flow checks
  // First setup a valid data flow
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kPrimaryRegionId))
      .WillRepeatedly(Invoke([](int32_t) {
        return pw::Result<SharedDataRegion>(
            SharedDataRegion{.id = kPrimaryRegionId});
      }));
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kSinkMetadataRegionId))
      .WillRepeatedly(Invoke([](int32_t) {
        return pw::Result<SharedDataRegion>(
            SharedDataRegion{.id = kSinkMetadataRegionId});
      }));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, source, _))
      .WillOnce(Return(pw::OkStatus()));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink, _))
      .WillOnce(Return(pw::OkStatus()));

  ASSERT_TRUE(mDataFlowManager
                  ->addHostSink(flowId, source, sink, kPrimaryRegionId,
                                kSinkMetadataRegionId, 0, 0)
                  .ok());

  // Source mismatch
  EndpointId wrongSource{.id = 99, .hubId = kHubId};
  EndpointId sink2{.id = 3, .hubId = kSinkHubId};
  EXPECT_EQ(mDataFlowManager
                ->addHostSink(flowId, wrongSource, sink2, kPrimaryRegionId,
                              kSinkMetadataRegionId, 0, 0)
                .status(),
            pw::Status::AlreadyExists());

  // Sink already exists
  EXPECT_EQ(mDataFlowManager
                ->addHostSink(flowId, source, sink, kPrimaryRegionId,
                              kSinkMetadataRegionId, 0, 0)
                .status(),
            pw::Status::AlreadyExists());

  // Metadata offset mismatch
  EXPECT_EQ(mDataFlowManager
                ->addHostSink(flowId, source, sink2, kPrimaryRegionId,
                              kSinkMetadataRegionId, 100, 0)
                .status(),
            pw::Status::AlreadyExists());
}

TEST_F(DataFlowManagerTest, AddHostSinkTriggerFailure) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  constexpr int32_t kPrimaryRegionId = 10;
  constexpr int32_t kSinkMetadataRegionId = 11;

  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kPrimaryRegionId))
      .WillRepeatedly(Invoke([](int32_t) {
        return pw::Result<SharedDataRegion>(
            SharedDataRegion{.id = kPrimaryRegionId});
      }));
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kSinkMetadataRegionId))
      .WillRepeatedly(Invoke([](int32_t) {
        return pw::Result<SharedDataRegion>(
            SharedDataRegion{.id = kSinkMetadataRegionId});
      }));

  // Test trigger failure for source (new flow)
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, source, _))
      .WillOnce(Return(pw::Status::Internal()));

  EXPECT_EQ(mDataFlowManager
                ->addHostSink(flowId, source, sink, kPrimaryRegionId,
                              kSinkMetadataRegionId, 0, 0)
                .status(),
            pw::Status::Internal());

  // Test trigger failure for sink (new flow, source succeed, sink fail)
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, source, _))
      .WillOnce(Return(pw::OkStatus()));
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink, _))
      .WillOnce(Return(pw::Status::Internal()));

  // Should trigger cleanup
  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  EXPECT_EQ(mDataFlowManager
                ->addHostSink(flowId, source, sink, kPrimaryRegionId,
                              kSinkMetadataRegionId, 0, 0)
                .status(),
            pw::Status::Internal());
}

}  // namespace
}  // namespace android::hardware::contexthub::common::implementation