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
#include <unistd.h>

#include <cstdint>
#include <memory>
#include <optional>

#include <aidl/android/hardware/contexthub/DataFlowAlertFds.h>
#include <aidl/android/hardware/contexthub/DataFlowInfo.h>
#include <aidl/android/hardware/contexthub/DataFlowSinkContext.h>
#include <aidl/android/hardware/contexthub/DataFlowSinkRegistrationParams.h>
#include <aidl/android/hardware/contexthub/EndpointId.h>
#include <aidl/android/hardware/contexthub/SharedDataRegionRequirements.h>
#include <android-base/unique_fd.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "android/binder_auto_utils.h"
#include "data_flow_epoll_waiter.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "region_allocator.h"
#include "wakelock_manager.h"

namespace android::hardware::contexthub::common::implementation {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowAlertFds;
using ::aidl::android::hardware::contexthub::DataFlowInfo;
using ::aidl::android::hardware::contexthub::DataFlowSinkContext;
using ::aidl::android::hardware::contexthub::DataFlowSinkRegistrationParams;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::aidl::android::hardware::contexthub::SharedDataRegionRequirements;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

constexpr int64_t kHubId = 123;
constexpr int32_t kRegionId = 1;
constexpr int64_t kSinkHubId = 456;
constexpr int32_t kPrimaryRegionId = 10;
constexpr int32_t kSinkMetadataRegionId = 11;

#define ASSERT_RESULT_OK_AND_ASSIGN(lhs, rhs) \
  auto rhs_ = (rhs);                          \
  ASSERT_TRUE(rhs_.ok());                     \
  lhs = std::move(rhs_.value());

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

  using DataFlowManager::onAlert;
  using DataFlowManager::onWakingAck;
};

class DataFlowManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mRegionAllocator = std::make_shared<MockRegionAllocator>();
    mWakelockManager = std::make_shared<MockWakelockManager>();
    mDataFlowManager = std::make_unique<TestableDataFlowManager>(
        mRegionAllocator, mWakelockManager,
        [this](DataFlowId dataFlowId, EndpointId receiver, bool waking) {
          return sendAlert(dataFlowId, receiver, waking);
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

  DataFlowAlertFds createAlertFds(bool isHost) {
    DataFlowAlertFds fds;
    fds.waking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK));
    fds.nonWaking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK));
    fds.halAck =
        ndk::ScopedFileDescriptor(isHost ? eventfd(0, EFD_NONBLOCK) : -1);
    return fds;
  }

  pw::Result<DataFlowId> createHostSourceDataFlow(EndpointId source,
                                                  int32_t regionId) {
    allocateRegion(regionId);
    DataFlowInfo info{.region = {.id = regionId},
                      .alertFds = createAlertFds(true)};

    EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
        .WillOnce(Return(pw::OkStatus()));

    return mDataFlowManager->addHostSourceDataFlow(source, info);
  }

  void addOffloadSink(DataFlowId flowId, EndpointId source, EndpointId sink,
                      int32_t regionId) {
    DataFlowSinkRegistrationParams params{
        .context = {.id = flowId, .alertFds = createAlertFds(false)},
        .sourceId = source,
        .sinkId = sink,
    };

    EXPECT_CALL(*mRegionAllocator, consumerRequiresSeparateRegion(sink.hubId))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mRegionAllocator, getRegionInfo(regionId))
        .WillOnce(
            Invoke([](int32_t id) { return SharedDataRegion{.id = id}; }));
    EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink, _))
        .WillOnce(Return(pw::OkStatus()));

    ASSERT_TRUE(mDataFlowManager->addOffloadSink(params).ok());
  }

  pw::Result<DataFlowSinkContext> createOffloadSourceDataFlowAndHostSink(
      DataFlowId flowId, EndpointId source, EndpointId sink,
      int32_t primaryRegionId, int32_t sinkMetadataRegionId) {
    EXPECT_CALL(*mRegionAllocator, getRegionInfo(primaryRegionId))
        .WillRepeatedly(Invoke([](int32_t id) {
          return pw::Result<SharedDataRegion>(SharedDataRegion{.id = id});
        }));
    EXPECT_CALL(*mRegionAllocator, getRegionInfo(sinkMetadataRegionId))
        .WillRepeatedly(Invoke([](int32_t id) {
          return pw::Result<SharedDataRegion>(SharedDataRegion{.id = id});
        }));
    EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, source, _))
        .WillOnce(Return(pw::OkStatus()));
    EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink, _))
        .WillOnce(Return(pw::OkStatus()));

    return mDataFlowManager->addHostSink(flowId, source, sink, primaryRegionId,
                                         sinkMetadataRegionId, 0, 0);
  }

  MOCK_METHOD(pw::Status, sendAlert, (DataFlowId, EndpointId, bool));

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
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

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
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));
  EXPECT_EQ(flowId.hubId, kHubId);
}

