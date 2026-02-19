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

#include <cstdint>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "pw_result/result.h"
#include "pw_status/status.h"
#include "region_allocator.h"
#include "wakelock_manager.h"

namespace android::hardware::contexthub::common::implementation {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

constexpr int64_t kHubId = 123;
constexpr int32_t kRegionId = 1;

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

class DataFlowManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mRegionAllocator = std::make_shared<MockRegionAllocator>();
    mWakelockManager = std::make_shared<MockWakelockManager>();
    mDataFlowManager = std::make_unique<DataFlowManager>(
        mRegionAllocator, mWakelockManager,
        [this](DataFlowId dataFlowId, EndpointId sender, EndpointId receiver,
               bool waking) {
          return sendAlert(dataFlowId, sender, receiver, waking);
        });
  }

  MOCK_METHOD(pw::Status, sendAlert,
              (DataFlowId, EndpointId, EndpointId, bool));

  std::shared_ptr<MockRegionAllocator> mRegionAllocator;
  std::shared_ptr<MockWakelockManager> mWakelockManager;
  std::unique_ptr<DataFlowManager> mDataFlowManager;
};

TEST_F(DataFlowManagerTest, AllocateRegionSuccess) {
  SharedDataRegionRequirements requirements;

  EXPECT_CALL(*mRegionAllocator, allocateRegion(_)).WillOnce(Invoke([] {
    SharedDataRegion region;
    region.id = kRegionId;
    return region;
  }));

  auto result = mDataFlowManager->allocateRegion(kHubId, requirements);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().id, kRegionId);
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
  SharedDataRegionRequirements requirements;

  // First allocate a region
  EXPECT_CALL(*mRegionAllocator, allocateRegion(_)).WillOnce(Invoke([] {
    SharedDataRegion region;
    region.id = kRegionId;
    return region;
  }));

  auto result = mDataFlowManager->allocateRegion(kHubId, requirements);
  ASSERT_TRUE(result.ok());

  // Now release it
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
  SharedDataRegionRequirements requirements;

  // First allocate a region for the hub
  EXPECT_CALL(*mRegionAllocator, allocateRegion(_)).WillOnce(Invoke([] {
    SharedDataRegion region;
    region.id = kRegionId;
    return region;
  }));

  auto result = mDataFlowManager->allocateRegion(kHubId, requirements);
  ASSERT_TRUE(result.ok());

  // Now attempt to release a different region ID
  constexpr int32_t kWrongRegionId = 2;
  auto status = mDataFlowManager->releaseRegion(kHubId, kWrongRegionId);
  ASSERT_EQ(status, pw::Status::NotFound());
}

TEST_F(DataFlowManagerTest, ReleaseRegionInUse) {
  // Skipping this case until we can implement the rest of the public API.
}

}  // namespace
}  // namespace android::hardware::contexthub::common::implementation
