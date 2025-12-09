/*
 * Copyright (C) 2023 The Android Open Source Project
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

#ifndef TINYSYS_CHRE_CONNECTION_H_
#define TINYSYS_CHRE_CONNECTION_H_

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

#include <android-base/thread_annotations.h>

using android::chre::StHalLpmaHandler;

namespace aidl::android::hardware::contexthub {

using namespace ::android::hardware::contexthub::common::implementation;

using ::android::base::ScopedLockAssertion;
using ::android::chre::HostProtocolHost;
using ::android::chre::StHalLpmaHandler;

/** A class handling message transmission between context hub HAL and CHRE. */
// TODO(b/267188769): We should add comments explaining how IPI works.
class TinysysChreConnection : public ChreConnection {
 public:
  explicit TinysysChreConnection(ChreConnectionCallback *callback)
      : mCallback(callback), mLpmaHandler(/* allowed= */ true) {
    mPayload = std::make_unique<uint8_t[]>(kMaxReceivingPayloadBytes);
  }

  ~TinysysChreConnection() override {
    // TODO(b/264308286): Need a decent way to terminate the listener thread.
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

  static void handleMessageFromChre(TinysysChreConnection *chreConnection,
                                    const unsigned char *messageBuffer,
                                    size_t messageLen);

  bool init() override;

  bool sendMessage(void *data, size_t length) override;

  std::string dump() override;

  void waitChreBackOnline(std::chrono::milliseconds timeoutMs) {
    flatbuffers::FlatBufferBuilder builder(48);
    HostProtocolHost::encodePulseRequest(builder);

    std::unique_lock lock(mChrePulseMutex);
    ScopedLockAssertion lockAssertion(mChrePulseMutex);
    // reset mIsChreRecovered before sending a PulseRequest message
    mIsChreBackOnline = false;
    sendMessage(builder.GetBufferPointer(), builder.GetSize());
    mChrePulseCondition.wait_for(lock, timeoutMs, [&] {
      ScopedLockAssertion cvLockAssertion(mChrePulseMutex);
      return mIsChreBackOnline;
    });
  }

  void notifyChreBackOnline() {
    {
      std::lock_guard lock(mChrePulseMutex);
      mIsChreBackOnline = true;
    }
    mChrePulseCondition.notify_all();
  }

 private:
  // The wakelock used to keep device awake while handleUsfMsgAsync() is being
  // called.
  static constexpr char kWakeLock[] = "tinysys_chre_hal_wakelock";

  // Max payload size that can be sent to CHRE
  static constexpr uint32_t kMaxSendingPayloadBytes = 0x8000;  // 32K

  // Max payload size that can be received from CHRE
  static constexpr uint32_t kMaxReceivingPayloadBytes = 0x8000;  // 32K

  // Max overhead of the nanoapp binary payload caused by the fbs encapsulation
  static constexpr uint32_t kMaxPayloadOverheadBytes = 1024;

  // The path to CHRE file descriptor
  static constexpr char kChreFileDescriptorPath[] = "/dev/scp_chre_manager";

  // Wrapper for a message sent to CHRE
  struct MessageToChre {
    // This magic number is the SCP_CHRE_MAGIC constant defined by kernel
    // scp_chre_manager service. The value is embedded in the payload as a
    // security check for proper use of the device node.
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
      TinysysChreConnection *chreConnection);

  // The task handling message from CHRE
  [[noreturn]] static void messageHandlerTask(
      TinysysChreConnection *chreConnection);

  // The task sending message to CHRE
  [[noreturn]] static void messageSenderTask(
      TinysysChreConnection *chreConnection);

  // The task receiving CHRE state update
  [[noreturn]] static void chreStateMonitorTask(
      TinysysChreConnection *chreConnection);

  [[nodiscard]] int getChreFileDescriptor() const {
    return mChreFileDescriptor;
  }

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
  // set to true after CHRE recovers from a restart.
  bool mIsChreBackOnline GUARDED_BY(mChrePulseMutex) = false;
};
}  // namespace aidl::android::hardware::contexthub

#endif  // TINYSYS_CHRE_CONNECTION_H_
