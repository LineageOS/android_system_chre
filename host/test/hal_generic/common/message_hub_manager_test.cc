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

#include "message_hub_manager.h"

#include <cstddef>
#include <memory>
#include <random>
#include <unordered_map>

#include <aidl/android/hardware/contexthub/IContextHub.h>

#include "android/binder_auto_utils.h"
#include "chre/platform/log.h"
#include "data_flow_manager.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace android::hardware::contexthub::common::implementation {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::DataFlowInfo;
using ::aidl::android::hardware::contexthub::DataFlowSinkContext;
using ::aidl::android::hardware::contexthub::DataFlowSinkRegistrationParams;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::aidl::android::hardware::contexthub::EndpointInfo;
using ::aidl::android::hardware::contexthub::ErrorCode;
using ::aidl::android::hardware::contexthub::HubInfo;
using ::aidl::android::hardware::contexthub::IEndpointCallback;
using ::aidl::android::hardware::contexthub::IEndpointCommunication;
using ::aidl::android::hardware::contexthub::Message;
using ::aidl::android::hardware::contexthub::MessageDeliveryStatus;
using ::aidl::android::hardware::contexthub::Reason;
using ::aidl::android::hardware::contexthub::SharedDataRegion;
using ::aidl::android::hardware::contexthub::SharedDataRegionRequirements;
using ::ndk::ScopedAStatus;
using ::ndk::SharedRefBase;
using ::ndk::SpAIBinder;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Ge;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Le;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;

class MockRegisterOffloadSinkCallback
    : public IEndpointCommunication::IRegisterOffloadSinkCallback {
 public:
  MOCK_METHOD(ScopedAStatus, addSinkInRegion,
              (const std::optional<SharedDataRegion> &, int64_t *), (override));

  MOCK_METHOD(SpAIBinder, asBinder, (), (override));
  MOCK_METHOD(bool, isRemote, (), (override));
  MOCK_METHOD(ScopedAStatus, getInterfaceVersion, (int32_t *), (override));
  MOCK_METHOD(ScopedAStatus, getInterfaceHash, (std::string *), (override));
};

class MockEndpointCallback : public IEndpointCallback {
 public:
  MOCK_METHOD(ScopedAStatus, onEndpointStarted,
              (const std::vector<EndpointInfo> &), (override));
  MOCK_METHOD(ScopedAStatus, onEndpointStopped,
              (const std::vector<EndpointId> &, Reason), (override));
  MOCK_METHOD(ScopedAStatus, onMessageReceived, (int32_t, const Message &),
              (override));
  MOCK_METHOD(ScopedAStatus, onMessageDeliveryStatusReceived,
              (int32_t, const MessageDeliveryStatus &), (override));
  MOCK_METHOD(ScopedAStatus, onEndpointSessionOpenRequest,
              (int32_t, const EndpointId &, const EndpointId &,
               const std::optional<std::string> &),
              (override));
  MOCK_METHOD(ScopedAStatus, onCloseEndpointSession, (int32_t, Reason),
              (override));
  MOCK_METHOD(ScopedAStatus, onEndpointSessionOpenComplete, (int32_t),
              (override));
  MOCK_METHOD(ScopedAStatus, onDataFlowHostSinkRegistered,
              (const DataFlowSinkRegistrationParams &), (override));
  MOCK_METHOD(ScopedAStatus, onDataFlowOffloadEndpointUnregistered,
              (const DataFlowId &, const EndpointId &,
               const std::vector<EndpointId> &),
              (override));

  MOCK_METHOD(SpAIBinder, asBinder, (), (override));
  MOCK_METHOD(bool, isRemote, (), (override));
  MOCK_METHOD(ScopedAStatus, getInterfaceVersion, (int32_t *), (override));
  MOCK_METHOD(ScopedAStatus, getInterfaceHash, (std::string *), (override));

  MockEndpointCallback() {
    ON_CALL(*this, onEndpointStarted)
        .WillByDefault([](const std::vector<EndpointInfo> &) {
          return ScopedAStatus::ok();
        });
    ON_CALL(*this, onEndpointStopped)
        .WillByDefault([](const std::vector<EndpointId> &, Reason) {
          return ScopedAStatus::ok();
        });
    ON_CALL(*this, onMessageReceived)
        .WillByDefault(
            [](int32_t, const Message &) { return ScopedAStatus::ok(); });
    ON_CALL(*this, onMessageDeliveryStatusReceived)
        .WillByDefault([](int32_t, const MessageDeliveryStatus &) {
          return ScopedAStatus::ok();
        });
    ON_CALL(*this, onEndpointSessionOpenRequest)
        .WillByDefault([](int32_t, const EndpointId &, const EndpointId &,
                          const std::optional<std::string> &) {
          return ScopedAStatus::ok();
        });
    ON_CALL(*this, onCloseEndpointSession).WillByDefault([](int32_t, Reason) {
      return ScopedAStatus::ok();
    });
    ON_CALL(*this, onEndpointSessionOpenComplete).WillByDefault([](int32_t) {
      return ScopedAStatus::ok();
    });
  }
};

class MockDataFlowManager : public DataFlowManager {
 public:
  MOCK_METHOD(pw::Result<DataFlowId>, addHostSourceDataFlow,
              (EndpointId endpoint, const DataFlowInfo &info), (override));
  MOCK_METHOD(
      (pw::Result<std::pair<DataFlowInfo, std::optional<SharedDataRegion>>>),
      addOffloadSink, (const DataFlowSinkRegistrationParams &params),
      (override));
  MOCK_METHOD(pw::Result<DataFlowSinkContext>, addHostSink,
              (DataFlowId flowId, EndpointId source, EndpointId sink,
               int32_t primaryRegionId, int32_t sinkMetadataRegionId,
               uint32_t metadataOffset, uint32_t sinkMetadataOffset),
              (override));
  MOCK_METHOD((pw::Result<std::pair<EndpointId, std::vector<EndpointId>>>),
              removeDataFlow, (DataFlowId flowId), (override));
  MOCK_METHOD(pw::Result<EndpointId>, removeSink,
              (DataFlowId flowId, EndpointId sink), (override));
  MOCK_METHOD(
      pw::Result<std::vector<DataFlowManager::PrunedEndpointDataFlowEntry>>,
      pruneEndpoint, (EndpointId endpoint), (override));
  MOCK_METHOD(pw::Status, verifyEndpointOnDataFlow,
              (DataFlowId flowId, EndpointId endpoint, bool isHost),
              (override));
};

