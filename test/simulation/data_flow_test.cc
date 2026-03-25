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

#include "chre/util/system/message_hub_callback_v2.h"
#include "chre/util/system/message_router_mocks.h"
#include "chre_api/chre.h"
#include "gtest/gtest.h"
#include "inc/test_util.h"
#include "test_base.h"
#include "test_event.h"

#include <condition_variable>
#include <mutex>

namespace chre {
namespace {

using ::chre::message::DataFlowSinkRegistration;
using ::chre::message::EndpointInfo;
using ::chre::message::EndpointType;
using ::chre::message::MessageHubInfo;
using ::chre::message::MockMessageHubCallbackV2;
using ::chre::message::SESSION_ID_INVALID;
using ::chre::message::SessionId;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

constexpr uint64_t kTestHubId = 0x1234567812345678;
constexpr uint64_t kTestEndpointId = 0x8765432187654321;

CREATE_CHRE_TEST_EVENT(TEST_CREATE_FIXED_DATA_FLOW, 0);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_FIXED_DATA_FLOW_2, 1);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_VARIABLE_DATA_FLOW, 2);
CREATE_CHRE_TEST_EVENT(TEST_DESTROY_DATA_FLOW, 3);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_WITH_NULL_NAME, 4);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_WITH_UNGRANTED_PERMISSION, 5);
CREATE_CHRE_TEST_EVENT(TEST_DESTROY_INVALID_DATA_FLOW, 6);
CREATE_CHRE_TEST_EVENT(TEST_DESTROY_UNOWNED_DATA_FLOW, 7);
CREATE_CHRE_TEST_EVENT(TEST_CREATE_WITH_DUPLICATE_NAME, 8);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_ADD_SINK_NO_MESSAGE, 9);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_ADD_SINK_WITH_MESSAGE, 10);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_CONFIGURE_SINK, 11);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_RESERVE, 12);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_COMMIT, 13);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_PUSH, 14);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_GET_SIZE_EMPTY, 15);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_GET_CAPACITY_EMPTY, 16);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_GET_SIZE_ONE_ELEMENT, 17);
CREATE_CHRE_TEST_EVENT(TEST_SOURCE_GET_CAPACITY_ONE_ELEMENT, 18);

class DataFlowTestApp : public TestNanoapp {
 public:
  DataFlowTestApp(uint32_t &dataFlowId, const TestNanoappInfo &info,
                  SessionId *sessionId = nullptr)
      : TestNanoapp(info), mDataFlowId(dataFlowId), mSessionId(sessionId) {}

