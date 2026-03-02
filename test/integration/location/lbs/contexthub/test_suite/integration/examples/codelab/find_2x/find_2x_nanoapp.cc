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

/* Find 2x send a request of type int and expects a response of type int where
 * the response is 2x the request. It then sends a boolean message indicating
 * whether the 2x response was correct or wrong.
 *
 * The request is only sent after 2 minutes.
 */

#include <chre.h>
#include <stdlib.h>
#include <time.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "location/lbs/contexthub/test_suite/integration/examples/codelab/find_2x/find_2x_common.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/time.h"

#define LOG_TAG "[Find2xNanoapp]"

int selected_number = 422;
int cookie = 5;

bool nanoappStart(void) {
  LOGD("Nanoapp successfully started.");
  chreTimerSet(chre::kOneMinuteInNanoseconds * 2, &cookie, true);
  return true;
}

void nanoappEnd(void) { LOGD("NanoappEnd triggered."); }

void nanoappHandleEvent(uint32_t /* sender_instance_id */, uint16_t event_type,
                        const void* event_data) {
  if (event_type == CHRE_EVENT_TIMER) {
    chreSendMessageToHostEndpoint(
        &selected_number, sizeof(selected_number), kFind2xRequestType,
        CHRE_HOST_ENDPOINT_BROADCAST, [](void* /* msg */, size_t) {});
  } else if (event_type == CHRE_EVENT_MESSAGE_FROM_HOST) {
    auto msg = static_cast<const chreMessageFromHostData*>(event_data);
    const int* ans = static_cast<const int*>(msg->message);
    bool* response = static_cast<bool*>(chreHeapAlloc(sizeof(bool)));
    *response = (*ans == 2 * selected_number);
    chreSendMessageToHostEndpoint(response, sizeof(*response),
                                  kFind2xResponseSuccessStatusType,
                                  CHRE_HOST_ENDPOINT_BROADCAST,
                                  [](void* msg, size_t) { chreHeapFree(msg); });
  }
}

#ifdef SIMULATION_LOAD_STATIC
#include "chre/platform/static_nanoapp_init.h"

CHRE_STATIC_NANOAPP_INIT(
    Find2x, 0x12345600000, 0x00000001,
    chre::NanoappPermissions::CHRE_PERMS_NONE)  // NANOAPP_ID = 0x12345600000,
                                                // NANOAPP_VERSION = 0x00000001
#endif                                          // SIMULATION_LOAD_STATIC
