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

/* Lat lng change nanoapp requests gnss location. After it receives 10
 * responses, it returns the average change in lat and lng per second. The final
 * host message type is a pair.
 */

#include <chre.h>

#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "chre/util/nanoapp/log.h"

#define LOG_TAG "[GetGnssNanoapp]"

int received_messages = 0;
int cookie_nb = 433;
int32_t lat_0;
int32_t lng_0;
uint64_t t_0;

void freeMessage(void* msg, size_t /* size */) { chreHeapFree(msg); }

bool nanoappStart(void) {
  LOGD("Nanoapp successfully started.");
  chreGnssLocationSessionStartAsync(/*minIntervalMs=*/100,
                                    /*minTimeToNextFixMs=*/0,
                                    /*cookie=*/static_cast<void*>(&cookie_nb));
  return true;
}

void nanoappEnd(void) { LOGD("NanoappEnd triggered."); }

void nanoappHandleEvent(uint32_t /* sender_instance_id */, uint16_t event_type,
                        const void* event_data) {
  if (event_type == CHRE_EVENT_GNSS_LOCATION) {
    auto event = static_cast<const chreGnssLocationEvent*>(event_data);
    LOGD("Received event at time=%" PRIu64 " with lat=%d and long=%d",
     event->timestamp,
     event->latitude_deg_e7,
     event->longitude_deg_e7);
    if (received_messages == 0) {
      lat_0 = event->latitude_deg_e7;
      lng_0 = event->longitude_deg_e7;
      t_0 = event->timestamp;
    } else if (received_messages == 9) {
      auto response_pair = static_cast<std::pair<int32_t, int32_t>*>(
          chreHeapAlloc(sizeof(std::pair<int32_t, int32_t>)));
      auto t_diff_scaled = (event->timestamp - t_0) * 1e4;
      response_pair->first = (event->latitude_deg_e7 - lat_0) / t_diff_scaled;
      response_pair->second = (event->longitude_deg_e7 - lng_0) / t_diff_scaled;
      // This nanoapp uses CHRE_HOST_ENDPOINT_BROADCAST in order to provide a
      // simple example of GNSS usage. CHRE_HOST_ENDPOINT_BROADCAST should not
      // be used in any deployed nanoapp.
      chreSendMessageToHostEndpoint(response_pair, sizeof(*response_pair), 5,
                                    CHRE_HOST_ENDPOINT_BROADCAST, freeMessage);
      chreGnssLocationSessionStopAsync(static_cast<void*>(&cookie_nb));
    }
    received_messages++;
  }
}

#ifdef SIMULATION_LOAD_STATIC
#include "chre/platform/static_nanoapp_init.h"

CHRE_STATIC_NANOAPP_INIT(
    LatLngChange, 0x12345600000, 0x00000001,
    chre::NanoappPermissions::CHRE_PERMS_GNSS)  // NANOAPP_ID = 0x12345600000,
                                                // NANOAPP_VERSION = 0x00000001
#endif                                          // SIMULATION_LOAD_STATIC