  void handleEvent(uint32_t, uint16_t eventType,
                   const void *eventData) override {
    switch (eventType) {
      case CHRE_EVENT_DATA_FLOW_CREATED: {
        auto *info = static_cast<const chreDataFlowCreatedInfo *>(eventData);
        EXPECT_EQ(info->status, CHRE_STATUS_OK);
        EXPECT_EQ(info->size, mExpectedSize);
        EXPECT_EQ(info->sinkDomains, mExpectedSinkDomains);
        EXPECT_EQ(info->permissions, mExpectedPermissions);
        EXPECT_EQ(info->dataFlowId, mDataFlowId);
        triggerWait(CHRE_EVENT_DATA_FLOW_CREATED);
        break;
      }
      case CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE: {
        auto *info =
            static_cast<const chreDataFlowSinkConfigureInfo *>(eventData);
        EXPECT_EQ(info->status, CHRE_STATUS_OK);
        EXPECT_EQ(info->dataFlowId, mDataFlowId);
        EXPECT_EQ(info->hubId, kTestHubId);
        EXPECT_EQ(info->endpointId, kTestEndpointId);
        triggerWait(CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE);
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
                /*maxElementCount=*/10, "test_data_flow", &mDataFlowId);
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
                /*maxElementCount=*/10, "test_data_flow_2", &mDataFlowId);
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
                /*maxElementCount=*/100, "test_data_flow", &mDataFlowId);
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
                /*maxElementCount=*/10, /*name=*/nullptr, &mDataFlowId);
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
                /*maxElementCount=*/10, "test_data_flow", &mDataFlowId);
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
                /*maxElementCount=*/10, "test_data_flow", &mDataFlowId);
            EXPECT_EQ(status, CHRE_STATUS_ALREADY_EXISTS);
            triggerWait(TEST_CREATE_WITH_DUPLICATE_NAME);
            break;
          }
          case TEST_SOURCE_ADD_SINK_NO_MESSAGE: {
            chreDataFlowSinkPolicy policy = {};
            policy.overwritePolicy =
                CHRE_DATA_FLOW_SINK_OVERWRITE_POLICY_ALLOWED;
            policy.newDataAlertPolicy =
                CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_NEVER;

            uint32_t status = chreDataFlowSourceAddSinkAsync(
                /*hubId=*/kTestHubId, /*endpointId=*/kTestEndpointId,
                /*dataFlowId=*/mDataFlowId, &policy);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            break;
          }
          case TEST_SOURCE_ADD_SINK_WITH_MESSAGE: {
            chreDataFlowSinkPolicy policy = {};
            policy.overwritePolicy =
                CHRE_DATA_FLOW_SINK_OVERWRITE_POLICY_ALLOWED;
            policy.newDataAlertPolicy =
                CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_NEVER;

            uint32_t *msgData =
                static_cast<uint32_t *>(chreHeapAlloc(4 * sizeof(uint32_t)));
            msgData[0] = 1;
            msgData[1] = 2;
            msgData[2] = 3;
            msgData[3] = 4;

            EXPECT_NE(mSessionId, nullptr);
            uint32_t status = chreDataFlowSourceAddSinkOverSessionAsync(
                /*hubId=*/kTestHubId, /*endpointId=*/kTestEndpointId,
                /*dataFlowId=*/mDataFlowId, &policy, msgData,
                /*messageSize=*/4 * sizeof(uint32_t), /*messageType=*/123,
                /*sessionId=*/*mSessionId, CHRE_MESSAGE_PERMISSION_NONE,
                [](void *message, size_t /*messageSize*/) {
                  chreHeapFree(message);
                });
            EXPECT_EQ(status, CHRE_STATUS_OK);
            break;
          }
          case TEST_SOURCE_CONFIGURE_SINK: {
            chreDataFlowSinkPolicy policy = {};
            policy.overwritePolicy =
                CHRE_DATA_FLOW_SINK_OVERWRITE_POLICY_ALLOWED;
            policy.newDataAlertPolicy =
                CHRE_DATA_FLOW_SINK_NEW_DATA_ALERT_POLICY_PERIODIC;
            policy.newDataAlertPolicyData.periodMs = 1000;

            uint32_t status = chreDataFlowSourceConfigureSink(
                /*hubId=*/kTestHubId, /*endpointId=*/kTestEndpointId,
                /*dataFlowId=*/mDataFlowId, &policy);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            triggerWait(TEST_SOURCE_CONFIGURE_SINK);
            break;
          }
          case TEST_SOURCE_RESERVE: {
            void *data = nullptr;
            uint32_t reservedBytes = 0;
            uint32_t status = chreDataFlowSourceReserve(
                /*dataFlowId=*/mDataFlowId, /*numBytes=*/10, &data,
                &reservedBytes);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            EXPECT_NE(data, nullptr);
            EXPECT_EQ(reservedBytes, 10);
            if (data != nullptr && reservedBytes >= 10) {
              const char msg[] = "DEADBEEF\0";
              memcpy(data, msg, 10);
            }
            triggerWait(TEST_SOURCE_RESERVE);
            break;
          }
          case TEST_SOURCE_COMMIT: {
            uint32_t status = chreDataFlowSourceCommit(
                /*dataFlowId=*/mDataFlowId, /*numBytes=*/10);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            triggerWait(TEST_SOURCE_COMMIT);
            break;
          }
          case TEST_SOURCE_PUSH: {
            uint8_t data[10];
            memcpy(data, "DEADBEEF\0", 10);
            uint32_t numberOfBytesPushed = 0;
            uint32_t status = chreDataFlowSourcePush(
                /*dataFlowId=*/mDataFlowId, data, /*numBytes=*/sizeof(data),
                /*allOrNothing=*/true, &numberOfBytesPushed);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            EXPECT_EQ(numberOfBytesPushed, sizeof(data));
            triggerWait(TEST_SOURCE_PUSH);
            break;
          }
          case TEST_SOURCE_GET_SIZE_EMPTY: {
            uint32_t size = 0;
            uint32_t status = chreDataFlowSourceGetSize(
                /*dataFlowId=*/mDataFlowId, /*includeReserved=*/false, &size);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            EXPECT_EQ(size, 0);
            triggerWait(TEST_SOURCE_GET_SIZE_EMPTY);
            break;
          }
          case TEST_SOURCE_GET_CAPACITY_EMPTY: {
            uint32_t capacity = 0;
            uint32_t status = chreDataFlowSourceGetCapacity(
                /*dataFlowId=*/mDataFlowId, &capacity);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            EXPECT_EQ(capacity, 100);  // 10 elements max
            triggerWait(TEST_SOURCE_GET_CAPACITY_EMPTY);
            break;
          }
          case TEST_SOURCE_GET_SIZE_ONE_ELEMENT: {
            uint32_t size = 0;
            uint32_t status = chreDataFlowSourceGetSize(
                /*dataFlowId=*/mDataFlowId, /*includeReserved=*/false, &size);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            EXPECT_EQ(size, 10);  // 1 element pushed
            triggerWait(TEST_SOURCE_GET_SIZE_ONE_ELEMENT);
            break;
          }
          case TEST_SOURCE_GET_CAPACITY_ONE_ELEMENT: {
            uint32_t capacity = 0;
            uint32_t status = chreDataFlowSourceGetCapacity(
                /*dataFlowId=*/mDataFlowId, &capacity);
            EXPECT_EQ(status, CHRE_STATUS_OK);
            EXPECT_EQ(capacity, 100);  // capacity shouldn't change
            triggerWait(TEST_SOURCE_GET_CAPACITY_ONE_ELEMENT);
            break;
          }
        }
      }
    }
  }

 private:
  uint32_t &mDataFlowId;
  SessionId *mSessionId;
  uint32_t mExpectedSize = 0;
  uint32_t mExpectedSinkDomains = 0;
  uint32_t mExpectedPermissions = 0;
};

