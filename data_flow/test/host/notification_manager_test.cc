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
#include <optional>
#include <vector>

#include <aidl/android/hardware/contexthub/IContextHub.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace android::contexthub::data_flow {
namespace {

using ::aidl::android::hardware::contexthub::DataFlowConsumerHandle;
using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::DataFlowInfo;
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

  pw::Result<DataFlowConsumerHandle> createHostConsumerHandle(
      DataFlowId dataFlowId) {
    DataFlowInfo info;
    info.producerEventFd.set(eventfd(0, EFD_NONBLOCK));
    info.producerEventFdNonwake.set(eventfd(0, EFD_NONBLOCK));
    if (info.producerEventFd.get() < 0 ||
        info.producerEventFdNonwake.get() < 0) {
      return pw::Status::Internal();
    }

    auto consumer = DataFlowConsumerHandle{
        .id = dataFlowId, .info = std::make_optional(std::move(info))};
    consumer.consumerEventFd.set(eventfd(0, EFD_NONBLOCK));
    consumer.consumerEventFdNonwake.set(eventfd(0, EFD_NONBLOCK));
    consumer.halAckEventFd.set(eventfd(0, EFD_NONBLOCK));
    if (consumer.consumerEventFd.get() < 0 ||
        consumer.consumerEventFdNonwake.get() < 0 ||
        consumer.halAckEventFd.get() < 0) {
      return pw::Status::Internal();
    }
    return consumer;
  }

  pw::Result<DataFlowInfo> setupHostProducerDataFlow(
      int dataFlowId, std::vector<int> *waitFds = nullptr) {
    PW_TRY_ASSIGN(auto result, mManager->prepareHostProducerDataFlow());
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
  auto result = mManager->prepareHostProducerDataFlow();
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result->first.producerEventFd.get() >= 0);
  EXPECT_TRUE(result->first.producerEventFdNonwake.get() >= 0);
  EXPECT_TRUE(result->first.halAckEventFd.get() >= 0);
  EXPECT_NE(result->second, nullptr);

  EXPECT_EQ(mManager->discardNotificationDataHandle(result->second),
            pw::OkStatus());
}