TEST_F(DataFlowManagerTest, AddHostSourceDataFlowRegionNotAllocated) {
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId},
                    .alertFds = createAlertFds(true)};

  auto result = mDataFlowManager->addHostSourceDataFlow(source, info);
  EXPECT_EQ(result.status(), pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, AddHostSourceDataFlowWrongRegionId) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId + 1},
                    .alertFds = createAlertFds(true)};

  auto result = mDataFlowManager->addHostSourceDataFlow(source, info);
  EXPECT_EQ(result.status(), pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, AddHostSourceDataFlowTriggerFailure) {
  allocateRegion(kRegionId);
  EndpointId source{.id = 1, .hubId = kHubId};
  DataFlowInfo info{.region = {.id = kRegionId},
                    .alertFds = createAlertFds(true)};

  EXPECT_CALL(*mEpollWaiter, addTriggers(_, _, _))
      .WillOnce(Return(pw::Status::Internal()));

  auto result = mDataFlowManager->addHostSourceDataFlow(source, info);
  EXPECT_EQ(result.status(), pw::Status::Internal());
}

TEST_F(DataFlowManagerTest, AddOffloadSinkSuccess) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  addOffloadSink(flowId, source, sink, kRegionId);
}

TEST_F(DataFlowManagerTest, AddOffloadSinkSeparateRegion) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  DataFlowSinkRegistrationParams params{
      .context = {.id = flowId, .alertFds = createAlertFds(false)},
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
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  DataFlowSinkRegistrationParams params{
      .context = {.id = flowId, .alertFds = createAlertFds(false)},
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
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->removeDataFlow(flowId);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(DataFlowManagerTest, RemoveDataFlowWithSinks) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  addOffloadSink(flowId, source, sink, kRegionId);

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

  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));
  EXPECT_EQ(context.id, flowId);

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

  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink1, kPrimaryRegionId, kSinkMetadataRegionId));

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

  // Region not found
  EXPECT_CALL(*mRegionAllocator, getRegionInfo(kPrimaryRegionId))
      .WillOnce(Return(pw::Status::NotFound()));
  EXPECT_EQ(mDataFlowManager
                ->addHostSink(flowId, source, sink, kPrimaryRegionId,
                              kSinkMetadataRegionId, 0, 0)
                .status(),
            pw::Status::NotFound());

  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));

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

TEST_F(DataFlowManagerTest, RemoveSinkSuccess) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  addOffloadSink(flowId, source, sink, kRegionId);

  // Expect removal of triggers for the sink
  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>(sink)))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->removeSink(flowId, sink);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), source);
}