class DataFlowTestApp2 : public DataFlowTestApp {
 public:
  DataFlowTestApp2(uint32_t &dataFlowId, const TestNanoappInfo &info,
                   SessionId *sessionId = nullptr)
      : DataFlowTestApp(dataFlowId, info, sessionId) {}
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

TEST_F(DataFlowTest, SourceAddSinkNoMessage) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  // Register the hub.
  auto callback =
      pw::MakeRefCounted<testing::NiceMock<MockMessageHubCallbackV2>>();
  std::optional<EndpointInfo> endpointInfo = EndpointInfo(
      kTestEndpointId, "test_endpoint", 1, EndpointType::HOST_NATIVE, 0);
  EXPECT_CALL(*callback, getEndpointInfo(kTestEndpointId))
      .WillRepeatedly(Return(endpointInfo));
  EXPECT_CALL(*callback, onRegisterDataFlowSink(_))
      .WillOnce(
          Invoke([&dataFlowId, appId](DataFlowSinkRegistration &&registration) {
            EXPECT_FALSE(registration.sessionMessage.has_value());
            EXPECT_EQ(registration.dataFlowId.id, dataFlowId);
            EXPECT_EQ(registration.sourceId.endpointId, appId);
            EXPECT_EQ(registration.sinkId.messageHubId, kTestHubId);
            EXPECT_EQ(registration.sinkId.endpointId, kTestEndpointId);
          }));

  MessageHubInfo messageHubInfo = {
      .id = kTestHubId,
      .name = "TEST_HUB",
      .sharedDataCapabilities = {.dataFlowsSupported = true}};
  std::optional<message::MessageRouter::MessageHub> messageHub =
      message::MessageRouterSingleton::get()->registerMessageHubV2(
          messageHubInfo, callback);
  ASSERT_TRUE(messageHub.has_value());

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_ADD_SINK_NO_MESSAGE,
                            CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE);
}

