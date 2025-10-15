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

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/event_helpers.h"

#include <iostream>

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/helpers_internal.h"
#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

namespace lbs {
namespace contexthub {
namespace helpers {
namespace {

class ChreEventOppositeForTest : public ChreApiEventFunctionsImpl {
 public:
  static bool SendMessageToHost(void * /* message */,
                                uint32_t /* messageSize */,
                                uint32_t /* messageType */,
                                MessageFreeFunction * /* freeCallback */) {
    return false;
  }

  static bool SendMessageToHostEndpoint(
      void * /* message */, size_t /* messageSize */,
      uint32_t /* messageType */, uint16_t /* hostEndpoint */,
      MessageFreeFunction * /* freeCallback */) {
    return false;
  }

  static bool GetNanoappInfoByAppId(uint64_t /* appId */,
                                    NanoappInfo * /* info */) {
    return true;
  }

  static bool GetNanoappInfoByInstanceId(uint32_t /* instanceId */,
                                         NanoappInfo * /* info */) {
    return true;
  }

  static void ConfigureNanoappInfoEvents(bool /* enable */) {}

  static void ConfigureHostSleepStateEvents(bool /* enable */) {}

  static bool IsHostAwake() {
    return true;
  }
};

void SetEventOppositeForTest() {
  lbs::contexthub::helpers::internal::ExtendHelperMethods<
      ChreEventOppositeForTest>(
      ::lbs::contexthub::FakeChreApiProvider::GetInstance()
          ->GetChreApiEventFunctions());
}

// passed as MessageFreeFunction
static void EmptyFreeFunction(void * /* message */, size_t /* messageSize */) {}

TEST_NANOAPP(ChreEventHelpersTest, ExtendHelperTest) {
  NanoappInfo *nanoapp_info = new NanoappInfo;
  int *message = new int;

  SetEmptyEvent();

  EXPECT_TRUE(chreSendMessageToHost(message, 0, 0, EmptyFreeFunction));
  EXPECT_TRUE(
      chreSendMessageToHostEndpoint(message, 0, 0, 0, EmptyFreeFunction));
  EXPECT_FALSE(chreGetNanoappInfoByAppId(0, nanoapp_info));
  EXPECT_FALSE(chreGetNanoappInfoByInstanceId(0, nanoapp_info));
  EXPECT_FALSE(chreIsHostAwake());
  chreConfigureNanoappInfoEvents(false);
  chreConfigureHostSleepStateEvents(false);

  SetEventOppositeForTest();

  EXPECT_FALSE(chreSendMessageToHost(message, 0, 0, EmptyFreeFunction));
  EXPECT_FALSE(
      chreSendMessageToHostEndpoint(message, 0, 0, 0, EmptyFreeFunction));
  EXPECT_TRUE(chreGetNanoappInfoByAppId(0, nanoapp_info));
  EXPECT_TRUE(chreGetNanoappInfoByInstanceId(0, nanoapp_info));
  EXPECT_TRUE(chreIsHostAwake());

  delete nanoapp_info;
  delete message;
}

TEST_NANOAPP(ChreEventHelpersTest, SanityTest) {
  EXPECT_CALL(*chre_api_fake_detector_, chreSendMessageToHost).Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreSendMessageToHostEndpoint).Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreGetNanoappInfoByAppId).Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreGetNanoappInfoByInstanceId)
      .Times(2);
  EXPECT_CALL(*chre_api_fake_detector_, chreConfigureNanoappInfoEvents)
      .Times(1);
  EXPECT_CALL(*chre_api_fake_detector_, chreConfigureHostSleepStateEvents)
      .Times(1);
  EXPECT_CALL(*chre_api_fake_detector_, chreIsHostAwake).Times(2);

  NanoappInfo *nanoapp_info = new NanoappInfo;
  int *message = new int;

  SetEmptyEvent();

  EXPECT_TRUE(chreSendMessageToHost(message, 0, 0, EmptyFreeFunction));
  EXPECT_TRUE(
      chreSendMessageToHostEndpoint(message, 0, 0, 0, EmptyFreeFunction));
  EXPECT_FALSE(chreGetNanoappInfoByAppId(0, nanoapp_info));
  EXPECT_FALSE(chreGetNanoappInfoByInstanceId(0, nanoapp_info));
  EXPECT_FALSE(chreIsHostAwake());
  chreConfigureNanoappInfoEvents(false);
  chreConfigureHostSleepStateEvents(false);

  SetEventHostIsAwake();

  EXPECT_TRUE(chreSendMessageToHost(message, 0, 0, EmptyFreeFunction));
  EXPECT_TRUE(
      chreSendMessageToHostEndpoint(message, 0, 0, 0, EmptyFreeFunction));
  EXPECT_FALSE(chreGetNanoappInfoByAppId(0, nanoapp_info));
  EXPECT_FALSE(chreGetNanoappInfoByInstanceId(0, nanoapp_info));
  EXPECT_TRUE(chreIsHostAwake());

  delete nanoapp_info;
  delete message;
}

TEST_NANOAPP(ChreEventHelpersTest, IsAsleepTest) {
  SetEventHostIsAsleep();
  EXPECT_FALSE(chreIsHostAwake());
}

}  // namespace
}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs