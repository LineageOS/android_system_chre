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

#include "location/lbs/contexthub/test_suite/chre_fake_api/chre_api_ble_fake.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "location/lbs/contexthub/test_suite/chre_unit_test.h"

namespace {

using ::lbs::contexthub::BleScanFilter;
using ::lbs::contexthub::BleScanMode;

TEST_NANOAPP(ChreApiBleFakeTest, GetCapabilities) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleGetCapabilities);
  ASSERT_EQ(chreBleGetCapabilities(), 0);
}

TEST_NANOAPP(ChreApiBleFakeTest, GetFilterCapabilities) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleGetFilterCapabilities);
  ASSERT_EQ(chreBleGetFilterCapabilities(), 0);
}

TEST_NANOAPP(ChreApiBleFakeTest, StartScanAsync) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleStartScanAsync);
  // Returns false because PAL stub is nullptr.
  ASSERT_FALSE(chreBleStartScanAsync(BleScanMode::CHRE_BLE_SCAN_MODE_BACKGROUND,
                                     0 /* reportDelayMs */,
                                     nullptr /* scanFilter */));
}

TEST_NANOAPP(ChreApiBleFakeTest, StopScanAsync) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleStopScanAsync);
  // Returns true because request does not change maximal request in
  // multiplexer.
  ASSERT_TRUE(chreBleStopScanAsync());
}

TEST_NANOAPP(ChreApiBleFakeTest, FlushAsync) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleFlushAsync);
  // Returns false because flush is not implemented yet.
  ASSERT_FALSE(chreBleFlushAsync(nullptr));
}

TEST_NANOAPP(ChreApiBleFakeTest, ReadRssiAsync) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleReadRssiAsync);
  // Returns false because PAL stub is nullptr.
  EXPECT_FALSE(chreBleReadRssiAsync(0, nullptr));
}

TEST_NANOAPP(ChreApiBleFakeTest, StartScanAsyncV1_9) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleStartScanAsyncV1_9);
  // Returns false because PAL stub is nullptr.
  ASSERT_FALSE(chreBleStartScanAsyncV1_9(
      BleScanMode::CHRE_BLE_SCAN_MODE_BACKGROUND, 0 /* reportDelayMs */,
      nullptr /* scanFilter */, nullptr /* cookie */));
}

TEST_NANOAPP(ChreApiBleFakeTest, StopScanAsyncV1_9) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleStopScanAsyncV1_9);
  // Returns true because request does not change maximal request in
  // multiplexer.
  ASSERT_TRUE(chreBleStopScanAsyncV1_9(nullptr /* cookie */));
}

TEST_NANOAPP(ChreApiBleFakeTest, GetScanStatus) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleGetScanStatus);
  // Returns true because request does not change maximal request in
  // multiplexer.
  ASSERT_FALSE(chreBleGetScanStatus(nullptr /* status */));
}

TEST_NANOAPP(ChreApiBleFakeTest, SocketAccept) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleSocketAccept);
  // Returns true because request does not change maximal request in
  // multiplexer.
  ASSERT_TRUE(chreBleSocketAccept(0 /* socketId */));
}

TEST_NANOAPP(ChreApiBleFakeTest, SocketSend) {
  EXPECT_CALL(*chre_api_fake_detector_, chreBleSocketSend);
  // Returns true because request does not change maximal request in
  // multiplexer.
  ASSERT_EQ(chreBleSocketSend(0 /* socketId */, nullptr /* data */,
                              0 /* length */, nullptr /* freeCallback */),
            chreBleSocketSendStatus::CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS);
}

}  // namespace