constexpr int64_t kHub1Id = 0x1, kHub2Id = 0x2;
constexpr int64_t kEndpoint1Id = 0x1, kEndpoint2Id = 0x2;
const std::string kTestServiceDescriptor = "test_service";
const HubInfo kHub1Info{.hubId = kHub1Id};
const HubInfo kHub2Info{.hubId = kHub2Id};
const Service kTestService{.serviceDescriptor = kTestServiceDescriptor};
const EndpointInfo kEndpoint1_1Info{
    .id = {.id = kEndpoint1Id, .hubId = kHub1Id}, .name = "endpoint1_1"};
const EndpointInfo kEndpoint1_2Info{
    .id = {.id = kEndpoint2Id, .hubId = kHub1Id}, .services = {kTestService}};
const EndpointInfo kEndpoint2_1Info{
    .id = {.id = kEndpoint1Id, .hubId = kHub2Id}};
const EndpointInfo kEndpoint2_2Info{
    .id = {.id = kEndpoint2Id, .hubId = kHub2Id}, .services = {kTestService}};

}  // namespace

class MessageHubManagerTest : public ::testing::Test {
 public:
  using HostHub = MessageHubManager::HostHub;
  using DeathRecipientCookie = HostHub::DeathRecipientCookie;
  using HostHubDownCb = MessageHubManager::HostHubDownCb;

  static constexpr auto kSessionIdMaxRange = HostHub::kSessionIdMaxRange;
  static constexpr auto kHostSessionIdBase =
      MessageHubManager::kHostSessionIdBase;

  void SetUp() override {
    mDataFlowManager = std::make_shared<MockDataFlowManager>();
    reinit([](std::function<pw::Result<int64_t>()>) { FAIL(); });
  }

  void TearDown() override {
    mManager->forEachHostHub([](HostHub &hub) { delete hub.mCookie; });
  }

  void reinit(HostHubDownCb cb) {
    auto deathRecipient = std::make_unique<NiceMock<MockDeathRecipient>>();
    mDeathRecipient = deathRecipient.get();
    ON_CALL(*deathRecipient, linkCallback(_, _))
        .WillByDefault(Return(pw::OkStatus()));
    ON_CALL(*deathRecipient, unlinkCallback(_, _))
        .WillByDefault(Return(pw::OkStatus()));
    mManager.reset(new MessageHubManager(
        mDataFlowManager, std::move(deathRecipient), std::move(cb)));
  }

  void onClientDeath(const std::shared_ptr<HostHub> &hub) {
    MessageHubManager::onClientDeath(hub->mCookie);
  }

  void setupDefaultHubs() {
    mManager->initEmbeddedState();
    mManager->addEmbeddedHub(kHub2Info);
    mManager->addEmbeddedEndpoint(kEndpoint2_1Info);
    mManager->setEmbeddedEndpointReady(kEndpoint2_1Info.id);
    mManager->addEmbeddedEndpoint(kEndpoint2_2Info);
    mManager->setEmbeddedEndpointReady(kEndpoint2_2Info.id);
    mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
    mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
    EXPECT_TRUE(mHostHub->addEndpoint(kEndpoint1_1Info).ok());
    EXPECT_TRUE(mHostHub->addEndpoint(kEndpoint1_2Info).ok());
  }

  uint16_t setupDefaultHubsAndSession() {
    setupDefaultHubs();
    auto range = *mHostHub->reserveSessionIdRange(1);
    EXPECT_TRUE(mHostHub
                    ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                  range.first, {}, /*hostInitiated=*/true)
                    .ok());
    EXPECT_CALL(*mHostHubCb, onEndpointSessionOpenComplete(range.first));
    EXPECT_TRUE(mManager->getHostHub(kHub1Id)
                    ->ackSession(range.first,
                                 /*hostAcked=*/false)
                    .ok());
    return range.first;
  }

 protected:
  class MockDeathRecipient : public MessageHubManager::DeathRecipient {
   public:
    MOCK_METHOD(pw::Status, linkCallback,
                (const std::shared_ptr<IEndpointCallback> &,
                 DeathRecipientCookie *),
                (override));
    MOCK_METHOD(pw::Status, unlinkCallback,
                (const std::shared_ptr<IEndpointCallback> &,
                 DeathRecipientCookie *),
                (override));
  };

  std::shared_ptr<MockDataFlowManager> mDataFlowManager;
  std::unique_ptr<MessageHubManager> mManager;
  NiceMock<MockDeathRecipient> *mDeathRecipient;

  std::shared_ptr<HostHub> mHostHub;
  std::shared_ptr<MockEndpointCallback> mHostHubCb;
};

namespace {

MATCHER_P(MatchSp, sp, "Matches an IEndpointCallback") {
  return arg.get() == sp.get();
}

TEST_F(MessageHubManagerTest, CreateAndUnregisterHostHub) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  DeathRecipientCookie *cookie;
  EXPECT_CALL(*mDeathRecipient, linkCallback(MatchSp(mHostHubCb), _))
      .WillOnce([&cookie](const std::shared_ptr<IEndpointCallback> &,
                          DeathRecipientCookie *c) {
        cookie = c;
        return pw::OkStatus();
      });
  auto statusOrHub = mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  ASSERT_TRUE(statusOrHub.ok());

  mHostHub = *statusOrHub;
  EXPECT_EQ(mHostHub->id(), kHub1Id);
  EXPECT_EQ(mHostHub, mManager->getHostHub(kHub1Id));

  EXPECT_CALL(*mDeathRecipient, unlinkCallback(MatchSp(mHostHubCb), cookie))
      .WillOnce(Return(pw::OkStatus()));
  mHostHub->unregister();
  EXPECT_EQ(mHostHub->unregister(), pw::Status::Aborted());
  EXPECT_THAT(mManager->getHostHub(kHub1Id), IsNull());
}

TEST_F(MessageHubManagerTest, CreateHostHubFails) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  EXPECT_CALL(*mDeathRecipient, linkCallback(MatchSp(mHostHubCb), _))
      .WillOnce(Return(pw::Status::Internal()));
  EXPECT_FALSE(mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0).ok());
}

