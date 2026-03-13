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

#include "chre_api/chre.h"
#include "gtest/gtest.h"
#include "inc/test_util.h"
#include "test_base.h"
#include "test_event.h"

namespace chre {
namespace {

CREATE_CHRE_TEST_EVENT(TEST_CREATE_FIXED_DATA_FLOW, 0);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_FIXED_DATA_FLOW_2, 1);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_VARIABLE_DATA_FLOW, 2);
CREATE_CHRE_TEST_EVENT(TEST_DESTROY_DATA_FLOW, 3);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_WITH_NULL_NAME, 4);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_WITH_UNGRANTED_PERMISSION, 5);
CREATE_CHRE_TEST_EVENT(TEST_DESTROY_INVALID_DATA_FLOW, 6);
CREATE_CHRE_TEST_EVENT(TEST_DESTROY_UNOWNED_DATA_FLOW, 7);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_WITH_DUPLICATE_NAME, 8);

class DataFlowTestApp : public TestNanoapp {
 public:
  DataFlowTestApp(uint32_t &dataFlowId, const TestNanoappInfo &info)
      : TestNanoapp(info), mDataFlowId(dataFlowId) {}

  void handleEvent(uint32_t, uint16_t eventType,
                   const void *eventData) override {
    switch (eventType) {
      case CHRE_EVENT_DATA_FLOW_CREATED: {
        auto *info = static_cast<const chreDataFlowCreatedInfo *>(eventData);
        EXPECT_EQ(info->status, CHRE_STATUS_OK);
        EXPECT_EQ(info->size, mExpectedSize);
        EXPECT_EQ(info->sinkDomains, mExpectedSinkDomains);
        EXPECT_EQ(info->permissions, mExpectedPermissions);
        mDataFlowId = info->dataFlowId;
        triggerWait(CHRE_EVENT_DATA_FLOW_CREATED);
        break;
      }
      case CHRE_EVENT_TEST_EVENT: {
        auto *event = static_cast<const TestEvent *>(eventData);
        switch (event->type) {
          case TEST_CREATE_FIXED_DATA_FLOW: {
            mExpectedSize = 100;
            mExpectedSinkDomains = CHRE_DATA_FLOW_SINK_DOMAIN_LOCAL_NANOAPP;
            mExpectedPermissions = CHRE_MESSAGE_PERMISSION_NONE;

            uint32_t status = chreDataFlowCreateAsync(
                /*sinkDomains=*/mExpectedSinkDomains,
                /*minAverageWriteIntervalNs=*/1000,
                /*maxAverageWriteBandwidthBytesPerSecond=*/1000,
                mExpectedPermissions,
                /*elementSize=*/10, /*alignment=*/4, /*minElementCount=*/10,
                /*maxElementCount=*/10, "test_data_flow");
            EXPECT_EQ(status, CHRE_STATUS_OK);
            break;
          }
          case TEST_CREATE_FIXED_DATA_FLOW_2: {
            mExpectedSize = 100;
            mExpectedSinkDomains = CHRE_DATA_FLOW_SINK_DOMAIN_LOCAL_NANOAPP;
            mExpectedPermissions = CHRE_MESSAGE_PERMISSION_NONE;

            uint32_t status = chreDataFlowCreateAsync(
                /*sinkDomains=*/mExpectedSinkDomains,
                /*minAverageWriteIntervalNs=*/1000,
                /*maxAverageWriteBandwidthBytesPerSecond=*/1000,
                mExpectedPermissions,
                /*elementSize=*/10, /*alignment=*/4, /*minElementCount=*/10,
                /*maxElementCount=*/10, "test_data_flow_2");
            EXPECT_EQ(status, CHRE_STATUS_OK);
            break;
          }
          case TEST_CREATE_VARIABLE_DATA_FLOW: {
            mExpectedSize = 100;
            mExpectedSinkDomains = CHRE_DATA_FLOW_SINK_DOMAIN_LOCAL_NANOAPP;
            mExpectedPermissions = CHRE_MESSAGE_PERMISSION_NONE;

            uint32_t status = chreDataFlowCreateAsync(
                /*sinkDomains=*/mExpectedSinkDomains,
                /*minAverageWriteIntervalNs=*/1000,
                /*maxAverageWriteBandwidthBytesPerSecond=*/1000,
                mExpectedPermissions,
                /*elementSize=*/0, /*alignment=*/0, /*minElementCount=*/10,
                /*maxElementCount=*/100, "test_data_flow");
            EXPECT_EQ(status, CHRE_STATUS_OK);
            break;
          }
          case TEST_DESTROY_DATA_FLOW: {
            uint32_t status = chreDataFlowDestroy(mDataFlowId);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            triggerWait(TEST_DESTROY_DATA_FLOW);
            break;
          }
          case TEST_CREATE_WITH_NULL_NAME: {
            uint32_t status = chreDataFlowCreateAsync(
                /*sinkDomains=*/CHRE_DATA_FLOW_SINK_DOMAIN_LOCAL_NANOAPP,
                /*minAverageWriteIntervalNs=*/1000,
                /*maxAverageWriteBandwidthBytesPerSecond=*/1000,
                /*sinkPermissions=*/CHRE_MESSAGE_PERMISSION_NONE,
                /*elementSize=*/10, /*alignment=*/4, /*minElementCount=*/10,
                /*maxElementCount=*/10, /*name=*/nullptr);
            EXPECT_EQ(status, CHRE_STATUS_INVALID_ARGUMENT);
            triggerWait(TEST_CREATE_WITH_NULL_NAME);
            break;
          }
          case TEST_CREATE_WITH_UNGRANTED_PERMISSION: {
            uint32_t status = chreDataFlowCreateAsync(
                /*sinkDomains=*/CHRE_DATA_FLOW_SINK_DOMAIN_LOCAL_NANOAPP,
                /*minAverageWriteIntervalNs=*/1000,
                /*maxAverageWriteBandwidthBytesPerSecond=*/1000,
                /*sinkPermissions=*/CHRE_MESSAGE_PERMISSION_AUDIO,
                /*elementSize=*/10, /*alignment=*/4, /*minElementCount=*/10,
                /*maxElementCount=*/10, "test_data_flow");
            EXPECT_EQ(status, CHRE_STATUS_PERMISSION_DENIED);
            triggerWait(TEST_CREATE_WITH_UNGRANTED_PERMISSION);
            break;
          }
          case TEST_DESTROY_INVALID_DATA_FLOW: {
            uint32_t status = chreDataFlowDestroy(CHRE_DATA_FLOW_ID_INVALID);
            EXPECT_EQ(status, CHRE_STATUS_NOT_FOUND);
            triggerWait(TEST_DESTROY_INVALID_DATA_FLOW);
            break;
          }
          case TEST_DESTROY_UNOWNED_DATA_FLOW: {
            uint32_t status = chreDataFlowDestroy(mDataFlowId);
            EXPECT_EQ(status, CHRE_STATUS_PERMISSION_DENIED);
            triggerWait(TEST_DESTROY_UNOWNED_DATA_FLOW);
            break;
          }
          case TEST_CREATE_WITH_DUPLICATE_NAME: {
            uint32_t status = chreDataFlowCreateAsync(
                /*sinkDomains=*/CHRE_DATA_FLOW_SINK_DOMAIN_LOCAL_NANOAPP,
                /*minAverageWriteIntervalNs=*/1000,
                /*maxAverageWriteBandwidthBytesPerSecond=*/1000,
                /*sinkPermissions=*/CHRE_MESSAGE_PERMISSION_NONE,
                /*elementSize=*/10, /*alignment=*/4, /*minElementCount=*/10,
                /*maxElementCount=*/10, "test_data_flow");
            EXPECT_EQ(status, CHRE_STATUS_ALREADY_EXISTS);
            triggerWait(TEST_CREATE_WITH_DUPLICATE_NAME);
            break;
          }
        }
      }
    }
  }