TEST_F(DataFlowManagerTest, RemoveSinkDataFlowNotFound) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};

  EXPECT_EQ(mDataFlowManager->removeSink(flowId, sink).status(),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, RemoveSinkSinkNotFound) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EndpointId sink{.id = 2, .hubId = kSinkHubId};

  EXPECT_EQ(mDataFlowManager->removeSink(flowId, sink).status(),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, PruneEndpointSource) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  // Expect removal of triggers for the flow (source)
  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->pruneEndpoint(source);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.value().size(), 1);
  EXPECT_EQ(result.value()[0].dataFlowId, flowId);
  EXPECT_TRUE(result.value()[0].isSource);
  EXPECT_TRUE(result.value()[0].endpoints.empty());

  // Data flow should be gone
  EXPECT_EQ(mDataFlowManager->removeDataFlow(flowId).status(),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, PruneEndpointSourceWithSink) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  addOffloadSink(flowId, source, sink, kRegionId);

  // Expect removal of triggers for the flow
  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->pruneEndpoint(source);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.value().size(), 1);
  EXPECT_EQ(result.value()[0].dataFlowId, flowId);
  EXPECT_TRUE(result.value()[0].isSource);
  EXPECT_THAT(result.value()[0].endpoints, ::testing::ElementsAre(sink));

  // Data flow should be gone
  EXPECT_EQ(mDataFlowManager->removeDataFlow(flowId).status(),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, PruneEndpointSink) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  addOffloadSink(flowId, source, sink, kRegionId);

  // Expect removal of triggers for the sink
  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>(sink)))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->pruneEndpoint(sink);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.value().size(), 1);
  EXPECT_EQ(result.value()[0].dataFlowId, flowId);
  EXPECT_FALSE(result.value()[0].isSource);
  EXPECT_THAT(result.value()[0].endpoints, ::testing::ElementsAre(source));

  // Sink should be gone from data flow
  EXPECT_EQ(mDataFlowManager->removeSink(flowId, sink).status(),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, PruneEndpointNotFound) {
  EndpointId endpoint{.id = 1, .hubId = kHubId};
  EXPECT_EQ(mDataFlowManager->pruneEndpoint(endpoint).status(),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, RemoveSinkHostSinkSuccess) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};

  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));

  // Expect removal of triggers for the flow, because this is the last host sink
  // on an offload data flow.
  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->removeSink(flowId, sink);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), source);

  // Data flow should be gone
  EXPECT_EQ(mDataFlowManager->removeDataFlow(flowId).status(),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, RemoveSinkHostSinkNotLast) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink1{.id = 2, .hubId = kSinkHubId};
  EndpointId sink2{.id = 3, .hubId = kSinkHubId};

  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink1, kPrimaryRegionId, kSinkMetadataRegionId));

  // Add second sink
  EXPECT_CALL(*mEpollWaiter, addTriggers(flowId, sink2, _))
      .WillOnce(Return(pw::OkStatus()));

  ASSERT_TRUE(mDataFlowManager
                  ->addHostSink(flowId, source, sink2, kPrimaryRegionId,
                                kSinkMetadataRegionId, 0, 0)
                  .ok());

  // Remove first sink. Should remove triggers for that sink, but NOT the flow.
  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>(sink1)))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->removeSink(flowId, sink1);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), source);

  // Data flow should still exist.
  // We can verify this by checking that removing the second sink succeeds.
  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  result = mDataFlowManager->removeSink(flowId, sink2);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), source);

  // Now data flow should be gone.
  EXPECT_EQ(mDataFlowManager->removeDataFlow(flowId).status(),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, PruneEndpointHostSink) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};

  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));

  // Pruning the endpoint should trigger removal of the flow since it's the last
  // host sink on an offload data flow.
  EXPECT_CALL(*mEpollWaiter, removeTriggers(std::optional<DataFlowId>(flowId),
                                            std::optional<EndpointId>()))
      .WillOnce(Return(pw::OkStatus()));

  auto result = mDataFlowManager->pruneEndpoint(sink);
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(result.value().size(), 1);
  EXPECT_EQ(result.value()[0].dataFlowId, flowId);
  EXPECT_FALSE(result.value()[0].isSource);
  EXPECT_THAT(result.value()[0].endpoints, ::testing::ElementsAre(source));

  // Data flow should be gone
  EXPECT_EQ(mDataFlowManager->removeDataFlow(flowId).status(),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, VerifyHostSourceDataFlowSource) {
  EndpointId hostSource{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(hostSource, kRegionId));

  EXPECT_EQ(mDataFlowManager->verifyEndpointOnDataFlow(flowId, hostSource,
                                                       /*isHost=*/true),
            pw::OkStatus());
  EXPECT_EQ(mDataFlowManager->verifyEndpointOnDataFlow(flowId, hostSource,
                                                       /*isHost=*/false),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, VerifyHostSourceDataFlowSink) {
  EndpointId hostSource{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(hostSource, kRegionId));
  EndpointId offloadSink{.id = 2, .hubId = kSinkHubId};
  addOffloadSink(flowId, hostSource, offloadSink, kRegionId);

  EXPECT_EQ(mDataFlowManager->verifyEndpointOnDataFlow(flowId, offloadSink,
                                                       /*isHost=*/false),
            pw::OkStatus());
  EXPECT_EQ(mDataFlowManager->verifyEndpointOnDataFlow(flowId, offloadSink,
                                                       /*isHost=*/true),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, VerifyOffloadSourceDataFlowSource) {
  DataFlowId offloadFlowId{.hubId = kSinkHubId, .id = 100};
  EndpointId offloadSource{.id = 3, .hubId = kSinkHubId};
  EndpointId hostSink{.id = 4, .hubId = kHubId};

  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowSinkContext context,
                              createOffloadSourceDataFlowAndHostSink(
                                  offloadFlowId, offloadSource, hostSink,
                                  kPrimaryRegionId, kSinkMetadataRegionId));

  EXPECT_EQ(mDataFlowManager->verifyEndpointOnDataFlow(
                offloadFlowId, offloadSource, /*isHost=*/false),
            pw::OkStatus());
  EXPECT_EQ(mDataFlowManager->verifyEndpointOnDataFlow(
                offloadFlowId, offloadSource, /*isHost=*/true),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, VerifyOffloadSourceDataFlowSink) {
  DataFlowId offloadFlowId{.hubId = kSinkHubId, .id = 100};
  EndpointId offloadSource{.id = 3, .hubId = kSinkHubId};
  EndpointId hostSink{.id = 4, .hubId = kHubId};

  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowSinkContext context,
                              createOffloadSourceDataFlowAndHostSink(
                                  offloadFlowId, offloadSource, hostSink,
                                  kPrimaryRegionId, kSinkMetadataRegionId));

  EXPECT_EQ(mDataFlowManager->verifyEndpointOnDataFlow(offloadFlowId, hostSink,
                                                       /*isHost=*/true),
            pw::OkStatus());
  EXPECT_EQ(mDataFlowManager->verifyEndpointOnDataFlow(offloadFlowId, hostSink,
                                                       /*isHost=*/false),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, VerifyEndpointOnDataFlowFailure) {
  EndpointId hostSource{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(hostSource, kRegionId));
  EndpointId randomEndpoint{.id = 99, .hubId = kHubId};

  EXPECT_EQ(mDataFlowManager->verifyEndpointOnDataFlow(flowId, randomEndpoint,
                                                       /*isHost=*/true),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, AlertHostEndpointsWaking) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};

  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext sinkContext,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));

  EXPECT_CALL(*mWakelockManager,
              increaseWakeCount(WakelockManager::Usage::kDataFlow, 1))
      .WillOnce(Return(pw::OkStatus()));

  EXPECT_EQ(mDataFlowManager->alertHostEndpoints(flowId, {sink},
                                                 /*isWaking=*/true),
            pw::OkStatus());

  uint64_t counter;
  ASSERT_EQ(read(sinkContext.alertFds.waking.get(), &counter, sizeof(counter)),
            sizeof(counter));
  EXPECT_EQ(counter, 1);
}

