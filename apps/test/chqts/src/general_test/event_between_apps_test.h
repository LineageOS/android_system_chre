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

#ifndef GTS_NANOAPPS_GENERAL_TEST_EVENT_BETWEEN_APPS_TEST_H_
#define GTS_NANOAPPS_GENERAL_TEST_EVENT_BETWEEN_APPS_TEST_H_

#include <general_test/test.h>

namespace general_test {

/**
 * Send CHRE event to another nanoapp.
 *
 * Protocol:
 * This is nanoapp app0.  This test also involves nanoapp app1.
 * All data to/from Host is in little endian.
 *
 * Host to app0:  kEventBetweenApps0, no data
 * app0 to Host:  kContinue, 64-bit app ID, 32-bit instance ID
 * Host to app0:  kContinue, app1's 32-bit instance ID
 * app0 to app1:  kEventType, kMagic
 */
class EventBetweenApps0 : public Test {
 public:
  EventBetweenApps0()
      : Test(/* minSupportedVersion= */ CHRE_API_VERSION_1_0),
        mContinueCount(0) {}
  ~EventBetweenApps0() override;

 protected:
  void handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                   const void *eventData) override;
  void setUp(uint32_t messageSize, const void *message) override;

 private:
  uint32_t *mMagic = nullptr;
  int mContinueCount;
};

/**
 * Receive CHRE event from another nanopp.
 *
 * Protocol:
 * This is nanoapp app1.  This test also involves nanoapp app0.
 * All data to/from Host is in little endian.
 *
 * Host to app1:  kEventBetweenApps1, no data
 * app1 to Host:  kContinue, 64-bit app ID, 32-bit instance ID
 * [NOTE: Next two event can happen in any order]
 * Host to app1:  kContinue, app0's 32-bit instance ID
 * app0 to app1:  kEventType, EventBetweenApps1::kMagic
 * app1 to Host:  kSuccess, no data
 */
class EventBetweenApps1 : public Test {
 public:
  EventBetweenApps1();

 protected:
  void handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                   const void *eventData) override;
  void setUp(uint32_t messageSize, const void *message) override;

 private:
  uint32_t mApp0InstanceId;
  uint32_t mReceivedInstanceId;
};

}  // namespace general_test

#endif  // GTS_NANOAPPS_GENERAL_TEST_EVENT_BETWEEN_APPS_TEST_H_