TEST_F(DataFlowTest, SourceAddSinkWithMessage) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;
  SessionId sessionId = SESSION_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId =
      loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info, &sessionId));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  auto callback =
      pw::MakeRefCounted<testing::NiceMock<MockMessageHubCallbackV2>>();
  std::optional<EndpointInfo> endpointInfo = EndpointInfo(
      kTestEndpointId, "test_endpoint", 1, EndpointType::HOST_NATIVE, 0);
  EXPECT_CALL(*callback, getEndpointInfo(kTestEndpointId))
      .WillRepeatedly(Return(endpointInfo));
  EXPECT_CALL(*callback, forEachEndpoint(_))
      .WillRepeatedly(
          Invoke([&endpointInfo](
                     const pw::Function<bool(const EndpointInfo &)> &function) {
            function(*endpointInfo);
          }));
  EXPECT_CALL(*callback, onRegisterDataFlowSink(_))
      .WillOnce(
          Invoke([&dataFlowId, appId](DataFlowSinkRegistration &&registration) {
            EXPECT_TRUE(registration.sessionMessage.has_value());
            EXPECT_EQ(registration.dataFlowId.id, dataFlowId);
            EXPECT_EQ(registration.sourceId.endpointId, appId);
            EXPECT_EQ(registration.sinkId.messageHubId, kTestHubId);
            EXPECT_EQ(registration.sinkId.endpointId, kTestEndpointId);

            const auto &msg = registration.sessionMessage.value();
            EXPECT_EQ(msg.messageType, 123);
            const uint32_t *data =
                reinterpret_cast<const uint32_t *>(msg.data.get());
            EXPECT_EQ(data[0], 1);
            EXPECT_EQ(data[1], 2);
            EXPECT_EQ(data[2], 3);
            EXPECT_EQ(data[3], 4);
          }));

  MessageHubInfo messageHubInfo = {
      .id = kTestHubId,
      .name = "TEST_HUB",
      .sharedDataCapabilities = {.dataFlowsSupported = true}};
  std::optional<message::MessageRouter::MessageHub> messageHub =
      message::MessageRouterSingleton::get()->registerMessageHubV2(
          messageHubInfo, callback);
  ASSERT_TRUE(messageHub.has_value());

  // TODO: We may not need this
  EXPECT_CALL(*callback, onSessionOpenRequest(_))
      .WillRepeatedly(Invoke([&messageHub](const message::Session &session) {
        if (messageHub.has_value()) {
          messageHub->onSessionOpenComplete(session.sessionId);
        }
      }));

  std::mutex sessionMutex;
  std::condition_variable sessionCondVar;
  bool sessionOpened = false;

  EXPECT_CALL(*callback, onSessionOpened(_))
      .WillRepeatedly(Invoke([&sessionMutex, &sessionCondVar, &sessionOpened](
                                 const message::Session & /*session*/) {
        std::lock_guard<std::mutex> lock(sessionMutex);
        sessionOpened = true;
        sessionCondVar.notify_one();
      }));

  sessionId = messageHub->openSession(kTestEndpointId, CHRE_PLATFORM_ID, appId);
  EXPECT_NE(sessionId, SESSION_ID_INVALID);

  {
    std::unique_lock<std::mutex> lock(sessionMutex);
    sessionCondVar.wait(lock, [&sessionOpened]() { return sessionOpened; });
  }

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_ADD_SINK_WITH_MESSAGE,
                            CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE);
}

TEST_F(DataFlowTest, SourceConfigureSink) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  auto callback =
      pw::MakeRefCounted<testing::NiceMock<MockMessageHubCallbackV2>>();
  std::optional<EndpointInfo> endpointInfo = EndpointInfo(
      kTestEndpointId, "test_endpoint", 1, EndpointType::HOST_NATIVE, 0);
  EXPECT_CALL(*callback, getEndpointInfo(kTestEndpointId))
      .WillRepeatedly(Return(endpointInfo));
  EXPECT_CALL(*callback, onRegisterDataFlowSink(_)).Times(1);

  MessageHubInfo messageHubInfo = {
      .id = kTestHubId,
      .name = "TEST_HUB",
      .sharedDataCapabilities = {.dataFlowsSupported = true}};
  std::optional<message::MessageRouter::MessageHub> messageHub =
      message::MessageRouterSingleton::get()->registerMessageHubV2(
          messageHubInfo, callback);
  ASSERT_TRUE(messageHub.has_value());

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_ADD_SINK_NO_MESSAGE,
                            CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_CONFIGURE_SINK,
                            TEST_SOURCE_CONFIGURE_SINK);
}