TEST_F(MessageHubManagerTest, OnClientDeath) {
  bool unlinked = false;
  reinit([&unlinked](std::function<pw::Result<int64_t>()> fn) {
    auto statusOrHubId = fn();
    ASSERT_TRUE(statusOrHubId.ok());
    EXPECT_EQ(*statusOrHubId, kHub1Id);
    unlinked = true;
  });

  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  EXPECT_CALL(*mDeathRecipient, linkCallback(MatchSp(mHostHubCb), _))
      .WillOnce(Return(pw::OkStatus()));
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  EXPECT_EQ(mHostHub->id(), kHub1Id);
  EXPECT_EQ(mHostHub, mManager->getHostHub(kHub1Id));

  EXPECT_CALL(*mDeathRecipient, unlinkCallback(_, _)).Times(0);
  onClientDeath(mHostHub);
  EXPECT_THAT(mManager->getHostHub(kHub1Id), IsNull());
}

TEST_F(MessageHubManagerTest, OnClientDeathAfterUnregister) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  mHostHub->unregister();
  onClientDeath(mHostHub);
}

TEST_F(MessageHubManagerTest, InitAndClearEmbeddedState) {
  mManager->addEmbeddedHub(kHub1Info);
  EXPECT_THAT(mManager->getEmbeddedHubs(), IsEmpty());

  mManager->initEmbeddedState();
  mManager->addEmbeddedHub(kHub1Info);
  EXPECT_THAT(mManager->getEmbeddedHubs(),
              UnorderedElementsAreArray({kHub1Info}));

  mManager->clearEmbeddedState();
  EXPECT_THAT(mManager->getEmbeddedHubs(), IsEmpty());
}

TEST_F(MessageHubManagerTest, AddAndRemoveEmbeddedHub) {
  mManager->initEmbeddedState();
  mManager->addEmbeddedHub(kHub1Info);
  EXPECT_THAT(mManager->getEmbeddedHubs(),
              UnorderedElementsAreArray({kHub1Info}));

  mManager->removeEmbeddedHub(kHub1Id);
  EXPECT_THAT(mManager->getEmbeddedHubs(), IsEmpty());
}

MATCHER_P(MatchEndpointInfo, info, "Matches an EndpointInfo") {
  if (arg.id.id != info.id.id || arg.id.hubId != info.id.hubId ||
      arg.services.size() != info.services.size()) {
    return false;
  }
  for (size_t i = 0; i < arg.services.size(); ++i) {
    if (arg.services[i].serviceDescriptor !=
        info.services[i].serviceDescriptor) {
      return false;
    }
  }
  return true;
}

TEST_F(MessageHubManagerTest, AddAndRemoveEmbeddedEndpoint) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  auto hostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  mManager->initEmbeddedState();
  mManager->addEmbeddedHub(kHub2Info);

  mManager->addEmbeddedEndpoint({.id = kEndpoint2_2Info.id});
  EXPECT_THAT(mManager->getEmbeddedEndpoints(), IsEmpty());

  mManager->addEmbeddedEndpointService(kEndpoint2_2Info.id,
                                       kEndpoint2_2Info.services[0]);
  EXPECT_THAT(mManager->getEmbeddedEndpoints(), IsEmpty());

  EXPECT_CALL(*mHostHubCb, onEndpointStarted(UnorderedElementsAreArray(
                               {MatchEndpointInfo(kEndpoint2_2Info)})));
  mManager->setEmbeddedEndpointReady(kEndpoint2_2Info.id);
  EXPECT_THAT(mManager->getEmbeddedEndpoints(),
              UnorderedElementsAreArray({MatchEndpointInfo(kEndpoint2_2Info)}));

  EXPECT_CALL(*mHostHubCb, onEndpointStopped(
                               UnorderedElementsAreArray({kEndpoint2_2Info.id}),
                               Reason::ENDPOINT_GONE));
  mManager->removeEmbeddedEndpoint(kEndpoint2_2Info.id);
  EXPECT_THAT(mManager->getEmbeddedEndpoints(), IsEmpty());
}

TEST_F(MessageHubManagerTest, RemovingEmbeddedHubRemovesEndpoints) {
  mManager->initEmbeddedState();
  mManager->addEmbeddedHub(kHub2Info);
  mManager->addEmbeddedEndpoint(kEndpoint2_1Info);
  mManager->setEmbeddedEndpointReady(kEndpoint2_1Info.id);
  mManager->addEmbeddedEndpoint(kEndpoint2_2Info);
  mManager->setEmbeddedEndpointReady(kEndpoint2_2Info.id);
  EXPECT_THAT(mManager->getEmbeddedEndpoints(),
              UnorderedElementsAreArray({MatchEndpointInfo(kEndpoint2_1Info),
                                         MatchEndpointInfo(kEndpoint2_2Info)}));

  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  auto hostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  EXPECT_CALL(*mHostHubCb,
              onEndpointStopped(UnorderedElementsAreArray(
                                    {kEndpoint2_1Info.id, kEndpoint2_2Info.id}),
                                Reason::HUB_RESET));
  mManager->removeEmbeddedHub(kHub2Id);
  EXPECT_THAT(mManager->getEmbeddedEndpoints(), IsEmpty());
}

// TODO(b/425440067): Uncomment this test once fixed.
// TEST_F(MessageHubManagerTest, AddEmbeddedEndpointForUnknownHub) {
//  mManager->initEmbeddedState();
//  mManager->addEmbeddedEndpoint(kEndpoint1_1Info);
//  mManager->setEmbeddedEndpointReady(kEndpoint1_1Info.id);
//  EXPECT_THAT(mManager->getEmbeddedEndpoints(), IsEmpty());
//}

// TODO(b/425440067): Remove this test once fixed.
TEST_F(MessageHubManagerTest, AddEmbeddedEndpointForUnknownHub) {
  mManager->initEmbeddedState();
  EXPECT_THAT(mManager->getEmbeddedHubs(), IsEmpty());
  mManager->addEmbeddedEndpoint(kEndpoint1_1Info);
  mManager->setEmbeddedEndpointReady(kEndpoint1_1Info.id);
  EXPECT_THAT(mManager->getEmbeddedHubs(), SizeIs(1));
  EXPECT_THAT(mManager->getEmbeddedEndpoints(),
              UnorderedElementsAreArray({kEndpoint1_1Info}));
}

