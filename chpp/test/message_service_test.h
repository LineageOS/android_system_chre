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

#ifndef CHPP_MESSAGE_SERVICE_TEST_H_
#define CHPP_MESSAGE_SERVICE_TEST_H_

#include "chpp/common/message.h"
#include "transport_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************************************
 *  Functions necessary for unit testing
 ***********************************************/

static constexpr uint16_t kTestSessionId = 1024;
static constexpr const char *kTestServiceDescriptor =
    "com.google.chre.chpp.test";
static constexpr uint64_t kTestNanoappId = 0x0123456789000017;
static constexpr uint64_t kChreMessageHubId = 0x476f6f676c000008;
static constexpr uint64_t kFromEndpointId = 0x101;

void chrePalMsgSendNotification(ChppMsgCommands command,
                                const char *serviceDescriptor);

size_t chppDequeueTxDatagram(struct ChppTransportState *context);

#ifdef __cplusplus
}
#endif

#endif  // CHPP_MESSAGE_SERVICE_TEST_H_
