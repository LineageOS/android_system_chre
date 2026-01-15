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

#include "data_flow/host/notification_manager.h"

#include <sys/eventfd.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include <aidl/android/hardware/contexthub/IContextHub.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "android/binder_auto_utils.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowAlertFds;
using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::DataFlowInfo;
using ::aidl::android::hardware::contexthub::DataFlowSinkContext;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::testing::_;

class MockEpollWaiter : public NotificationManager::EpollWaiter {
 public:
  MOCK_METHOD(void, addFd, (int fd), (override));
  MOCK_METHOD(void, removeFd, (int fd), (override));

  void triggerNotification(int fd, bool error) {
    handleNotification(fd, error);
  }
};

class NotificationManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto mockWaiter = std::make_unique<MockEpollWaiter>();
    mWaiter = mockWaiter.get();
    mManager = NotificationManager::create(
        std::move(mockWaiter),
        [this](DataFlowId, bool) { mNotificationCount++; });
  }

  pw::Result<DataFlowSinkContext> createHostConsumerHandle(
      DataFlowId dataFlowId) {
    PW_TRY_ASSIGN(auto eventFdsPair, createHostConsumerEventFds());
    return DataFlowSinkContext{
        .id = dataFlowId,
        .info = DataFlowInfo{.alertFds = std::move(eventFdsPair.second)},
        .alertFds = std::move(eventFdsPair.first)};
  };

  pw::Result<std::pair<DataFlowAlertFds, DataFlowAlertFds>>
  createHostConsumerEventFds() {
    DataFlowAlertFds notifyHostFds{
        .waking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK)),
        .nonWaking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK)),
        .halAck = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK))};
    DataFlowAlertFds notifyOffloadFds{
        .waking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK)),
        .nonWaking = ndk::ScopedFileDescriptor(eventfd(0, EFD_NONBLOCK))};
    if (notifyHostFds.waking.get() < 0 || notifyHostFds.nonWaking.get() < 0 ||
        notifyHostFds.halAck.get() < 0 || notifyOffloadFds.waking.get() < 0 ||
        notifyOffloadFds.nonWaking.get() < 0) {
      return pw::Status::Internal();
    }
    return std::make_pair(std::move(notifyHostFds),
                          std::move(notifyOffloadFds));
  }

  pw::Result<DataFlowInfo> setupHostProducerDataFlowInfo(
      int dataFlowId, std::vector<int> *waitFds = nullptr) {
    PW_TRY_ASSIGN(auto result, mManager->prepareHostProducerDataFlowInfo());
    if (waitFds) {
      EXPECT_CALL(*mWaiter, addFd(_))
          .Times(2)
          .WillRepeatedly([waitFds](int fd) { waitFds->push_back(fd); });
    } else {
      EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
    }
    PW_TRY(mManager->activateHostProducerDataFlow(dataFlowId, result.second));
    return std::move(result.first);
  }

  pw::Result<DataFlowAlertFds> setupHostProducerDataFlowEventFds(
      int dataFlowId, std::vector<int> *waitFds = nullptr) {
    PW_TRY_ASSIGN(auto result, mManager->prepareHostProducerDataFlowEventFds());
    if (waitFds) {
      EXPECT_CALL(*mWaiter, addFd(_))
          .Times(2)
          .WillRepeatedly([waitFds](int fd) { waitFds->push_back(fd); });
    } else {
      EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
    }
    PW_TRY(mManager->activateHostProducerDataFlow(dataFlowId, result.second));
    return std::move(result.first);
  }

  MockEpollWaiter *mWaiter;
  std::shared_ptr<NotificationManager> mManager;
  int mNotificationCount = 0;
};

TEST_F(NotificationManagerTest, PrepareAndDiscardHostProducerDataFlow) {
  auto result = mManager->prepareHostProducerDataFlowInfo();
  ASSERT_TRUE(result.ok());
  EXPECT_GE(result->first.alertFds.waking.get(), 0);
  EXPECT_GE(result->first.alertFds.nonWaking.get(), 0);
  EXPECT_GE(result->first.alertFds.halAck.get(), 0);
  EXPECT_NE(result->second, nullptr);

  EXPECT_EQ(mManager->discardNotificationDataHandle(result->second),
            pw::OkStatus());
}

