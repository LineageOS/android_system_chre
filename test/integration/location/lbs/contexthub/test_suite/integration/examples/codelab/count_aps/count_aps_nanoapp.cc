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

/* Count APs nanoapp signs up to receive passive wifi updates. When it receives
 * a message with more than 5 active APs (so 6 or more), it sends the framework
 * a host message of type boolean.
 */

#include <chre.h>

#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "third_party/contexthub/chre/util/include/chre/util/nanoapp/log.h"

#define LOG_TAG "[CountAPsNanoapp]"

int cookie_nb = 433;

namespace chre {

bool nanoappStart(void) {
  LOGD("Nanoapp successfully started.");
  chreWifiConfigureScanMonitorAsync(true, &cookie_nb);
  return true;
}

void nanoappEnd(void) { LOGD("NanoappEnd triggered."); }

void nanoappHandleEvent(uint32_t /* sender_instance_id */, uint16_t event_type,
                        const void* event_data) {
  if (event_type == CHRE_EVENT_WIFI_SCAN_RESULT) {
    auto event = static_cast<const chreWifiScanEvent*>(event_data);
    LOGD("Received event at time=%" PRIu64 " with %" PRIu8 " active APs",
         event->referenceTime, event->resultTotal);
    if (event->resultTotal > 5) {
      bool* it_worked = static_cast<bool*>(chreHeapAlloc(sizeof(bool)));
      *it_worked = true;
      chreSendMessageToHostEndpoint(
          it_worked, sizeof(*it_worked), 5, CHRE_HOST_ENDPOINT_BROADCAST,
          [](void* msg, size_t) { chreHeapFree(msg); });
    }
  }
}
}  // namespace chre

#ifdef SIMULATION_LOAD_STATIC

#include "chre/platform/static_nanoapp_init.h"

CHRE_STATIC_NANOAPP_INIT(
    CountAps, 0x12345600000, 0x00000001,
    chre::NanoappPermissions::CHRE_PERMS_WIFI)  // NANOAPP_ID = 0x12345600000,
                                                // NANOAPP_VERSION = 0x00000001
#endif                                          // SIMULATION_LOAD_STATIC