TEST_F(DataFlowTest, SourceReserveAndCommit) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  std::optional<android::contexthub::data_flow::UntypedConsumer> testConsumer;
  int32_t testRegionId = -1;

  auto callback =
      pw::MakeRefCounted<testing::NiceMock<MockMessageHubCallbackV2>>();
  std::optional<EndpointInfo> endpointInfo = EndpointInfo(
      kTestEndpointId, "test_endpoint", 1, EndpointType::HOST_NATIVE, 0);
  EXPECT_CALL(*callback, getEndpointInfo(kTestEndpointId))
      .WillRepeatedly(Return(endpointInfo));
  EXPECT_CALL(*callback, onRegisterDataFlowSink(_))
      .WillOnce(Invoke([&testConsumer, &testRegionId](
                           DataFlowSinkRegistration &&registration) {
        testRegionId = registration.primaryRegionId;
        auto maybeRegion =
            EventLoopManagerSingleton::get()
                ->getSharedDataRegionManager()
                .incrementRegionRefCount(registration.primaryRegionId);
        ASSERT_TRUE(maybeRegion.ok());
        android::contexthub::data_flow::RemoteNotifyArgs notifyArgs{
            .fn =
                [](const android::contexthub::data_flow::RemoteEndpointId &) {},
            .id = {},
        };
        LOGE("Creating consumer: %" PRIuPTR ": %" PRIu32,
             maybeRegion->first.base, maybeRegion->first.size);
        LOGE("Metadata offset: %" PRIu32 ", sink metadata offset: %" PRIu32,
             registration.metadataOffset, registration.sinkMetadataOffset);
        auto maybeConsumer =
            android::contexthub::data_flow::UntypedConsumer::createRemote(
                maybeRegion->first, std::nullopt, registration.metadataOffset,
                registration.sinkMetadataOffset, std::move(notifyArgs),
                maybeRegion->second);
        ASSERT_TRUE(maybeConsumer.ok());
        testConsumer.emplace(std::move(maybeConsumer.value()));
      }));

  MessageHubInfo messageHubInfo = {
      .id = kTestHubId,
      .name = "TEST_HUB",
      .sharedDataCapabilities = {.dataFlowsSupported = true}};
  std::optional<message::MessageRouter::MessageHub> messageHub =
      message::MessageRouterSingleton::get()->registerMessageHubV2(
          messageHubInfo, callback);
  ASSERT_TRUE(messageHub.has_value());

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_ADD_SINK_NO_MESSAGE,
                            CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE);
  ASSERT_TRUE(testConsumer.has_value());

  sendEventToNanoappAndWait(appId, TEST_SOURCE_RESERVE, TEST_SOURCE_RESERVE);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_COMMIT, TEST_SOURCE_COMMIT);

  std::byte pullBuffer[10] = {std::byte{0}};
  auto pullResult = testConsumer->pop(pw::span<std::byte>(pullBuffer));
  ASSERT_TRUE(pullResult.ok());
  EXPECT_STREQ(reinterpret_cast<const char *>(pullBuffer), "DEADBEEF");

  EXPECT_TRUE(EventLoopManagerSingleton::get()
                  ->getSharedDataRegionManager()
                  .decrementRegionRefCount(testRegionId)
                  .ok());
}