TEST_F(NotificationManagerTest, ActivateAndRemoveHostProducerDataFlow) {
  auto result = mManager->prepareHostProducerDataFlowInfo();
  ASSERT_TRUE(result.ok());

  int dataFlowId = 1;
  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  EXPECT_EQ(mManager->activateHostProducerDataFlow(dataFlowId, result->second),
            pw::OkStatus());

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, AddAndRemoveOffloadConsumer) {
  int dataFlowId = 1;
  ASSERT_EQ(setupHostProducerDataFlowInfo(dataFlowId).status(), pw::OkStatus());

  EndpointId consumerId = {.hubId = 1, .id = 1};
  auto consumerResult =
      mManager->addOffloadConsumerAndCreateHandle(dataFlowId, consumerId);
  ASSERT_TRUE(consumerResult.ok());
  EXPECT_GE(consumerResult->alertFds.waking.get(), 0);
  EXPECT_GE(consumerResult->alertFds.nonWaking.get(), 0);

  EXPECT_EQ(mManager->removeOffloadConsumer(consumerId), pw::OkStatus());
  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, AddAndRemoveOffloadConsumerWithEventFds) {
  int dataFlowId = 1;
  ASSERT_EQ(setupHostProducerDataFlowInfo(dataFlowId).status(), pw::OkStatus());

  EndpointId consumerId = {.hubId = 1, .id = 1};
  auto eventFds =
      mManager->addOffloadConsumerAndGetEventFds(dataFlowId, consumerId);
  ASSERT_TRUE(eventFds.ok());
  EXPECT_GE(eventFds->waking.get(), 0);
  EXPECT_GE(eventFds->nonWaking.get(), 0);

  EXPECT_EQ(mManager->removeOffloadConsumer(consumerId), pw::OkStatus());
  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, AddAndRemoveOffloadProducerDataFlowWithHandle) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  auto hostConsumer = createHostConsumerHandle(dataFlowId);
  ASSERT_TRUE(hostConsumer.ok());

  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  EXPECT_EQ(mManager->enableHostConsumerFromHandle(*hostConsumer),
            pw::OkStatus());

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->disableHostConsumer(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest,
       AddAndRemoveOffloadProducerDataFlowWithEventFds) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  auto hostConsumer = createHostConsumerEventFds();
  ASSERT_TRUE(hostConsumer.ok());

  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  EXPECT_EQ(mManager->enableHostConsumerFromEventFds(
                dataFlowId, std::move(hostConsumer->first),
                std::move(hostConsumer->second)),
            pw::OkStatus());

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->disableHostConsumer(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, NotifyOffloadProducerWaking) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  auto hostConsumer = createHostConsumerHandle(dataFlowId);
  ASSERT_TRUE(hostConsumer.ok());
  auto wakingFd = hostConsumer->info->alertFds.waking.dup();
  ASSERT_GE(wakingFd.get(), 0);

  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  EXPECT_EQ(mManager->enableHostConsumerFromHandle(*hostConsumer),
            pw::OkStatus());

  EXPECT_EQ(mManager->notifyOffloadProducer(dataFlowId, /*waking=*/true),
            pw::OkStatus());
  uint64_t count;
  EXPECT_EQ(read(wakingFd.get(), &count, sizeof(count)), sizeof(count));
  EXPECT_EQ(count, 1);

  EXPECT_EQ(mManager->notifyOffloadProducer(dataFlowId, /*waking=*/true),
            pw::OkStatus());
  EXPECT_EQ(read(wakingFd.get(), &count, sizeof(count)), sizeof(count));
  EXPECT_EQ(count, 1);

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->disableHostConsumer(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, NotifyOffloadProducerNonWaking) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  auto hostConsumer = createHostConsumerHandle(dataFlowId);
  ASSERT_TRUE(hostConsumer.ok());
  auto nonWakingFd = hostConsumer->info->alertFds.nonWaking.dup();
  ASSERT_GE(nonWakingFd.get(), 0);

  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  EXPECT_EQ(mManager->enableHostConsumerFromHandle(*hostConsumer),
            pw::OkStatus());

  EXPECT_EQ(mManager->notifyOffloadProducer(dataFlowId, /*waking=*/false),
            pw::OkStatus());
  uint64_t count;
  EXPECT_EQ(read(nonWakingFd.get(), &count, sizeof(count)), sizeof(count));
  EXPECT_EQ(count, 1);

  EXPECT_EQ(mManager->notifyOffloadProducer(dataFlowId, /*waking=*/false),
            pw::OkStatus());
  EXPECT_EQ(read(nonWakingFd.get(), &count, sizeof(count)), sizeof(count));
  EXPECT_EQ(count, 1);

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->disableHostConsumer(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, NotifyOffloadConsumerWaking) {
  int dataFlowId = 1;
  ASSERT_EQ(setupHostProducerDataFlowInfo(dataFlowId).status(), pw::OkStatus());

  EndpointId consumerId = {.hubId = 1, .id = 1};
  auto consumerResult =
      mManager->addOffloadConsumerAndCreateHandle(dataFlowId, consumerId);
  ASSERT_TRUE(consumerResult.ok());
  auto &wakingFd = consumerResult->alertFds.waking;
  ASSERT_GE(wakingFd.get(), 0);

  EXPECT_EQ(mManager->notifyOffloadConsumer(consumerId, /*waking=*/true),
            pw::OkStatus());
  uint64_t count;
  EXPECT_EQ(read(wakingFd.get(), &count, sizeof(count)), sizeof(count));
  EXPECT_EQ(count, 1);

  EXPECT_EQ(mManager->notifyOffloadConsumer(consumerId, /*waking=*/true),
            pw::OkStatus());
  EXPECT_EQ(read(wakingFd.get(), &count, sizeof(count)), sizeof(count));
  EXPECT_EQ(count, 1);

  EXPECT_EQ(mManager->removeOffloadConsumer(consumerId), pw::OkStatus());
  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, NotifyOffloadConsumerNonWaking) {
  int dataFlowId = 1;
  ASSERT_EQ(setupHostProducerDataFlowInfo(dataFlowId).status(), pw::OkStatus());

  EndpointId consumerId = {.hubId = 1, .id = 1};
  auto consumerResult =
      mManager->addOffloadConsumerAndCreateHandle(dataFlowId, consumerId);
  ASSERT_TRUE(consumerResult.ok());
  auto &nonWakingFd = consumerResult->alertFds.nonWaking;
  ASSERT_GE(nonWakingFd.get(), 0);

  EXPECT_EQ(mManager->notifyOffloadConsumer(consumerId, /*waking=*/false),
            pw::OkStatus());
  uint64_t count;
  EXPECT_EQ(read(nonWakingFd.get(), &count, sizeof(count)), sizeof(count));
  EXPECT_EQ(count, 1);

  EXPECT_EQ(mManager->notifyOffloadConsumer(consumerId, /*waking=*/false),
            pw::OkStatus());
  EXPECT_EQ(read(nonWakingFd.get(), &count, sizeof(count)), sizeof(count));
  EXPECT_EQ(count, 1);

  EXPECT_EQ(mManager->removeOffloadConsumer(consumerId), pw::OkStatus());
  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, HandleHostProducerNotifications) {
  // Set up the host producer data flow and capture the wait fds.
  int dataFlowId = 1;
  std::vector<int> waitFds;
  auto result = setupHostProducerDataFlowEventFds(dataFlowId, &waitFds);
  ASSERT_EQ(result.status(), pw::OkStatus());

  // Send a notification on the waking and non-waking fds.
  uint64_t count = 1;
  ASSERT_EQ(write(result->waking.get(), &count, sizeof(count)), sizeof(count));
  ASSERT_EQ(write(result->nonWaking.get(), &count, sizeof(count)),
            sizeof(count));

  // Trigger notification handling on both fds.
  for (int fd : waitFds) {
    mWaiter->triggerNotification(fd, false);
  }
  EXPECT_EQ(mNotificationCount, 2);

  // The HAL ack count should only be 1.
  uint64_t ackCount;
  ASSERT_EQ(read(result->halAck.get(), &ackCount, sizeof(ackCount)),
            sizeof(ackCount));
  EXPECT_EQ(ackCount, 1);

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, HandleHostConsumerNotifications) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  auto hostConsumer = createHostConsumerHandle(dataFlowId);
  ASSERT_TRUE(hostConsumer.ok());
  auto wakingFd = hostConsumer->alertFds.waking.dup();
  auto nonWakingFd = hostConsumer->alertFds.nonWaking.dup();
  auto halAckFd = hostConsumer->alertFds.halAck.dup();
  ASSERT_GE(wakingFd.get(), 0);
  ASSERT_GE(nonWakingFd.get(), 0);
  ASSERT_GE(halAckFd.get(), 0);

  // Enable the host consumer and capture the consumer wait fds.
  std::vector<int> waitFds;
  EXPECT_CALL(*mWaiter, addFd(_)).Times(2).WillRepeatedly([&waitFds](int fd) {
    waitFds.push_back(fd);
  });
  EXPECT_EQ(mManager->enableHostConsumerFromHandle(*hostConsumer),
            pw::OkStatus());

  // Send a notification on the waking and non-waking fds.
  uint64_t count = 1;
  ASSERT_EQ(write(wakingFd.get(), &count, sizeof(count)), sizeof(count));
  ASSERT_EQ(write(nonWakingFd.get(), &count, sizeof(count)), sizeof(count));

  // Trigger notification handling on both fds.
  for (int fd : waitFds) {
    mWaiter->triggerNotification(fd, false);
  }
  EXPECT_EQ(mNotificationCount, 2);

  // The HAL ack count should only be 1 for the waking notification.
  uint64_t ackCount;
  ASSERT_EQ(read(halAckFd.get(), &ackCount, sizeof(ackCount)),
            sizeof(ackCount));
  EXPECT_EQ(ackCount, 1);

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->disableHostConsumer(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, WakingNotificationCoalescedForHalAck) {
  int dataFlowId = 1;
  std::vector<int> waitFds;
  auto result = setupHostProducerDataFlowEventFds(dataFlowId, &waitFds);
  ASSERT_EQ(result.status(), pw::OkStatus());

  int wakingFd = result->waking.get();
  uint64_t count = 1;
  // Send 3 notifications.
  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(write(wakingFd, &count, sizeof(count)), sizeof(count));
  }

  // Trigger the epoll event on both fds. Only one should affect the ack count.
  for (int fd : waitFds) {
    mWaiter->triggerNotification(fd, false);
  }

  // Check that the ack sent to the HAL contains the coalesced count.
  uint64_t ackCount;
  ASSERT_EQ(read(result->halAck.get(), &ackCount, sizeof(ackCount)),
            sizeof(ackCount));
  EXPECT_EQ(ackCount, 3);

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, NotificationCallbackDisabledAfterError) {
  int dataFlowId = 1;
  std::vector<int> waitFds;
  auto result = setupHostProducerDataFlowEventFds(dataFlowId, &waitFds);
  ASSERT_EQ(result.status(), pw::OkStatus());

  // Trigger an error notification. The callback should not be invoked and the
  // epoll trigger should be removed.
  EXPECT_CALL(*mWaiter, removeFd(waitFds[0]));
  mWaiter->triggerNotification(waitFds[0], true);
  EXPECT_EQ(mNotificationCount, 0);

  // Trigger a subsequent valid notification which should be ignored.
  uint64_t count = 1;
  ASSERT_EQ(write(result->waking.get(), &count, sizeof(count)), sizeof(count));
  ASSERT_EQ(write(result->nonWaking.get(), &count, sizeof(count)),
            sizeof(count));
  mWaiter->triggerNotification(waitFds[0], false);
  EXPECT_EQ(mNotificationCount, 0);

  // Only the non-waking fd should be removed since the waking one was already
  // removed on error.
  EXPECT_CALL(*mWaiter, removeFd(waitFds[1]));
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, DiscardUnknownHandle) {
  EXPECT_EQ(mManager->discardNotificationDataHandle(nullptr),
            pw::Status::NotFound());
}