TEST_F(MessageHubManagerTest, AddAndRemoveHostEndpoint) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);

  EXPECT_TRUE(mHostHub->addEndpoint(kEndpoint1_1Info).ok());
  EXPECT_THAT(mHostHub->getEndpoints(),
              UnorderedElementsAreArray({kEndpoint1_1Info}));

  EXPECT_TRUE(mHostHub->removeEndpoint(kEndpoint1_1Info.id).ok());
  EXPECT_THAT(mHostHub->getEndpoints(), IsEmpty());
}

TEST_F(MessageHubManagerTest, AddDuplicateEndpointId) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  ASSERT_TRUE(mHostHub->addEndpoint(kEndpoint1_1Info).ok());
  EndpointInfo duplicate = kEndpoint1_1Info;
  duplicate.name = "notEndpoint1_1";
  EXPECT_EQ(mHostHub->addEndpoint(duplicate), pw::Status::AlreadyExists());
  EXPECT_EQ(mHostHub->addEndpoint(kEndpoint1_1Info),
            pw::Status::AlreadyExists());
}

TEST_F(MessageHubManagerTest, AddDuplicateEndpointName) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  ASSERT_TRUE(mHostHub->addEndpoint(kEndpoint1_1Info).ok());
  EndpointInfo duplicate = kEndpoint1_1Info;
  duplicate.id.id++;
  EXPECT_EQ(mHostHub->addEndpoint(duplicate), pw::Status::AlreadyExists());
}

TEST_F(MessageHubManagerTest, RemoveNonexistentEndpoint) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  EXPECT_EQ(mHostHub->removeEndpoint(kEndpoint1_1Info.id).status(),
            pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, ReserveSessionIdRange) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  std::random_device rand;
  auto range = *mHostHub->reserveSessionIdRange(
      std::uniform_int_distribution<size_t>(1, kSessionIdMaxRange)(rand));
  EXPECT_THAT(range.second - range.first + 1,
              AllOf(Ge(1), Le(kSessionIdMaxRange)));
}

TEST_F(MessageHubManagerTest, ReserveBadSessionIdRange) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  EXPECT_EQ(mHostHub->reserveSessionIdRange(0).status(),
            pw::Status::InvalidArgument());
  EXPECT_EQ(mHostHub->reserveSessionIdRange(kSessionIdMaxRange + 1).status(),
            pw::Status::InvalidArgument());
}

TEST_F(MessageHubManagerTest, ReserveSessionIdRangeFull) {
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  int iterations = (USHRT_MAX - kHostSessionIdBase + 1) / kSessionIdMaxRange;
  for (int i = 0; i < iterations; ++i)
    ASSERT_TRUE(mHostHub->reserveSessionIdRange(kSessionIdMaxRange).ok());
  EXPECT_EQ(mHostHub->reserveSessionIdRange(kSessionIdMaxRange).status(),
            pw::Status::ResourceExhausted());
}

TEST_F(MessageHubManagerTest, OpenHostSessionRequest) {
  setupDefaultHubs();

  auto range = *mHostHub->reserveSessionIdRange(1);
  EXPECT_TRUE(mHostHub
                  ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                range.first, {}, /*hostInitiated=*/true)
                  .ok());
  EXPECT_FALSE(mHostHub->checkSessionOpen(range.first).ok());
}

TEST_F(MessageHubManagerTest, OpenHostSessionRequestBadSessionId) {
  setupDefaultHubs();

  auto range = *mHostHub->reserveSessionIdRange(1);
  EXPECT_EQ(mHostHub->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                  range.first + 1, {}, /*hostInitiated=*/true),
            pw::Status::OutOfRange());
}

TEST_F(MessageHubManagerTest, OpenEmbeddedSessionRequest) {
  setupDefaultHubs();

  static constexpr uint16_t kSessionId = 1;
  std::optional<std::string> serviceDescriptor;
  EXPECT_CALL(*mHostHubCb, onEndpointSessionOpenRequest(
                               kSessionId, kEndpoint1_1Info.id,
                               kEndpoint2_1Info.id, serviceDescriptor));
  EXPECT_TRUE(mHostHub
                  ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                kSessionId, {}, /*hostInitiated=*/false)
                  .ok());
  EXPECT_FALSE(mHostHub->checkSessionOpen(kSessionId).ok());
}

TEST_F(MessageHubManagerTest, OpenEmbeddedSessionRequestBadSessionId) {
  setupDefaultHubs();

  EXPECT_FALSE(mHostHub
                   ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                 kHostSessionIdBase, {},
                                 /*hostInitiated=*/false)
                   .ok());
  EXPECT_EQ(mHostHub->checkSessionOpen(kHostSessionIdBase),
            pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, OpenSessionRequestUnknownHostEndpoint) {
  mManager->initEmbeddedState();
  mManager->addEmbeddedHub(kHub2Info);
  mManager->addEmbeddedEndpoint(kEndpoint2_1Info);
  mManager->setEmbeddedEndpointReady(kEndpoint2_1Info.id);
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);

  auto range = *mHostHub->reserveSessionIdRange(1);
  EXPECT_EQ(mHostHub->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                  range.first, {}, /*hostInitiated=*/true),
            pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, OpenSessionRequestUnknownEmbeddedEndpoint) {
  mManager->initEmbeddedState();
  mManager->addEmbeddedHub(kHub2Info);
  mHostHubCb = SharedRefBase::make<MockEndpointCallback>();
  mHostHub = *mManager->createHostHub(mHostHubCb, kHub1Info, 0, 0);
  ASSERT_TRUE(mHostHub->addEndpoint(kEndpoint1_1Info).ok());

  auto range = *mHostHub->reserveSessionIdRange(1);
  EXPECT_EQ(mHostHub->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                  range.first, {}, /*hostInitiated=*/true),
            pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, OpenHostSessionRequestWithService) {
  setupDefaultHubs();

  auto range = *mHostHub->reserveSessionIdRange(1);
  uint16_t sessionId = range.first;
  EXPECT_TRUE(mHostHub
                  ->openSession(kEndpoint1_2Info.id, kEndpoint2_2Info.id,
                                sessionId, kTestServiceDescriptor,
                                /*hostInitiated=*/true)
                  .ok());
}

TEST_F(MessageHubManagerTest, OpenEmbeddedSessionRequestWithService) {
  setupDefaultHubs();

  auto range = *mHostHub->reserveSessionIdRange(1);
  uint16_t sessionId = range.first;
  EXPECT_TRUE(mHostHub
                  ->openSession(kEndpoint1_2Info.id, kEndpoint2_2Info.id,
                                sessionId, kTestServiceDescriptor,
                                /*hostInitiated=*/true)
                  .ok());
}