TEST_F(DataFlowManagerTest, AlertHostEndpointsNonWaking) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};

  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext sinkContext,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));

  EXPECT_EQ(mDataFlowManager->alertHostEndpoints(flowId, {sink},
                                                 /*isWaking=*/false),
            pw::OkStatus());

  uint64_t counter;
  ASSERT_EQ(
      read(sinkContext.alertFds.nonWaking.get(), &counter, sizeof(counter)),
      sizeof(counter));
  EXPECT_EQ(counter, 1);
}

TEST_F(DataFlowManagerTest, AlertHostEndpointsInvalidArgument) {
  EndpointId hostSource{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId hostFlowId,
                              createHostSourceDataFlow(hostSource, kRegionId));

  EXPECT_EQ(mDataFlowManager->alertHostEndpoints(hostFlowId, {hostSource},
                                                 /*isWaking=*/true),
            pw::Status::InvalidArgument());
}

TEST_F(DataFlowManagerTest, AlertHostEndpointsDataFlowNotFound) {
  DataFlowId unknownFlowId{.hubId = 999, .id = 999};
  EndpointId hostSource{.id = 1, .hubId = kHubId};

  EXPECT_EQ(mDataFlowManager->alertHostEndpoints(unknownFlowId, {hostSource},
                                                 /*isWaking=*/true),
            pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, AlertHostEndpointsEndpointNotFound) {
  DataFlowId offloadFlowId{.hubId = kSinkHubId, .id = 100};
  EndpointId offloadSource{.id = 3, .hubId = kSinkHubId};
  EndpointId hostSink{.id = 4, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowSinkContext context,
                              createOffloadSourceDataFlowAndHostSink(
                                  offloadFlowId, offloadSource, hostSink,
                                  kPrimaryRegionId, kSinkMetadataRegionId));

  EndpointId unknownEndpoint{.id = 99, .hubId = kHubId};
  EXPECT_EQ(
      mDataFlowManager->alertHostEndpoints(offloadFlowId, {unknownEndpoint},
                                           /*isWaking=*/true),
      pw::OkStatus());
}

TEST_F(DataFlowManagerTest, OnAlertSuccess) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EXPECT_CALL(*this, sendAlert(flowId, source, true))
      .WillOnce(Return(pw::OkStatus()));

  mDataFlowManager->onAlert(flowId, source, true);
}