TEST_F(NotificationManagerTest, DiscardActiveHandle) {
  auto result = mManager->prepareHostProducerDataFlowInfo();
  ASSERT_TRUE(result.ok());
  int dataFlowId = 1;
  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  ASSERT_EQ(mManager->activateHostProducerDataFlow(dataFlowId, result->second),
            pw::OkStatus());

  EXPECT_EQ(mManager->discardNotificationDataHandle(result->second),
            pw::Status::FailedPrecondition());

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, ActivateUnknownHandle) {
  EXPECT_EQ(mManager->activateHostProducerDataFlow(1, nullptr),
            pw::Status::NotFound());
}

TEST_F(NotificationManagerTest, ActivateDuplicateDataFlow) {
  auto result = mManager->prepareHostProducerDataFlowInfo();
  ASSERT_TRUE(result.ok());
  int dataFlowId = 1;
  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  ASSERT_EQ(mManager->activateHostProducerDataFlow(dataFlowId, result->second),
            pw::OkStatus());

  EXPECT_EQ(mManager->activateHostProducerDataFlow(dataFlowId, result->second),
            pw::Status::AlreadyExists());

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, RemoveUnknownHostProducerDataFlow) {
  EXPECT_EQ(mManager->removeHostProducerDataFlow(1), pw::Status::NotFound());
}