TEST_F(MessageHubManagerTest, OpenSessionWithServiceHostSideDoesNotSupport) {
  setupDefaultHubs();

  EXPECT_FALSE(mHostHub
                   ->openSession(kEndpoint1_1Info.id, kEndpoint2_2Info.id,
                                 kHostSessionIdBase, kTestServiceDescriptor,
                                 /*hostInitiated=*/true)
                   .ok());
}

TEST_F(MessageHubManagerTest,
       OpenSessionWithServiceEmbeddedSideDoesNotSupport) {
  setupDefaultHubs();

  EXPECT_FALSE(mHostHub
                   ->openSession(kEndpoint1_2Info.id, kEndpoint2_1Info.id,
                                 kHostSessionIdBase, kTestServiceDescriptor,
                                 /*hostInitiated=*/true)
                   .ok());
}

TEST_F(MessageHubManagerTest, OpenSessionRequestServiceSupportedButNotUsed) {
  setupDefaultHubs();

  std::optional<std::string> serviceDescriptor;
  auto range = *mHostHub->reserveSessionIdRange(1);
  uint16_t sessionId = range.first;
  EXPECT_TRUE(mHostHub
                  ->openSession(kEndpoint1_2Info.id, kEndpoint2_2Info.id,
                                sessionId, serviceDescriptor,
                                /*hostInitiated=*/true)
                  .ok());
}

TEST_F(MessageHubManagerTest, OpenHostSessionEmbeddedEndpointAccepts) {
  auto sessionId = setupDefaultHubsAndSession();
  EXPECT_TRUE(mHostHub->checkSessionOpen(sessionId).ok());
}

TEST_F(MessageHubManagerTest, OpenHostSessionEmbeddedEndpointRejects) {
  setupDefaultHubs();
  auto range = *mHostHub->reserveSessionIdRange(1);
  ASSERT_TRUE(mHostHub
                  ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                range.first, {}, /*hostInitiated=*/true)
                  .ok());

  EXPECT_CALL(*mHostHubCb,
              onCloseEndpointSession(
                  range.first, Reason::OPEN_ENDPOINT_SESSION_REQUEST_REJECTED));
  EXPECT_TRUE(mManager->getHostHub(kHub1Id)
                  ->closeSession(range.first,
                                 Reason::OPEN_ENDPOINT_SESSION_REQUEST_REJECTED)
                  .ok());
  EXPECT_EQ(mHostHub->checkSessionOpen(range.first), pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, OpenHostSessionHostTriesToAck) {
  setupDefaultHubs();
  auto range = *mHostHub->reserveSessionIdRange(1);
  ASSERT_TRUE(mHostHub
                  ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                range.first, {}, /*hostInitiated=*/true)
                  .ok());

  EXPECT_FALSE(mHostHub->ackSession(range.first, /*hostAcked=*/true).ok());
}

TEST_F(MessageHubManagerTest, OpenEmbeddedSessionHostEndpointAccepts) {
  setupDefaultHubs();
  static constexpr uint16_t kSessionId = 1;
  EXPECT_CALL(*mHostHubCb, onEndpointSessionOpenRequest(kSessionId, _, _, _));
  ASSERT_TRUE(mHostHub
                  ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                kSessionId, {}, /*hostInitiated=*/false)
                  .ok());

  EXPECT_TRUE(mHostHub->ackSession(kSessionId, /*hostAcked=*/true).ok());
  EXPECT_FALSE(mHostHub->checkSessionOpen(kSessionId).ok());
}

TEST_F(MessageHubManagerTest, OpenEmbeddedSessionMessageRouterTriesToAck) {
  setupDefaultHubs();
  static constexpr uint16_t kSessionId = 1;
  EXPECT_CALL(*mHostHubCb, onEndpointSessionOpenRequest(kSessionId, _, _, _));
  ASSERT_TRUE(mHostHub
                  ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                kSessionId, {}, /*hostInitiated=*/false)
                  .ok());

  EXPECT_FALSE(mHostHub->ackSession(kSessionId, /*hostAcked=*/false).ok());
}

TEST_F(MessageHubManagerTest, OpenEmbeddedSessionPrunePendingSession) {
  setupDefaultHubs();
  static constexpr uint16_t kSessionId = 1;
  EXPECT_CALL(*mHostHubCb, onEndpointSessionOpenRequest(kSessionId, _, _, _));
  EXPECT_TRUE(mHostHub
                  ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                kSessionId, {}, /*hostInitiated=*/false)
                  .ok());
  EXPECT_TRUE(mHostHub->ackSession(kSessionId, /*hostAcked=*/true).ok());

  EXPECT_CALL(*mHostHubCb, onCloseEndpointSession(kSessionId, _));
  EXPECT_CALL(*mHostHubCb, onEndpointSessionOpenRequest(kSessionId, _, _, _));
  EXPECT_TRUE(mHostHub
                  ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                kSessionId, {}, /*hostInitiated=*/false)
                  .ok());
}

TEST_F(MessageHubManagerTest, OpenEmbeddedSessionMessageRouterAcks) {
  setupDefaultHubs();
  static constexpr uint16_t kSessionId = 1;
  EXPECT_CALL(*mHostHubCb, onEndpointSessionOpenRequest(kSessionId, _, _, _));
  ASSERT_TRUE(mHostHub
                  ->openSession(kEndpoint1_1Info.id, kEndpoint2_1Info.id,
                                kSessionId, {}, /*hostInitiated=*/false)
                  .ok());
  ASSERT_TRUE(mHostHub->ackSession(kSessionId, /*hostAcked=*/true).ok());

  EXPECT_TRUE(mHostHub->ackSession(kSessionId, /*hostAcked=*/false).ok());
  EXPECT_TRUE(mHostHub->checkSessionOpen(kSessionId).ok());
}

