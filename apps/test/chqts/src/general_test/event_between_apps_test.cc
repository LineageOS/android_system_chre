/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <chre/util/nanoapp/log.h>
#include <general_test/event_between_apps_test.h>

#include <general_test/nanoapp_info.h>

#include <shared/macros.h>
#include <shared/nano_endian.h>
#include <shared/nano_string.h>
#include <shared/send_message.h>

#include "chre_api/chre.h"

#ifndef LOG_TAG
#define LOG_TAG "[event_between_apps_test]"
#endif

using nanoapp_testing::MessageType;
using nanoapp_testing::sendSuccessToHost;

namespace general_test {

namespace {
// Arbitrary, just to confirm our data is properly sent.
constexpr uint32_t kMagic = UINT32_C(0x51501984);

// Arbitrary as long as it's different from
// CHRE_EVENT_MESSAGE_FROM_HOST (which this value assures us).
constexpr uint16_t kEventType = CHRE_EVENT_FIRST_USER_VALUE;
}  // namespace

EventBetweenApps0::~EventBetweenApps0() {
  if (mMagic != nullptr) {
    chreHeapFree(mMagic);
  }
}

void EventBetweenApps0::setUp(uint32_t messageSize,
                              const void * /* message */) {
  if (messageSize != 0) {
    EXPECT_FAIL_RETURN("Initial message expects 0 additional bytes, got ",
                       &messageSize);
  }

  mMagic = static_cast<uint32_t *>(chreHeapAlloc(sizeof(uint32_t)));
  if (mMagic == nullptr) {
    EXPECT_FAIL_RETURN("Failed to allocate mem for mMagic", &messageSize);
  }
  // Arbitrary, just to confirm our data is properly sent.
  *mMagic = kMagic;

  NanoappInfo info;
  info.sendToHost();
}

void EventBetweenApps0::handleEvent(uint32_t senderInstanceId,
                                    uint16_t eventType, const void *eventData) {
  uint32_t app1InstanceId;
  const void *message = getMessageDataFromHostEvent(
      senderInstanceId, eventType, eventData, MessageType::kContinue,
      sizeof(app1InstanceId));
  if (mContinueCount > 0) {
    EXPECT_FAIL_RETURN("Multiple kContinue messages sent");
  }

  mContinueCount++;
  nanoapp_testing::memcpy(&app1InstanceId, message, sizeof(app1InstanceId));
  app1InstanceId = nanoapp_testing::littleEndianToHost(app1InstanceId);
  // It's safe to strip the 'const' because we're using nullptr for our
  // free callback.
  // Send an event to app1.  Note since app1 is on the same system, there are
  // no endian concerns for our sendData.
  chreSendEvent(kEventType, mMagic, nullptr, app1InstanceId);
  LOGI("App0 has sent the magic number");
}

EventBetweenApps1::EventBetweenApps1()
    : Test(CHRE_API_VERSION_1_0),
      mApp0InstanceId(CHRE_INSTANCE_ID),
      mReceivedInstanceId(CHRE_INSTANCE_ID) {}

void EventBetweenApps1::setUp(uint32_t messageSize,
                              const void * /* message */) {
  if (messageSize != 0) {
    EXPECT_FAIL_RETURN("Initial message expects 0 additional bytes, got ",
                       &messageSize);
  }

  NanoappInfo appInfo;
  appInfo.sendToHost();
}

void EventBetweenApps1::handleEvent(uint32_t senderInstanceId,
                                    uint16_t eventType, const void *eventData) {
  if (eventType == CHRE_EVENT_MESSAGE_FROM_HOST) {
    const void *message = getMessageDataFromHostEvent(
        senderInstanceId, eventType, eventData, MessageType::kContinue,
        sizeof(mApp0InstanceId));
    // We expect kContinue once, with the app0's instance ID as data.
    if (mApp0InstanceId != CHRE_INSTANCE_ID) {
      // We know app0's instance ID can't be CHRE_INSTANCE_ID, otherwise
      // we would have aborted this test in commonInit().
      EXPECT_FAIL_RETURN("Multiple kContinue messages from host.");
    }
    nanoapp_testing::memcpy(&mApp0InstanceId, message, sizeof(mApp0InstanceId));
    mApp0InstanceId = nanoapp_testing::littleEndianToHost(mApp0InstanceId);

  } else if (eventType == kEventType) {
    if (mReceivedInstanceId != CHRE_INSTANCE_ID) {
      EXPECT_FAIL_RETURN("Multiple messages from other nanoapp.");
    }
    if (senderInstanceId == CHRE_INSTANCE_ID) {
      EXPECT_FAIL_RETURN(
          "Received event from other nanoapp with CHRE_INSTANCE_ID for sender");
    }
    mReceivedInstanceId = senderInstanceId;
    uint32_t magic;
    nanoapp_testing::memcpy(&magic, eventData, sizeof(magic));
    LOGI("App1 has received the magic number");
    if (magic != kMagic) {
      EXPECT_FAIL_RETURN("Got incorrect magic data: ", &magic);
    }
  } else {
    unexpectedEvent(eventType);
  }

  if (mApp0InstanceId != CHRE_INSTANCE_ID &&
      mReceivedInstanceId != CHRE_INSTANCE_ID) {
    if (mApp0InstanceId == mReceivedInstanceId) {
      sendSuccessToHost();
    } else {
      EXPECT_FAIL_RETURN("Got bad sender instance ID for nanoapp event: ",
                         &mReceivedInstanceId);
    }
  }
}

}  // namespace general_test