TEST_F(DataFlowManagerTest, OnAlertFailure) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));

  EXPECT_CALL(*this, sendAlert(flowId, source, false))
      .WillOnce(Return(pw::Status::Internal()));

  mDataFlowManager->onAlert(flowId, source, false);
}

TEST_F(DataFlowManagerTest, OnAlertUnknownEndpoint) {
  EndpointId source{.id = 1, .hubId = kHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(DataFlowId flowId,
                              createHostSourceDataFlow(source, kRegionId));
  EndpointId unknown{.id = 99, .hubId = kHubId};

  mDataFlowManager->onAlert(flowId, unknown, true);
}

TEST_F(DataFlowManagerTest, OnWakingAckSuccess) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));

  // Simulate waking alert to increment wake count
  EXPECT_CALL(*mWakelockManager,
              increaseWakeCount(WakelockManager::Usage::kDataFlow, 1))
      .WillOnce(Return(pw::OkStatus()));
  EXPECT_EQ(mDataFlowManager->alertHostEndpoints(flowId, {sink},
                                                 /*isWaking=*/true),
            pw::OkStatus());

  EXPECT_CALL(*mWakelockManager,
              decreaseWakeCount(WakelockManager::Usage::kDataFlow, 1))
      .WillOnce(Return(pw::OkStatus()));

  mDataFlowManager->onWakingAck(flowId, sink, 1);
}

TEST_F(DataFlowManagerTest, OnWakingAckPartial) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));

  // Simulate waking alert
  EXPECT_CALL(*mWakelockManager,
              increaseWakeCount(WakelockManager::Usage::kDataFlow, 1))
      .WillOnce(Return(pw::OkStatus()));
  EXPECT_EQ(mDataFlowManager->alertHostEndpoints(flowId, {sink},
                                                 /*isWaking=*/true),
            pw::OkStatus());

  // Decrement only what was acked (1 out of 1 in this test, but logic should
  // hold)
  EXPECT_CALL(*mWakelockManager,
              decreaseWakeCount(WakelockManager::Usage::kDataFlow, 1))
      .WillOnce(Return(pw::OkStatus()));
  mDataFlowManager->onWakingAck(flowId, sink, 1);
}

TEST_F(DataFlowManagerTest, OnWakingAckZero) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));

  mDataFlowManager->onWakingAck(flowId, sink, 0);
}

TEST_F(DataFlowManagerTest, OnWakingAckUnknownEndpoint) {
  DataFlowId flowId{.hubId = kHubId, .id = 1};
  EndpointId source{.id = 1, .hubId = kHubId};
  EndpointId sink{.id = 2, .hubId = kSinkHubId};
  ASSERT_RESULT_OK_AND_ASSIGN(
      DataFlowSinkContext context,
      createOffloadSourceDataFlowAndHostSink(
          flowId, source, sink, kPrimaryRegionId, kSinkMetadataRegionId));

  EndpointId unknown{.id = 99, .hubId = kHubId};
  mDataFlowManager->onWakingAck(flowId, unknown, 1);
}

}  // namespace
}  // namespace android::hardware::contexthub::common::implementation