TEST_F(MessageHubManagerTest, ActiveSessionEmbeddedHubGone) {
  auto sessionId = setupDefaultHubsAndSession();

  EXPECT_CALL(*mHostHubCb,
              onCloseEndpointSession(sessionId, Reason::HUB_RESET));
  EXPECT_CALL(*mHostHubCb,
              onEndpointStopped(UnorderedElementsAreArray(
                                    {kEndpoint2_1Info.id, kEndpoint2_2Info.id}),
                                Reason::HUB_RESET));
  mManager->removeEmbeddedHub(kHub2Id);
  EXPECT_EQ(mHostHub->checkSessionOpen(sessionId), pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, ActiveSessionEmbeddedEndpointGone) {
  auto sessionId = setupDefaultHubsAndSession();

  EXPECT_CALL(*mHostHubCb,
              onCloseEndpointSession(sessionId, Reason::ENDPOINT_GONE));
  EXPECT_CALL(*mHostHubCb, onEndpointStopped(
                               UnorderedElementsAreArray({kEndpoint2_1Info.id}),
                               Reason::ENDPOINT_GONE));
  mManager->removeEmbeddedEndpoint(kEndpoint2_1Info.id);
  EXPECT_EQ(mHostHub->checkSessionOpen(sessionId), pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, ActiveSessionHostEndpointGone) {
  auto sessionId = setupDefaultHubsAndSession();

  EXPECT_THAT(*mHostHub->removeEndpoint(kEndpoint1_1Info.id),
              UnorderedElementsAreArray({sessionId}));
  EXPECT_EQ(mHostHub->checkSessionOpen(sessionId), pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, HandleMessage) {
  auto sessionId = setupDefaultHubsAndSession();

  Message message{.content = {0xde, 0xad, 0xbe, 0xef}};
  EXPECT_CALL(*mHostHubCb, onMessageReceived(sessionId, message));
  EXPECT_TRUE(mHostHub->handleMessage(sessionId, message).ok());
}

TEST_F(MessageHubManagerTest, HandleMessageForUnknownSession) {
  setupDefaultHubs();

  Message message{.content = {0xde, 0xad, 0xbe, 0xef}};
  EXPECT_CALL(*mHostHubCb, onMessageReceived(_, _)).Times(0);
  EXPECT_FALSE(mHostHub->handleMessage(1, message).ok());
}

TEST_F(MessageHubManagerTest, HandleMessageDeliveryStatus) {
  auto sessionId = setupDefaultHubsAndSession();

  MessageDeliveryStatus status{.errorCode = ErrorCode::TRANSIENT_ERROR};
  EXPECT_CALL(*mHostHubCb, onMessageDeliveryStatusReceived(sessionId, status));
  EXPECT_TRUE(mHostHub->handleMessageDeliveryStatus(sessionId, status).ok());
}

TEST_F(MessageHubManagerTest, HandleMessageDeliveryStatusForUnknownSession) {
  setupDefaultHubs();

  MessageDeliveryStatus status{.errorCode = ErrorCode::TRANSIENT_ERROR};
  EXPECT_CALL(*mHostHubCb, onMessageDeliveryStatusReceived(_, _)).Times(0);
  EXPECT_FALSE(mHostHub->handleMessageDeliveryStatus(1, status).ok());
}

TEST_F(MessageHubManagerTest, AddHostSourceDataFlow) {
  setupDefaultHubs();

  DataFlowInfo info;
  DataFlowId dataFlowId{.hubId = kHub1Id, .id = 1};
  EXPECT_CALL(*mDataFlowManager, addHostSourceDataFlow(kEndpoint1_1Info.id, _))
      .WillOnce(Return(dataFlowId));

  auto result = mHostHub->addDataFlow(kEndpoint1_1Info.id, info);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->id, dataFlowId.id);
  EXPECT_EQ(result->hubId, dataFlowId.hubId);
}

TEST_F(MessageHubManagerTest, AddHostSourceDataFlowInvalidEndpoint) {
  setupDefaultHubs();

  DataFlowInfo info;
  EXPECT_NE(mHostHub->addDataFlow(kEndpoint2_1Info.id, info).status(),
            pw::OkStatus());
}

TEST_F(MessageHubManagerTest, AddSinkToHostSourceDataFlow) {
  setupDefaultHubs();

  // Host Endpoint 1 (Source) -> Embedded Endpoint 1 (Sink)
  DataFlowSinkRegistrationParams params;
  params.context.id = {.hubId = kHub1Id, .id = 1};
  params.sourceId = kEndpoint1_1Info.id;
  params.sinkId = kEndpoint2_1Info.id;
  params.sessionId = IEndpointCommunication::SESSION_ID_INVALID;

  EXPECT_CALL(*mDataFlowManager, addOffloadSink(_))
      .WillOnce(Invoke([](const DataFlowSinkRegistrationParams &) {
        return std::make_pair(DataFlowInfo(),
                              std::optional<SharedDataRegion>());
      }));

  const int32_t kSinkMetadataOffsetBytes = 1024;
  auto callback = SharedRefBase::make<MockRegisterOffloadSinkCallback>();
  EXPECT_CALL(*callback, addSinkInRegion(_, _))
      .WillOnce(Invoke([](const std::optional<SharedDataRegion> &,
                          int64_t *metadataOffsetBytes) {
        *metadataOffsetBytes = kSinkMetadataOffsetBytes;
        return ndk::ScopedAStatus::ok();
      }));

  auto result = mHostHub->addSinkToDataFlow(params, callback);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->context.id.id, params.context.id.id);
  EXPECT_EQ(result->context.metadataOffsetBytes, kSinkMetadataOffsetBytes);
}

TEST_F(MessageHubManagerTest, AddSinkToHostSourceDataFlowWithRegion) {
  setupDefaultHubs();

  // Host Endpoint 1 (Source) -> Embedded Endpoint 1 (Sink)
  DataFlowSinkRegistrationParams params;
  params.context.id = {.hubId = kHub1Id, .id = 1};
  params.sourceId = kEndpoint1_1Info.id;
  params.sinkId = kEndpoint2_1Info.id;
  params.sessionId = IEndpointCommunication::SESSION_ID_INVALID;

  std::optional<SharedDataRegion> region = SharedDataRegion{.id = 2};
  EXPECT_CALL(*mDataFlowManager, addOffloadSink(_))
      .WillOnce(Invoke([](const DataFlowSinkRegistrationParams &) {
        return std::make_pair(DataFlowInfo(), std::optional<SharedDataRegion>(
                                                  SharedDataRegion{.id = 2}));
      }));

  const int32_t kSinkMetadataOffsetBytes = 1024;
  auto callback = SharedRefBase::make<MockRegisterOffloadSinkCallback>();
  EXPECT_CALL(*callback, addSinkInRegion(_, _))
      .WillOnce(Invoke([](const std::optional<SharedDataRegion> &region,
                          int64_t *metadataOffsetBytes) {
        EXPECT_EQ(region->id, 2);
        *metadataOffsetBytes = kSinkMetadataOffsetBytes;
        return ndk::ScopedAStatus::ok();
      }));

  auto result = mHostHub->addSinkToDataFlow(params, callback);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->context.id.id, params.context.id.id);
  EXPECT_EQ(result->context.metadataOffsetBytes, kSinkMetadataOffsetBytes);
  EXPECT_TRUE(result->context.sinkMetadataRegion.has_value());
  EXPECT_EQ(result->context.sinkMetadataRegion->id, 2);
}