TEST_F(NotificationManagerTest, ActivateAndRemoveHostProducerDataFlow) {
  auto result = mManager->prepareHostProducerDataFlow();
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
  ASSERT_EQ(setupHostProducerDataFlow(dataFlowId).status(), pw::OkStatus());

  EndpointId consumerId = {.hubId = 1, .id = 1};
  auto consumerResult = mManager->addOffloadConsumer(dataFlowId, consumerId);
  ASSERT_TRUE(consumerResult.ok());
  EXPECT_GE(consumerResult->consumerEventFd.get(), 0);
  EXPECT_GE(consumerResult->consumerEventFdNonwake.get(), 0);

  EXPECT_EQ(mManager->removeOffloadConsumer(consumerId), pw::OkStatus());
  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, AddAndRemoveOffloadProducerDataFlow) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  auto hostConsumer = createHostConsumerHandle(dataFlowId);
  ASSERT_TRUE(hostConsumer.ok());

  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  EXPECT_EQ(mManager->enableHostConsumer(*hostConsumer), pw::OkStatus());

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->disableHostConsumer(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, NotifyOffloadProducerWaking) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  auto hostConsumer = createHostConsumerHandle(dataFlowId);
  ASSERT_TRUE(hostConsumer.ok());
  auto wakingFd = hostConsumer->info->producerEventFd.dup();
  ASSERT_GE(wakingFd.get(), 0);

  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  EXPECT_EQ(mManager->enableHostConsumer(*hostConsumer), pw::OkStatus());

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
  auto nonWakingFd = hostConsumer->info->producerEventFdNonwake.dup();
  ASSERT_GE(nonWakingFd.get(), 0);

  EXPECT_CALL(*mWaiter, addFd(_)).Times(2);
  EXPECT_EQ(mManager->enableHostConsumer(*hostConsumer), pw::OkStatus());

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
  ASSERT_EQ(setupHostProducerDataFlow(dataFlowId).status(), pw::OkStatus());

  EndpointId consumerId = {.hubId = 1, .id = 1};
  auto consumerResult = mManager->addOffloadConsumer(dataFlowId, consumerId);
  ASSERT_TRUE(consumerResult.ok());
  auto &wakingFd = consumerResult->consumerEventFd;
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
  ASSERT_EQ(setupHostProducerDataFlow(dataFlowId).status(), pw::OkStatus());

  EndpointId consumerId = {.hubId = 1, .id = 1};
  auto consumerResult = mManager->addOffloadConsumer(dataFlowId, consumerId);
  ASSERT_TRUE(consumerResult.ok());
  auto &nonWakingFd = consumerResult->consumerEventFdNonwake;
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
  auto result = setupHostProducerDataFlow(dataFlowId, &waitFds);
  ASSERT_EQ(result.status(), pw::OkStatus());

  // Send a notification on the waking and non-waking fds.
  uint64_t count = 1;
  ASSERT_EQ(write(result->producerEventFd.get(), &count, sizeof(count)),
            sizeof(count));
  ASSERT_EQ(write(result->producerEventFdNonwake.get(), &count, sizeof(count)),
            sizeof(count));

  // Trigger notification handling on both fds.
  for (int fd : waitFds) {
    mWaiter->triggerNotification(fd, false);
  }
  EXPECT_EQ(mNotificationCount, 2);

  // The HAL ack count should only be 1.
  uint64_t ackCount;
  ASSERT_EQ(read(result->halAckEventFd.get(), &ackCount, sizeof(ackCount)),
            sizeof(ackCount));
  EXPECT_EQ(ackCount, 1);

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, HandleHostConsumerNotifications) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  auto hostConsumer = createHostConsumerHandle(dataFlowId);
  ASSERT_TRUE(hostConsumer.ok());
  auto wakingFd = hostConsumer->consumerEventFd.dup();
  auto nonWakingFd = hostConsumer->consumerEventFdNonwake.dup();
  auto halAckFd = hostConsumer->halAckEventFd.dup();
  ASSERT_GE(wakingFd.get(), 0);
  ASSERT_GE(nonWakingFd.get(), 0);
  ASSERT_GE(halAckFd.get(), 0);

  // Enable the host consumer and capture the consumer wait fds.
  std::vector<int> waitFds;
  EXPECT_CALL(*mWaiter, addFd(_)).Times(2).WillRepeatedly([&waitFds](int fd) {
    waitFds.push_back(fd);
  });
  EXPECT_EQ(mManager->enableHostConsumer(*hostConsumer), pw::OkStatus());

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
  auto result = setupHostProducerDataFlow(dataFlowId, &waitFds);
  ASSERT_EQ(result.status(), pw::OkStatus());

  int wakingFd = result->producerEventFd.get();
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
  ASSERT_EQ(read(result->halAckEventFd.get(), &ackCount, sizeof(ackCount)),
            sizeof(ackCount));
  EXPECT_EQ(ackCount, 3);

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->removeHostProducerDataFlow(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, NotificationCallbackDisabledAfterError) {
  int dataFlowId = 1;
  std::vector<int> waitFds;
  auto result = setupHostProducerDataFlow(dataFlowId, &waitFds);
  ASSERT_EQ(result.status(), pw::OkStatus());

  // Trigger an error notification. The callback should not be invoked and the
  // epoll trigger should be removed.
  EXPECT_CALL(*mWaiter, removeFd(waitFds[0]));
  mWaiter->triggerNotification(waitFds[0], true);
  EXPECT_EQ(mNotificationCount, 0);

  // Trigger a subsequent valid notification which should be ignored.
  uint64_t count = 1;
  ASSERT_EQ(write(result->producerEventFd.get(), &count, sizeof(count)),
            sizeof(count));
  ASSERT_EQ(write(result->producerEventFdNonwake.get(), &count, sizeof(count)),
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
  auto result = mManager->prepareHostProducerDataFlow();
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
  auto result = mManager->prepareHostProducerDataFlow();
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
  EXPECT_EQ(mManager->addOffloadConsumer(1, consumerId).status(),
            pw::Status::NotFound());
}

TEST_F(NotificationManagerTest, AddDuplicateConsumer) {
  int dataFlowId = 1;
  ASSERT_EQ(setupHostProducerDataFlow(dataFlowId).status(), pw::OkStatus());
  EndpointId consumerId = {.hubId = 1, .id = 1};
  ASSERT_EQ(mManager->addOffloadConsumer(dataFlowId, consumerId).status(),
            pw::OkStatus());

  EXPECT_EQ(mManager->addOffloadConsumer(dataFlowId, consumerId).status(),
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
  ASSERT_EQ(mManager->enableHostConsumer(*hostConsumer), pw::OkStatus());

  DataFlowConsumerHandle duplicateConsumer = {.id = dataFlowId};
  EXPECT_EQ(mManager->enableHostConsumer(duplicateConsumer),
            pw::Status::AlreadyExists());

  EXPECT_CALL(*mWaiter, removeFd(_)).Times(2);
  EXPECT_EQ(mManager->disableHostConsumer(dataFlowId), pw::OkStatus());
}

TEST_F(NotificationManagerTest, EnableHostConsumerWithInvalidFds) {
  DataFlowId dataFlowId = {.hubId = 1, .id = 1};
  DataFlowConsumerHandle consumer = {.id = dataFlowId};
  EXPECT_EQ(mManager->enableHostConsumer(consumer),
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