 private:
  uint32_t &mDataFlowId;
  uint32_t mExpectedSize = 0;
  uint32_t mExpectedSinkDomains = 0;
  uint32_t mExpectedPermissions = 0;
};

class DataFlowTestApp2 : public DataFlowTestApp {
 public:
  DataFlowTestApp2(uint32_t &dataFlowId, const TestNanoappInfo &info)
      : DataFlowTestApp(dataFlowId, info) {}
};

class DataFlowTest : public SingleThreadTestBase {};

TEST_F(DataFlowTest, CreateAndDestroyFixedSizeDataFlow) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_DESTROY_DATA_FLOW,
                            TEST_DESTROY_DATA_FLOW);
}

TEST_F(DataFlowTest, CreateAndDestroyVariableSizeDataFlow) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  sendEventToNanoappAndWait(appId, TEST_CREATE_VARIABLE_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_DESTROY_DATA_FLOW,
                            TEST_DESTROY_DATA_FLOW);
}

TEST_F(DataFlowTest, CreateWithNullNameReturnsInvalidArgument) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  sendEventToNanoappAndWait(appId, TEST_CREATE_WITH_NULL_NAME,
                            TEST_CREATE_WITH_NULL_NAME);
}

TEST_F(DataFlowTest, CreateWithUngrantedPermissionReturnsPermissionDenied) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  sendEventToNanoappAndWait(appId, TEST_CREATE_WITH_UNGRANTED_PERMISSION,
                            TEST_CREATE_WITH_UNGRANTED_PERMISSION);
}