TEST_F(NotificationManagerTest, AddConsumerToUnknownDataFlow) {
  EndpointId consumerId = {.hubId = 1, .id = 1};
  EXPECT_EQ(mManager->addOffloadConsumerAndCreateHandle(1, consumerId).status(),
            pw::Status::NotFound());
}

TEST_F(NotificationManagerTest, AddDuplicateConsumer) {
  int dataFlowId = 1;
  ASSERT_EQ(setupHostProducerDataFlowInfo(dataFlowId).status(), pw::OkStatus());
  EndpointId consumerId = {.hubId = 1, .id = 1};
  ASSERT_EQ(mManager->addOffloadConsumerAndCreateHandle(dataFlowId, consumerId)
                .status(),
            pw::OkStatus());

  EXPECT_EQ(mManager->addOffloadConsumerAndCreateHandle(dataFlowId, consumerId)
                .status(),
            pw::Status::AlreadyExists());

  EXPECT_EQ(mManager->removeOffloadConsumer(consumerId), pw::OkStatus());
  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, RemoveUnknownOffloadConsumer) {
  EndpointId consumerId = {.hubId = 1, .id = 1};
  EXPECT_EQ(mManager->removeOffloadConsumer(consumerId),
            pw::Status::NotFound());
}

TEST_F(NotificationManagerTest, EnableDuplicateHostConsumer) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  auto hostConsumer = createHostConsumerHandle(dataFlowId);
  ASSERT_TRUE(hostConsumer.ok());
  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  ASSERT_EQ(mManager->enableHostConsumerFromHandle(*hostConsumer),
            pw::OkStatus());

  auto duplicateConsumer = createHostConsumerHandle(dataFlowId);
  ASSERT_TRUE(duplicateConsumer.ok());
  EXPECT_EQ(mManager->enableHostConsumerFromHandle(*duplicateConsumer),
            pw::Status::AlreadyExists());

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->disableHostConsumer(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, EnableHostConsumerWithInvalidFds) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  DataFlowSinkContext consumer = {.id = dataFlowId};
  EXPECT_EQ(mManager->enableHostConsumerFromHandle(consumer),
            pw::Status::InvalidArgument());
}

TEST_F(NotificationManagerTest, DisableUnknownHostConsumer) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  EXPECT_EQ(mManager->disableHostConsumer(dataFlowId), pw::Status::NotFound());
}

TEST_F(NotificationManagerTest, NotifyUnknownOffloadProducer) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  EXPECT_EQ(mManager->notifyOffloadProducer(dataFlowId, true),
            pw::Status::NotFound());
}

TEST_F(NotificationManagerTest, NotifyUnknownOffloadConsumer) {
  EndpointId consumerId = {.hubId = 1, .id = 1};
  EXPECT_EQ(mManager->notifyOffloadConsumer(consumerId, true),
            pw::Status::NotFound());
}

}  // namespace
}  // namespace android::contexthub::data_flow