TEST_F(DataFlowTest, SourcePush) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  std::optional<android::contexthub::data_flow::UntypedConsumer> testConsumer;
  int32_t testRegionId = -1;

  auto callback =
      pw::MakeRefCounted<testing::NiceMock<MockMessageHubCallbackV2>>();
  std::optional<EndpointInfo> endpointInfo = EndpointInfo(
      kTestEndpointId, "test_endpoint", 1, EndpointType::HOST_NATIVE, 0);
  EXPECT_CALL(*callback, getEndpointInfo(kTestEndpointId))
      .WillRepeatedly(Return(endpointInfo));
  EXPECT_CALL(*callback, onRegisterDataFlowSink(_))
      .WillOnce(Invoke([&testConsumer, &testRegionId](
                           DataFlowSinkRegistration &&registration) {
        testRegionId = registration.primaryRegionId;
        auto maybeRegion =
            EventLoopManagerSingleton::get()
                ->getSharedDataRegionManager()
                .incrementRegionRefCount(registration.primaryRegionId);
        ASSERT_TRUE(maybeRegion.ok());
        android::contexthub::data_flow::RemoteNotifyArgs notifyArgs{
            .fn =
                [](const android::contexthub::data_flow::RemoteEndpointId &) {},
            .id = {},
        };
        auto maybeConsumer =
            android::contexthub::data_flow::UntypedConsumer::createRemote(
                maybeRegion->first, std::nullopt, registration.metadataOffset,
                registration.sinkMetadataOffset, std::move(notifyArgs),
                maybeRegion->second);
        ASSERT_TRUE(maybeConsumer.ok());
        testConsumer.emplace(std::move(maybeConsumer.value()));
      }));

  MessageHubInfo messageHubInfo = {
      .id = kTestHubId,
      .name = "TEST_HUB",
      .sharedDataCapabilities = {.dataFlowsSupported = true}};
  std::optional<message::MessageRouter::MessageHub> messageHub =
      message::MessageRouterSingleton::get()->registerMessageHubV2(
          messageHubInfo, callback);
  ASSERT_TRUE(messageHub.has_value());

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_ADD_SINK_NO_MESSAGE,
                            CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE);
  ASSERT_TRUE(testConsumer.has_value());

  sendEventToNanoappAndWait(appId, TEST_SOURCE_PUSH, TEST_SOURCE_PUSH);

  std::byte pullBuffer[10] = {std::byte{0}};
  auto pullResult = testConsumer->pop(pw::span<std::byte>(pullBuffer));
  ASSERT_TRUE(pullResult.ok());
  EXPECT_STREQ(reinterpret_cast<const char *>(pullBuffer), "DEADBEEF");

  EXPECT_TRUE(EventLoopManagerSingleton::get()
                  ->getSharedDataRegionManager()
                  .decrementRegionRefCount(testRegionId)
                  .ok());
}

TEST_F(DataFlowTest, SourceGetSizeAndCapacity) {
  uint32_t dataFlowId = CHRE_DATA_FLOW_ID_INVALID;

  TestNanoappInfo info = {.name = "DataFlowTest", .id = 0x1234};
  uint64_t appId = loadNanoapp(MakeUnique<DataFlowTestApp>(dataFlowId, info));
  ASSERT_NE(getNanoappByAppId(appId), nullptr);

  auto callback =
      pw::MakeRefCounted<testing::NiceMock<MockMessageHubCallbackV2>>();
  std::optional<EndpointInfo> endpointInfo = EndpointInfo(
      kTestEndpointId, "test_endpoint", 1, EndpointType::HOST_NATIVE, 0);
  EXPECT_CALL(*callback, getEndpointInfo(kTestEndpointId))
      .WillRepeatedly(Return(endpointInfo));

  MessageHubInfo messageHubInfo = {
      .id = kTestHubId,
      .name = "TEST_HUB",
      .sharedDataCapabilities = {.dataFlowsSupported = true}};
  std::optional<message::MessageRouter::MessageHub> messageHub =
      message::MessageRouterSingleton::get()->registerMessageHubV2(
          messageHubInfo, callback);
  ASSERT_TRUE(messageHub.has_value());

  sendEventToNanoappAndWait(appId, TEST_CREATE_FIXED_DATA_FLOW,
                            CHRE_EVENT_DATA_FLOW_CREATED);
  EXPECT_NE(dataFlowId, CHRE_DATA_FLOW_ID_INVALID);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_ADD_SINK_NO_MESSAGE,
                            CHRE_EVENT_DATA_FLOW_SINK_CONFIGURE_DONE);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_GET_SIZE_EMPTY,
                            TEST_SOURCE_GET_SIZE_EMPTY);
  sendEventToNanoappAndWait(appId, TEST_SOURCE_GET_CAPACITY_EMPTY,
                            TEST_SOURCE_GET_CAPACITY_EMPTY);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_PUSH, TEST_SOURCE_PUSH);

  sendEventToNanoappAndWait(appId, TEST_SOURCE_GET_SIZE_ONE_ELEMENT,
                            TEST_SOURCE_GET_SIZE_ONE_ELEMENT);
  sendEventToNanoappAndWait(appId, TEST_SOURCE_GET_CAPACITY_ONE_ELEMENT,
                            TEST_SOURCE_GET_CAPACITY_ONE_ELEMENT);
}

// TODO(b/457453613): Test destroy sends the appropriate events to registered
// sinks - nanoapp or not.
// TODO(b/457453613): Test adding a nanoapp sink with and without a message.
// TODO(b/457453613): Test reserve/commit/push/size/capacity with a variable
// data flow.
// TODO(b/457453613): Test invalid message data when creating a sink.

}  // namespace
}  // namespace chre