TEST_F(MessageHubManagerTest, AddSinkToHostSourceDataFlowWithSession) {
  auto sessionId = setupDefaultHubsAndSession();

  // Host Endpoint 1 (Source) -> Embedded Endpoint 1 (Sink)
  DataFlowSinkRegistrationParams params;
  params.context.id = {.hubId = kHub1Id, .id = 1};
  params.sourceId = kEndpoint1_1Info.id;
  params.sinkId = kEndpoint2_1Info.id;
  params.sessionId = sessionId;

  EXPECT_CALL(*mDataFlowManager, addOffloadSink(_))
      .WillOnce(Invoke([](const DataFlowSinkRegistrationParams &) {
        return std::make_pair(DataFlowInfo(),
                              std::optional<SharedDataRegion>());
      }));

  const int32_t kSinkMetadataOffsetBytes = 1024;
  auto callback = SharedRefBase::make<MockRegisterOffloadSinkCallback>();
  EXPECT_CALL(*callback, addSinkInRegion(_, _))
      .WillOnce(Invoke([](const std::optional<SharedDataRegion> &,
                          int64_t *metadataOffsetBytes) {
        *metadataOffsetBytes = kSinkMetadataOffsetBytes;
        return ndk::ScopedAStatus::ok();
      }));

  auto result = mHostHub->addSinkToDataFlow(params, callback);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->context.id.id, params.context.id.id);
  EXPECT_EQ(result->context.metadataOffsetBytes, kSinkMetadataOffsetBytes);
}

