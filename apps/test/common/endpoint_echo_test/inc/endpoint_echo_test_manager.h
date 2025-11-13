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
#include "chre/util/pigweed/rpc_server.h"
#include "chre/util/singleton.h"
#include "chre/util/time.h"
#include "chre_api/chre.h"
#include "endpoint_echo_test.rpc.pb.h"

class EndpointEchoTestService final
    : public chre::rpc::pw_rpc::nanopb::EndpointEchoTestService::Service<
          EndpointEchoTestService> {
 public:
  void RunNanoappToHostTest(const google_protobuf_Empty &request,
                            ServerWriter<chre_rpc_ReturnStatus> &writer);
};

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

  /**
   * Sets the permission for the next server message.
   *
   * @params permission Bitmasked CHRE_MESSAGE_PERMISSION_.
   */
  void setPermissionForNextMessage(uint32_t permission);

  /**
   * Starts the nanoapp-initiated part of the test.
   * @param writer The writer to use to send the test status.
   */
  void startTest(
      EndpointEchoTestService::ServerWriter<chre_rpc_ReturnStatus> &&writer);

 private:
  /** The service descriptor for the echo service. */
  constexpr static char kTestEchoServiceDescriptor[] =
      "android.hardware.contexthub.test.EchoService";

  /** The echo test service used for endpoint messaging. */
  constexpr static chreMsgServiceInfo kTestEchoService = {
      .majorVersion = 1,
      .minorVersion = 0,
      .serviceDescriptor = kTestEchoServiceDescriptor,
      .serviceFormat = chreMsgEndpointServiceFormat::
          CHRE_MSG_ENDPOINT_SERVICE_FORMAT_CUSTOM};

  /** The timeout for the test. */
  constexpr static chre::Nanoseconds kTestTimeout =
      chre::Nanoseconds(5 * chre::kOneSecondInNanoseconds);

  /** The phases of the test. */
  enum class TestPhase {
    kOpenSession,
    kSendMessage,
    kCloseSession,
  };

  /**
   * Handle a CHRE event for the nanoapp -> host -> nanoapp test path.
   *
   * @param senderInstanceId    the instand ID that sent the event.
   * @param eventType           the type of the event.
   * @param eventData           the data for the event.
   * @return                    true if the event was handled, false otherwise.
   */
  bool handleEventNanoappToHostTest(uint32_t senderInstanceId,
                                    uint16_t eventType, const void *eventData);

  /**
   * Handle a CHRE event for the host -> nanoapp -> host test path.
   *
   * @param senderInstanceId    the instand ID that sent the event.
   * @param eventType           the type of the event.
   * @param eventData           the data for the event.
   * @return                    true if the event was handled, false otherwise.
   */
  bool handleEventHostToNanoappTest(uint32_t senderInstanceId,
                                    uint16_t eventType, const void *eventData);

  /** Runs the nanoapp-initiated part of the test. */
  void runNanoappToHostTest(TestPhase phase);

  /**
   * Sends the test status to the host.
   * @param success Whether the test passed.
   * @param errorMessage The error message if the test failed.
   */
  void sendTestStatus(bool success, const char *errorMessage);

  /** Sends a test pass status to the host. */
  void passTest();

  /** Sends a test fail status to the host. */
  void failTest(const char *errorMessage);

  /** pw_rpc service used to process the RPCs. */
  EndpointEchoTestService mEndpointEchoTestService;

  /** RPC server. */
  chre::RpcServer mServer;

  /** The open session for the echo service. */
  chre::Optional<chreMsgSessionInfo> mOpenSession;

  /** The timer handle for the test. */
  uint32_t mTimerHandle = CHRE_TIMER_INVALID;

  /** The writer to use to send the test status. */
  chre::Optional<EndpointEchoTestService::ServerWriter<chre_rpc_ReturnStatus>>
      mWriter;

  /** Whether the nanoapp-initiated part of the test is in progress. */
  bool mNanoappToHostTestInProgress = false;

  /** The session ID for the echo service. */
  uint16_t mSessionId = CHRE_MSG_SESSION_ID_INVALID;

  /** The message to send for the test. */
  uint8_t mMessageBuffer[10];
};

typedef chre::Singleton<EndpointEchoTestManager>
    EndpointEchoTestManagerSingleton;

#endif  // ENDPOINT_ECHO_TEST_MANAGER_H_
