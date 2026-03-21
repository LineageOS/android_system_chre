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

/* Echo nanoapp simply sends back to host any message it receives. */

#include <chre.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "chre/util/nanoapp/log.h"

#define LOG_TAG "[EchoNanoapp]"

void freeMessage(void* msg, size_t /* size */) { chreHeapFree(msg); }

bool nanoappStart(void) {
  LOGD("Nanoapp successfully started.");
  return true;
}

void nanoappEnd(void) { LOGD("NanoappEnd triggered."); }

void nanoappHandleEvent(uint32_t sender_instance_id, uint16_t event_type,
                        const void* event_data) {
  if (event_type == CHRE_EVENT_MESSAGE_FROM_HOST) {
    auto event = static_cast<const chreMessageFromHostData*>(event_data);

    auto new_message = chreHeapAlloc(event->messageSize);

    memcpy(new_message, event->message, event->messageSize);

    chreSendMessageToHostEndpoint(new_message, event->messageSize,
                                  event->messageType, sender_instance_id,
                                  freeMessage);
  }
}

#ifdef SIMULATION_LOAD_STATIC
#include "chre/platform/static_nanoapp_init.h"

CHRE_STATIC_NANOAPP_INIT(
    Echo, 0x12345600000, 0x00000001,
    chre::NanoappPermissions::CHRE_PERMS_NONE)  // NANOAPP_ID = 0x12345600000,
                                                // NANOAPP_VERSION = 0x00000001
#endif                                          // SIMULATION_LOAD_STATIC
