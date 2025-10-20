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

#include "exynos_chre_connection.h"
#include "chre_host/generated/host_messages_generated.h"
#include "chre_host/host_protocol_host.h"

#include <hardware_legacy/power.h>
#include <sys/ioctl.h>
#include <utils/SystemClock.h>
#include <cerrno>
#include <thread>

#define EXYNOS_IOC_MAGIC 'e'
#define EXYNOS_CHRE_IOC_GET_STAT _IOR(EXYNOS_IOC_MAGIC, 1, unsigned int)

namespace aidl::android::hardware::contexthub {

using namespace ::android::chre;
namespace fbs = ::chre::fbs;

namespace {
constexpr std::chrono::milliseconds kMessageHandlingTimeThreshold{1000};

enum ChreState {
  EXYNOS_CHRE_INIT = 0,
  EXYNOS_CHRE_OFF = 4,
  EXYNOS_CHRE_ON = 5,
};

ChreState chreCurrentState = EXYNOS_CHRE_INIT;

}  // namespace

bool ExynosChreConnection::init() {
  // Make sure the payload size is large enough for nanoapp binary fragment
  static_assert(kMaxSendingPayloadBytes > CHRE_HOST_DEFAULT_FRAGMENT_SIZE &&
                kMaxSendingPayloadBytes - CHRE_HOST_DEFAULT_FRAGMENT_SIZE >
                    kMaxPayloadOverheadBytes);
  mChreFileDescriptor =
      TEMP_FAILURE_RETRY(open(kChreFileDescriptorPath, O_RDWR));
  if (mChreFileDescriptor < 0) {
    LOGE("open chre device failed err=%d errno=%d\n", mChreFileDescriptor,
         errno);
    return false;
  }
  // launch the tasks
  mMessageListener = std::thread(messageListenerTask, this);
  mMessageHandler = std::thread(messageHandlerTask, this);
  mMessageSender = std::thread(messageSenderTask, this);
  mStateListener = std::thread(chreStateMonitorTask, this);

  mLpmaHandler.init();
  return true;
}

[[noreturn]] void ExynosChreConnection::messageListenerTask(
    ExynosChreConnection *chreConnection) {
  auto chreFd = chreConnection->getChreFileDescriptor();
  while (true) {
    {
      ssize_t payloadSize = TEMP_FAILURE_RETRY(read(
          chreFd, chreConnection->mPayload.get(), kMaxReceivingPayloadBytes));
      if (payloadSize == 0) {
        // Payload size 0 is a fake signal from kernel which is normal if the
        // device is in sleep.
        LOGV("%s: Received a payload size 0. Ignored. errno=%d", __func__,
             errno);
        continue;
      }
      if (payloadSize < 0) {
        LOGE("%s: read failed. payload size: %zu. errno=%d", __func__,
             payloadSize, errno);
        continue;
      }
      chreConnection->mReceivingQueue.emplace(chreConnection->mPayload.get(),
                                              payloadSize);
    }
  }
}

void ExynosChreConnection::messageHandlerTask(
    ExynosChreConnection *chreConnection) {
  while (true) {
    chreConnection->mReceivingQueue.waitForMessage();
    MessageFromChre &message = chreConnection->mReceivingQueue.front();
    handleMessageFromChre(chreConnection, message.buffer.get(), message.size);
    chreConnection->mReceivingQueue.pop();
  }
}

bool ExynosChreConnection::isChreRestarted() {
  uint32_t nextState = 0;
  bool ret = false;

  if (TEMP_FAILURE_RETRY(ioctl(mChreFileDescriptor, EXYNOS_CHRE_IOC_GET_STAT,
                               &nextState)) < 0) {
    LOGE("Unable to get an update for the CHRE state: errno=%d", errno);
    return ret;
  }

  auto chreNextState = static_cast<ChreState>(nextState);

  if (chreCurrentState != chreNextState) {
    LOGI("CHRE state changes from %" PRIu32 " to %" PRIu32, chreCurrentState,
         chreNextState);
  }
  if (chreCurrentState == EXYNOS_CHRE_OFF && chreNextState == EXYNOS_CHRE_ON) {
    ret = true;
  }
  chreCurrentState = chreNextState;

  return ret;
}

[[noreturn]] void ExynosChreConnection::chreStateMonitorTask(
    ExynosChreConnection *chreConnection) {
  while (true) {
    if (chreConnection->isChreRestarted()) {
      int64_t startTime = ::android::elapsedRealtime();
      chreConnection->waitChreBackOnline(
          /* timeoutMs= */ std::chrono::milliseconds(10000));
      LOGW("CHRE restarted! Recover time: %" PRIu64 "ms.",
           ::android::elapsedRealtime() - startTime);
      chreConnection->mCallback->onChreRestarted();
    }
  }
}

[[noreturn]] void ExynosChreConnection::messageSenderTask(
    ExynosChreConnection *chreConnection) {
  LOGI("Message sender task is launched");
  int chreFd = chreConnection->getChreFileDescriptor();
  while (true) {
    chreConnection->mSendingQueue.waitForMessage();
    MessageToChre &message = chreConnection->mSendingQueue.front();
    auto size =
        TEMP_FAILURE_RETRY(write(chreFd, &message, message.getMessageSize()));
    if (size < 0) {
      LOGE("Failed to write to chre file descriptor. errno=%d\n", errno);
    }
    chreConnection->mSendingQueue.pop();
  }
}

bool ExynosChreConnection::sendMessage(void *data, size_t length) {
  if (length <= 0 || length > kMaxSendingPayloadBytes) {
    LOGE("length %zu is not within the accepted range", length);
    return false;
  }
  return mSendingQueue.emplace(data, length);
}

void ExynosChreConnection::handleMessageFromChre(
    ExynosChreConnection *chreConnection, const unsigned char *messageBuffer,
    size_t messageLen) {
  int64_t startTime = ::android::elapsedRealtime();
  bool isWakelockAcquired =
      acquire_wake_lock(PARTIAL_WAKE_LOCK, kWakeLock) == 0;
  if (!isWakelockAcquired) {
    LOGE("Failed to acquire the wakelock before handling a message");
  } else {
    LOGV("Wakelock is acquired before handling a message");
  }
  HalClientId hostClientId;
  fbs::ChreMessage messageType = fbs::ChreMessage::NONE;
  if (!HostProtocolHost::extractHostClientIdAndType(
          messageBuffer, messageLen, &hostClientId, &messageType)) {
    LOGW("Failed to extract host client ID from message - sending broadcast");
    hostClientId = chre::kHostClientIdUnspecified;
  }
  LOGV("Received a message (type: %hhu, len: %zu) from CHRE for client %d",
       messageType, messageLen, hostClientId);

  switch (messageType) {
    case fbs::ChreMessage::LowPowerMicAccessRequest: {
      chreConnection->mLpmaHandler.enable(/* enabled= */ true);
      break;
    }
    case fbs::ChreMessage::LowPowerMicAccessRelease: {
      chreConnection->mLpmaHandler.enable(/* enabled= */ false);
      break;
    }
    case fbs::ChreMessage::PulseResponse: {
      chreConnection->notifyChreBackOnline();
      break;
    }
    case fbs::ChreMessage::MetricLog:
    case fbs::ChreMessage::NanConfigurationRequest:
    case fbs::ChreMessage::TimeSyncRequest:
    case fbs::ChreMessage::LogMessage: {
      LOGE("Unsupported message type %hhu received from CHRE", messageType);
      break;
    }
    default: {
      chreConnection->mCallback->handleMessageFromChre(messageBuffer,
                                                       messageLen);
      break;
    }
  }
  if (isWakelockAcquired) {
    if (release_wake_lock(kWakeLock)) {
      LOGE("Failed to release the wake lock");
    } else {
      LOGV("The wake lock is released after handling a message.");
    }
  }
  int64_t durationMs = ::android::elapsedRealtime() - startTime;
  if (durationMs > kMessageHandlingTimeThreshold.count()) {
    LOGW("It takes %" PRIu64 "ms to handle a message with ClientId=%" PRIu16
         " Type=%" PRIu8,
         durationMs, hostClientId, static_cast<uint8_t>(messageType));
  }
}
}  // namespace aidl::android::hardware::contexthub