TEST_F(DataFlowTest, DestroyInvalidDataFlowReturnsNotFound) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  sendEventToNanoappAndWait(appId, TEST_DESTROY_INVALID_DATA_FLOW,
                            TEST_DESTROY_INVALID_DATA_FLOW);
}

TEST_F(DataFlowTest, DestroyUnownedDataFlowReturnsPermissionDenied) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info1 = {.name = "DataFlowTest1", .id = 0x1234};
  uint64_t appId1 = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info1));
  ASSERT_NE(getNanoappByAppId(appId1), nullptr);

  TestNanoappInfo info2 = {.name = "DataFlowTest2", .id = 0x1235};
  uint64_t appId2 =
      loadNanoapp(MakeUnique<DataFlowTestApp2>(dataFlowId, info2));
  ASSERT_NE(getNanoappByAppId(appId2), nullptr);

  sendEventToNanoappAndWait(appId1, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId2, TEST_DESTROY_UNOWNED_DATA_FLOW,
                            TEST_DESTROY_UNOWNED_DATA_FLOW);
}

TEST_F(DataFlowTest, EnsureRegionIsDeallocatedAfterDataFlowDestroyed) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  PlatformSharedDataRegionManagerBase *manager =
      reinterpret_cast<PlatformSharedDataRegionManagerBase *>(
          &EventLoopManagerSingleton::get()->getSharedDataRegionManager());
  manager->resetNumCallsToDeallocateRegion();

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_DESTROY_DATA_FLOW,
                            TEST_DESTROY_DATA_FLOW);
  EXPECT_EQ(manager->getNumCallsToDeallocateRegion(), 1);
  manager->resetNumCallsToDeallocateRegion();
}

TEST_F(DataFlowTest, RegionIsDeallocatedWhenAllDataFlowsAreDestroyed) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  PlatformSharedDataRegionManagerBase *manager =
      reinterpret_cast<PlatformSharedDataRegionManagerBase *>(
          &EventLoopManagerSingleton::get()->getSharedDataRegionManager());
  manager->resetNumCallsToDeallocateRegion();

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);
  uint32_t dataFlowId1 = dataFlowId;

  // Create a second data flow which will share the same region as the first on
  // the linux platform.
  dataFlowId = CHRE_DATA_FLOW_ID_INVALID;
  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW_2,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);
  uint32_t dataFlowId2 = dataFlowId;

  // Destroy the first data flow, but not the second. The region should not be
  // deallocated since it is still in use by the second data flow.
  dataFlowId = dataFlowId1;
  sendEventToNanoappAndWait(appId, TEST_DESTROY_DATA_FLOW,
                            TEST_DESTROY_DATA_FLOW);
  EXPECT_EQ(manager->getNumCallsToDeallocateRegion(), 0);

  // Now that the second data flow is destroyed, the region should be
  // deallocated.
  dataFlowId = dataFlowId2;
  sendEventToNanoappAndWait(appId, TEST_DESTROY_DATA_FLOW,
                            TEST_DESTROY_DATA_FLOW);
  EXPECT_EQ(manager->getNumCallsToDeallocateRegion(), 1);
  manager->resetNumCallsToDeallocateRegion();
}

TEST_F(DataFlowTest, CreateWithDuplicateNameReturnsAlreadyExists) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_CREATE_WITH_DUPLICATE_NAME,
                            TEST_CREATE_WITH_DUPLICATE_NAME);
}

// TODO(b/457453613): Test destroy sends the appropriate events to registered
// sinks - nanoapp or not.

}  // namespace
}  // namespace chre