TEST_F(MessageHubManagerTest, AddSinkToHostSourceDataFlowUnknownSession) {
  auto sessionId = setupDefaultHubsAndSession();

  // Host Endpoint 1 (Source) -> Embedded Endpoint 1 (Sink)
  DataFlowSinkRegistrationParams params;
  params.context.id = {.hubId = kHub1Id, .id = 1};
  params.sourceId = kEndpoint1_1Info.id;
  params.sinkId = kEndpoint2_1Info.id;
  params.sessionId = sessionId + 1;

  auto callback = SharedRefBase::make<MockRegisterOffloadSinkCallback>();
  EXPECT_EQ(mHostHub->addSinkToDataFlow(params, callback).status(),
            pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, AddSinkToHostSourceDataFlowInvalidEndpoints) {
  setupDefaultHubs();

  auto callback = SharedRefBase::make<MockRegisterOffloadSinkCallback>();

  // Source not on hub
  DataFlowSinkRegistrationParams params;
  params.sourceId = kEndpoint2_1Info.id;
  params.sinkId = kEndpoint2_1Info.id;
  EXPECT_EQ(mHostHub->addSinkToDataFlow(params, callback).status(),
            pw::Status::InvalidArgument());

  // Sink not on embedded hub
  params.sourceId = kEndpoint1_1Info.id;
  params.sinkId = kEndpoint1_1Info.id;
  EXPECT_EQ(mHostHub->addSinkToDataFlow(params, callback).status(),
            pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, RemoveHostSourceDataFlow) {
  setupDefaultHubs();

  DataFlowId dataFlowId{.hubId = kHub1Id, .id = 1};
  EXPECT_CALL(*mDataFlowManager, removeDataFlow(dataFlowId))
      .WillOnce(Return(std::make_pair(
          EndpointId(), std::vector<EndpointId>{kEndpoint1_1Info.id})));

  auto result = mHostHub->removeDataFlow(dataFlowId);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, UnorderedElementsAreArray({kEndpoint1_1Info.id}));
}

TEST_F(MessageHubManagerTest, AddHostSinkToEmbeddedSourceDataFlow) {
  setupDefaultHubs();

  // Embedded Endpoint 1 (Source) -> Host Endpoint 1 (Sink)
  DataFlowSinkRegistrationParams params;
  params.context.id = {.hubId = kHub2Id, .id = 1};
  params.context.info = DataFlowInfo{.region = {.id = 2}};
  params.context.sinkMetadataRegion = SharedDataRegion{.id = 3};
  params.sourceId = kEndpoint2_1Info.id;
  params.sinkId = kEndpoint1_1Info.id;
  params.sessionId = IEndpointCommunication::SESSION_ID_INVALID;

  EXPECT_CALL(*mDataFlowManager,
              addHostSink(params.context.id, params.sourceId, params.sinkId,
                          params.context.info->region.id,
                          params.context.sinkMetadataRegion->id, _, _))
      .WillOnce(Invoke([&params](DataFlowId, EndpointId, EndpointId, int32_t,
                                 int32_t, uint32_t, uint32_t) {
        // Return a copy of params.context, but we have to construct it because
        // it's not copyable.
        DataFlowSinkContext ctx;
        ctx.id = params.context.id;
        ctx.info = std::make_optional<DataFlowInfo>();
        ctx.info->region.id = params.context.info->region.id;
        ctx.sinkMetadataRegion = std::make_optional<SharedDataRegion>();
        ctx.sinkMetadataRegion->id = params.context.sinkMetadataRegion->id;
        return ctx;
      }));
  EXPECT_CALL(*mHostHubCb, onDataFlowHostSinkRegistered(_));

  EXPECT_TRUE(mHostHub->handleAddSink(params).ok());
}

TEST_F(MessageHubManagerTest, AddHostSinkToEmbeddedSourceDataFlowWithSession) {
  auto sessionId = setupDefaultHubsAndSession();

  // Embedded Endpoint 1 (Source) -> Host Endpoint 1 (Sink)
  DataFlowSinkRegistrationParams params;
  params.context.id = {.hubId = kHub2Id, .id = 1};
  params.context.info = DataFlowInfo{.region = {.id = 2}};
  params.context.sinkMetadataRegion = SharedDataRegion{.id = 3};
  params.sourceId = kEndpoint2_1Info.id;
  params.sinkId = kEndpoint1_1Info.id;
  params.sessionId = sessionId;

  EXPECT_CALL(*mDataFlowManager,
              addHostSink(params.context.id, params.sourceId, params.sinkId,
                          params.context.info->region.id,
                          params.context.sinkMetadataRegion->id, _, _))
      .WillOnce(Invoke([&params](DataFlowId, EndpointId, EndpointId, int32_t,
                                 int32_t, uint32_t, uint32_t) {
        // Return a copy of params.context, but we have to construct it because
        // it's not copyable.
        DataFlowSinkContext ctx;
        ctx.id = params.context.id;
        ctx.info = std::make_optional<DataFlowInfo>();
        ctx.info->region.id = params.context.info->region.id;
        ctx.sinkMetadataRegion = std::make_optional<SharedDataRegion>();
        ctx.sinkMetadataRegion->id = params.context.sinkMetadataRegion->id;
        return ctx;
      }));
  EXPECT_CALL(*mHostHubCb, onDataFlowHostSinkRegistered(_));

  EXPECT_TRUE(mHostHub->handleAddSink(params).ok());
}

TEST_F(MessageHubManagerTest,
       AddHostSinkToEmbeddedSourceDataFlowUnknownSession) {
  auto sessionId = setupDefaultHubsAndSession();

  // Embedded Endpoint 1 (Source) -> Host Endpoint 1 (Sink)
  DataFlowSinkRegistrationParams params;
  params.context.id = {.hubId = kHub2Id, .id = 1};
  params.sourceId = kEndpoint2_1Info.id;
  params.sinkId = kEndpoint1_1Info.id;
  params.sessionId = sessionId + 1;

  EXPECT_EQ(mHostHub->handleAddSink(params), pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest,
       AddHostSinkToEmbeddedSourceDataFlowInvalidEndpoints) {
  setupDefaultHubs();

  // Sink not on hub
  DataFlowSinkRegistrationParams params;
  params.sourceId = kEndpoint2_1Info.id;
  params.sinkId = kEndpoint2_1Info.id;
  params.sessionId = IEndpointCommunication::SESSION_ID_INVALID;
  EXPECT_EQ(mHostHub->handleAddSink(params), pw::Status::InvalidArgument());

  // Source not on embedded hub
  params.sourceId = kEndpoint1_1Info.id;
  params.sinkId = kEndpoint1_1Info.id;
  params.sessionId = IEndpointCommunication::SESSION_ID_INVALID;
  EXPECT_EQ(mHostHub->handleAddSink(params), pw::Status::NotFound());
}

TEST_F(MessageHubManagerTest, AddHostSinkToEmbeddedSourceDataFlowMissingData) {
  setupDefaultHubs();

  // Missing info
  DataFlowSinkRegistrationParams params;
  params.context.id = {.hubId = kHub2Id, .id = 1};
  params.context.sinkMetadataRegion = SharedDataRegion{.id = 3};
  params.sourceId = kEndpoint2_1Info.id;
  params.sinkId = kEndpoint1_1Info.id;
  params.sessionId = IEndpointCommunication::SESSION_ID_INVALID;
  EXPECT_EQ(mHostHub->handleAddSink(params), pw::Status::InvalidArgument());

  // Missing sink metadata region
  params.context.info = DataFlowInfo{.region = {.id = 2}};
  params.context.sinkMetadataRegion.reset();
  EXPECT_EQ(mHostHub->handleAddSink(params), pw::Status::InvalidArgument());
}

TEST_F(MessageHubManagerTest, RemoveHostSinkFromEmbeddedSourceDataFlow) {
  setupDefaultHubs();

  DataFlowId dataFlowId{.hubId = kHub2Id, .id = 1};
  EXPECT_CALL(*mDataFlowManager, removeSink(dataFlowId, kEndpoint1_1Info.id))
      .WillOnce(Return(kEndpoint2_1Info.id));

  auto result = mHostHub->removeSink(dataFlowId, kEndpoint1_1Info.id);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->id, kEndpoint2_1Info.id.id);
  EXPECT_EQ(result->hubId, kEndpoint2_1Info.id.hubId);
}

TEST_F(MessageHubManagerTest,
       RemoveHostSinkFromEmbeddedSourceDataFlowInvalidEndpoint) {
  setupDefaultHubs();

  DataFlowId dataFlowId{.hubId = kHub2Id, .id = 1};
  EXPECT_EQ(mHostHub->removeSink(dataFlowId, kEndpoint2_1Info.id).status(),
            pw::Status::InvalidArgument());
}

TEST_F(MessageHubManagerTest, RemoveEmbeddedSourceDataFlow) {
  setupDefaultHubs();

  DataFlowId dataFlowId{.hubId = kHub2Id, .id = 1};
  // Host endpoint 1 is a sink on this data flow
  EXPECT_CALL(*mDataFlowManager, removeDataFlow(dataFlowId))
      .WillOnce(Return(std::pair<EndpointId, std::vector<EndpointId>>{
          kEndpoint2_1Info.id, {kEndpoint1_1Info.id}}));

  EXPECT_CALL(*mHostHubCb,
              onDataFlowOffloadEndpointUnregistered(
                  dataFlowId, kEndpoint2_1Info.id,
                  UnorderedElementsAreArray({kEndpoint1_1Info.id})));

  mManager->removeEmbeddedSourceDataFlow(dataFlowId);
}

TEST_F(MessageHubManagerTest, RemoveEmbeddedSinkFromHostSourceDataFlow) {
  setupDefaultHubs();

  DataFlowId dataFlowId{.hubId = kHub1Id, .id = 1};
  EXPECT_CALL(*mDataFlowManager, removeSink(dataFlowId, kEndpoint2_1Info.id))
      .WillOnce(Return(kEndpoint1_1Info.id));

  EXPECT_CALL(*mHostHubCb,
              onDataFlowOffloadEndpointUnregistered(
                  dataFlowId, kEndpoint2_1Info.id,
                  UnorderedElementsAreArray({kEndpoint1_1Info.id})));

  mManager->removeDataFlowEmbeddedSink(dataFlowId, kEndpoint2_1Info.id);
}

}  // namespace
}  // namespace android::hardware::contexthub::common::implementation
