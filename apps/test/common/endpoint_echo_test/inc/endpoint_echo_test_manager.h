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

#ifndef ENDPOINT_ECHO_TEST_MANAGER_H_
#define ENDPOINT_ECHO_TEST_MANAGER_H_

#include <cinttypes>
#include <cstdint>

#include "chre/util/optional.h"
#include "chre/util/singleton.h"
#include "chre_api/chre.h"

/**
 * Handles requests for the Endpoint Echo Test nanoapp.
 */
class EndpointEchoTestManager {
 public:
  /**
   * Allows the manager to do any init necessary as part of nanoappStart.
   */
  bool start();

  /**
   * Allows the manager to do any cleanup necessary as part of nanoappEnd.
   */
  void end();

  /**
   * Handle a CHRE event.
   *
   * @param senderInstanceId    the instand ID that sent the event.
   * @param eventType           the type of the event.
   * @param eventData           the data for the event.
   */
  void handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                   const void *eventData);

 private:
  /**
   * The echo test service used for endpoint messaging.
   */
  constexpr static chreMsgServiceInfo kTestEchoService = {
      .majorVersion = 1,
      .minorVersion = 0,
      .serviceDescriptor = "android.hardware.contexthub.test.EchoService",
      .serviceFormat = chreMsgEndpointServiceFormat::
          CHRE_MSG_ENDPOINT_SERVICE_FORMAT_CUSTOM};

  /**
   * The open session for the echo service.
   */
  chre::Optional<chreMsgSessionInfo> mOpenSession;
};

typedef chre::Singleton<EndpointEchoTestManager>
    EndpointEchoTestManagerSingleton;

#endif  // ENDPOINT_ECHO_TEST_MANAGER_H_
