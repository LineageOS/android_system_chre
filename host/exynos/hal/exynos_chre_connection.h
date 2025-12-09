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

#ifndef EXYNOS_CHRE_CONNECTION_H_
#define EXYNOS_CHRE_CONNECTION_H_

#include "chre_connection.h"
#include "chre_connection_callback.h"
#include "chre_host/host_protocol_host.h"
#include "chre_host/log.h"
#include "chre_host/log_message_parser.h"
#include "chre_host/st_hal_lpma_handler.h"

#include <unistd.h>
#include <cassert>
#include <future>
#include <queue>
#include <thread>

using android::chre::StHalLpmaHandler;

namespace aidl::android::hardware::contexthub {

using namespace ::android::hardware::contexthub::common::implementation;
using ::android::chre::HostProtocolHost;

/** A class handling message transmission between context hub HAL and CHRE. */
class ExynosChreConnection : public ChreConnection {
 public:
  explicit ExynosChreConnection(ChreConnectionCallback *callback)
      : mCallback(callback), mLpmaHandler(/* allowed= */ true) {
    mPayload = std::make_unique<uint8_t[]>(kMaxReceivingPayloadBytes);
  }

  ~ExynosChreConnection() override {
    close(mChreFileDescriptor);
    if (mMessageListener.joinable()) {
      mMessageListener.join();
    }
    if (mMessageHandler.joinable()) {
      mMessageHandler.join();
    }
    if (mMessageSender.joinable()) {
      mMessageSender.join();
    }
    if (mStateListener.joinable()) {
      mStateListener.join();
    }
  }

  static void handleMessageFromChre(ExynosChreConnection *chreConnection,
                                    const unsigned char *messageBuffer,
                                    size_t messageLen);

  bool init() override;

  bool sendMessage(void *data, size_t length) override;

  void waitChreBackOnline(std::chrono::milliseconds timeoutMs) {
    flatbuffers::FlatBufferBuilder builder(48);
    HostProtocolHost::encodePulseRequest(builder);

    std::unique_lock lock(mChrePulseMutex);
    // reset mIsChreRecovered before sending a PulseRequest message
    mIsChreBackOnline = false;
    sendMessage(builder.GetBufferPointer(), builder.GetSize());
    mChrePulseCondition.wait_for(
        lock, timeoutMs,
        [&isChreBackOnline = mIsChreBackOnline] { return isChreBackOnline; });
  }

  void notifyChreBackOnline() {
    {
      std::unique_lock lock(mChrePulseMutex);
      mIsChreBackOnline = true;
    }
    mChrePulseCondition.notify_all();
  }

 private:
  // The wakelock used to keep device awake while handleUsfMsgAsync() is being
  // called.
  static constexpr char kWakeLock[] = "exynos_chre_hal_wakelock";

  // Max payload size that can be sent to CHRE
  static constexpr uint32_t kMaxSendingPayloadBytes = 0x8000;  // 32K

  // Max payload size that can be received from CHRE
  static constexpr uint32_t kMaxReceivingPayloadBytes = 0x8000;  // 32K

  // Max overhead of the nanoapp binary payload caused by the fbs encapsulation
  static constexpr uint32_t kMaxPayloadOverheadBytes = 1024;

  static constexpr char kChreFileDescriptorPath[] = "/dev/exynos_chre";

  // Wrapper for a message sent to CHRE
  struct MessageToChre {
    // TODO(b/425474601) - A proposed format for sending message to CHRE.
    //  This magic number is embedded in the payload for potential security
    //  check. The format is pending finalization.
    uint32_t magic = 0x67728269;
    uint32_t payloadSize = 0;
    uint8_t payload[kMaxSendingPayloadBytes]{};

    MessageToChre(void *data, size_t length) {
      assert(length <= kMaxSendingPayloadBytes);
      memcpy(payload, data, length);
      payloadSize = static_cast<uint32_t>(length);
    }

    [[nodiscard]] uint32_t getMessageSize() const {
      return sizeof(magic) + sizeof(payloadSize) + payloadSize;
    }
  };

  // Wrapper for a message from CHRE
  struct MessageFromChre {
    std::unique_ptr<uint8_t[]> buffer;
    size_t size;

    MessageFromChre(void *data, ssize_t length) {
      buffer = std::make_unique<uint8_t[]>(length);
      memcpy(buffer.get(), data, length);
      size = length;
    }
  };

  // A queue suitable for multiple producers and a single consumer.
  template <typename ElementType>
  class SynchronousMessageQueue {
   public:
    explicit SynchronousMessageQueue(size_t capacity) : mCapacity(capacity) {}

    bool emplace(void *data, size_t length) {
      std::unique_lock lock(mMutex);
      if (mQueue.size() >= mCapacity) {
        LOGE("Message queue is full!");
        return false;
      }
      mQueue.emplace(data, length);
      mCv.notify_all();
      return true;
    }

    void pop() {
      std::unique_lock lock(mMutex);
      mQueue.pop();
    }

    ElementType &front() {
      std::unique_lock lock(mMutex);
      return mQueue.front();
    }

    void waitForMessage() {
      std::unique_lock lock(mMutex);
      mCv.wait(lock, [&] { return !mQueue.empty(); });
    }

    size_t size() {
      return mQueue.size();
    }

   private:
    const size_t mCapacity;
    std::mutex mMutex;
    std::condition_variable mCv;
    std::queue<ElementType> mQueue;
  };

  // The task receiving message from CHRE
  [[noreturn]] static void messageListenerTask(
      ExynosChreConnection *chreConnection);

  // The task handling message from CHRE
  [[noreturn]] static void messageHandlerTask(
      ExynosChreConnection *chreConnection);

  // The task sending message to CHRE
  [[noreturn]] static void messageSenderTask(
      ExynosChreConnection *chreConnection);

  // The task receiving CHRE state update
  [[noreturn]] static void chreStateMonitorTask(
      ExynosChreConnection *chreConnection);

  [[nodiscard]] int getChreFileDescriptor() const {
    return mChreFileDescriptor;
  }

  bool isChreRestarted();

  // The file descriptor for communication with CHRE
  int mChreFileDescriptor = 0;

  // The calback function that should be implemented by HAL
  ChreConnectionCallback *mCallback;

  // the message listener thread that receives messages from CHRE
  std::thread mMessageListener;
  // the message handling thread that handles messages from CHRE
  std::thread mMessageHandler;
  // the message sender thread that sends messages to CHRE
  std::thread mMessageSender;
  // the status listener thread that hosts chreStateMonitorTask
  std::thread mStateListener;

  // Payload received from CHRE
  std::unique_ptr<uint8_t[]> mPayload;

  // The LPMA handler to talk to the ST HAL
  StHalLpmaHandler mLpmaHandler;

  // Queues for sending to and receiving messages from CHRE, with heuristic
  // capacity size.
  SynchronousMessageQueue<MessageToChre> mSendingQueue{/* capacity= */ 64};
  SynchronousMessageQueue<MessageFromChre> mReceivingQueue{/* capacity= */ 256};

  // Mutex and CV are used to get PulseResponse from CHRE synchronously.
  std::mutex mChrePulseMutex;
  std::condition_variable mChrePulseCondition;
  bool mIsChreBackOnline =
      false;  // set to true after CHRE recovers from a restart
};
}  // namespace aidl::android::hardware::contexthub

#endif  // EXYNOS_CHRE_CONNECTION_H_
