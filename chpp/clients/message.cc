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

#ifdef CHPP_CLIENT_ENABLED_MESSAGE

#include "chpp/clients/message.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "chpp/app.h"
#include "chpp/clients/discovery.h"
#include "chpp/common/message.h"
#include "chpp/log.h"
#include "chpp/macros.h"
#include "chpp/memory.h"
#include "chre/util/pigweed/pal_pw_allocator.h"
#include "pw_allocator/unique_ptr.h"

#ifndef CHPP_MESSAGE_DISCOVERY_TIMEOUT_MS
#define CHPP_MESSAGE_DISCOVERY_TIMEOUT_MS CHPP_DISCOVERY_DEFAULT_TIMEOUT_MS
#endif

/************************************************
 *  Prototypes
 ***********************************************/

static enum ChppAppErrorCode chppDispatchMsgResponse(void *clientContext,
                                                     uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppDispatchMsgNotification(void *clientContext,
                                                         uint8_t *buf,
                                                         size_t len);
static bool chppMsgClientInit(void *clientContext, uint8_t handle,
                              struct ChppVersion serviceVersion);
static void chppMsgClientDeinit(void *clientContext);
static void chppMsgClientNotifyReset(void *clientContext);
static void chppMsgClientNotifyMatch(void *clientContext);

/************************************************
 *  Private Definitions
 ***********************************************/

/**
 * Structure to maintain state for the Message client and its Request/Response
 * (RR) functionality.
 */
struct ChppMsgClientState {
  struct ChppEndpointState client;       // CHPP client state
  const struct chppMsgEndpointApi *api;  // Message PAL API

  struct ChppOutgoingRequestState
      outReqStates[CHPP_MESSAGE_CLIENT_REQUEST_MAX + 1];
};

// Note: This global definition of gMsgClientContext supports only one
// instance of the CHPP Msg client at a time.
struct ChppMsgClientState gMsgClientContext;
static const struct chrePalSystemApi *gSystemApi;
static const struct chppMsgEndpointCallbacks *gCallbacks;

/**
 * Configuration parameters for this client
 */
static const struct ChppClient kMsgClientConfig = {
    .descriptor.uuid = CHPP_UUID_MESSAGE_STANDARD,

    // Version
    .descriptor.version.major = 1,
    .descriptor.version.minor = 0,
    .descriptor.version.patch = 0,

    // Notifies client if CHPP is reset
    .resetNotifierFunctionPtr = &chppMsgClientNotifyReset,

    // Notifies client if they are matched to a service
    .matchNotifierFunctionPtr = &chppMsgClientNotifyMatch,

    // Service response dispatch function pointer
    .responseDispatchFunctionPtr = &chppDispatchMsgResponse,

    // Service notification dispatch function pointer
    .notificationDispatchFunctionPtr = &chppDispatchMsgNotification,

    // Service response dispatch function pointer
    .initFunctionPtr = &chppMsgClientInit,

    // Service notification dispatch function pointer
    .deinitFunctionPtr = &chppMsgClientDeinit,

    // Number of request-response states in the outReqStates array.
    .outReqCount = ARRAY_SIZE(gMsgClientContext.outReqStates),

    // Min length is the entire header
    .minLength = sizeof(struct ChppAppHeader),
};

/************************************************
 *  Prototypes
 ***********************************************/

static bool chppMsgClientOpen(const struct chrePalSystemApi *systemApi,
                              const struct chppMsgEndpointCallbacks *callbacks);
static void chppMsgClientClose(void);

static bool chppMsgClientPublishServices(
    const struct chreMsgServiceInfo *services, size_t numServices);
static bool chppMsgClientConfigureEndpointReadyEvents(uint64_t fromEndpointId,
                                                      uint64_t hubId,
                                                      uint64_t endpointId,
                                                      bool enable);
static bool chppMsgClientConfigureServiceReadyEvents(
    uint64_t fromEndpointId, uint64_t hubId, const char *serviceDescriptor,
    bool enable);
static bool chppMsgClientOpenSession(uint64_t fromEndpointId, uint64_t hubId,
                                     uint64_t endpointId,
                                     const char *serviceDescriptor);
static bool chppMsgClientCloseSession(uint16_t sessionId);
static bool chppMsgClientOpenSessionComplete(uint16_t sessionId);
static bool chppMsgClientSendMessage(pw::UniquePtr<std::byte[]> &&message,
                                     uint32_t messageType,
                                     uint32_t messagePermissions,
                                     uint16_t sessionId,
                                     uint64_t fromEndpointId);

static void chppMsgCloseResult(struct ChppMsgClientState *clientContext,
                               uint8_t *buf, size_t len);

static enum ChppAppErrorCode chppMsgOnEndpointReadyNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgOnServiceReadyNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgOnSessionOpenedNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgOnSessionClosedNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgOnSessionOpenRequestNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgOnMessageReceivedNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len);

/************************************************
 *  Private Functions
 ***********************************************/

/**
 * Dispatches a service response from the transport layer that is determined to
 * be for the Message client.
 *
 * This function is called from the app layer using its function pointer given
 * during client registration.
 *
 * @param clientContext Maintains status for each client instance.
 * @param buf Input data. Cannot be null.
 * @param len Length of input data in bytes.
 *
 * @return Indicates the result of this function call.
 */
static enum ChppAppErrorCode chppDispatchMsgResponse(void *clientContext,
                                                     uint8_t *buf, size_t len) {
  if (buf == nullptr) {
    CHPP_LOGE("%s: buf cannot be null", __func__);
    return CHPP_APP_ERROR_INVALID_ARG;
  }

  struct ChppAppHeader *rxHeader = (struct ChppAppHeader *)buf;
  struct ChppMsgClientState *messageClientContext =
      (struct ChppMsgClientState *)clientContext;
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;

  if (rxHeader->command > CHPP_MESSAGE_CLIENT_REQUEST_MAX) {
    error = CHPP_APP_ERROR_INVALID_COMMAND;

  } else if (!chppTimestampIncomingResponse(
                 messageClientContext->client.appContext,
                 &messageClientContext->outReqStates[rxHeader->command],
                 rxHeader)) {
    error = CHPP_APP_ERROR_UNEXPECTED_RESPONSE;

  } else {
    switch (rxHeader->command) {
      case CHPP_MESSAGE_OPEN: {
        chppClientProcessOpenResponse(&messageClientContext->client, buf, len);
        break;
      }

      case CHPP_MESSAGE_CLOSE: {
        chppMsgCloseResult(messageClientContext, buf, len);
        break;
      }

      case CHPP_MESSAGE_OPEN_SESSION:
      case CHPP_MESSAGE_CLOSE_SESSION:
      case CHPP_MESSAGE_OPEN_SESSION_COMPLETE:
      case CHPP_MESSAGE_SEND_MESSAGE:
        // TODO(b/453756093)
        break;

      default: {
        error = CHPP_APP_ERROR_INVALID_COMMAND;
        break;
      }
    }
  }

  return error;
}

/**
 * Dispatches a service notification from the transport layer that is determined
 * to be for the message client.
 *
 * This function is called from the app layer using its function pointer given
 * during client registration.
 *
 * @param clientContext Maintains status for each client instance.
 * @param buf Input data. Cannot be null.
 * @param len Length of input data in bytes.
 *
 * @return Indicates the result of this function call.
 */
static enum ChppAppErrorCode chppDispatchMsgNotification(void *clientContext,
                                                         uint8_t *buf,
                                                         size_t len) {
  CHPP_LOGI("%s", __func__);
  struct ChppAppHeader *rxHeader = (struct ChppAppHeader *)buf;
  buf += sizeof(struct ChppAppHeader);
  len -= sizeof(struct ChppAppHeader);

  struct ChppMsgClientState *messageClientContext =
      (struct ChppMsgClientState *)clientContext;
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;

  switch (rxHeader->command) {
    case CHPP_MESSAGE_ON_ENDPOINT_READY: {
      chppMsgOnEndpointReadyNotification(messageClientContext, buf, len);
      break;
    }

    case CHPP_MESSAGE_ON_SERVICE_READY: {
      chppMsgOnServiceReadyNotification(messageClientContext, buf, len);
      break;
    }

    case CHPP_MESSAGE_ON_SESSION_OPENED: {
      chppMsgOnSessionOpenedNotification(messageClientContext, buf, len);
      break;
    }

    case CHPP_MESSAGE_ON_SESSION_CLOSED: {
      chppMsgOnSessionClosedNotification(messageClientContext, buf, len);
      break;
    }

    case CHPP_MESSAGE_ON_SESSION_OPEN_REQUEST: {
      chppMsgOnSessionOpenRequestNotification(messageClientContext, buf, len);
      break;
    }

    case CHPP_MESSAGE_ON_MESSAGE_RECEIVED: {
      chppMsgOnMessageReceivedNotification(messageClientContext, buf, len);
      break;
    }

    default: {
      CHPP_LOGE("%s: invalid command", __func__);
      error = CHPP_APP_ERROR_INVALID_COMMAND;
      break;
    }
  }

  return error;
}

/**
 * Initializes the client and provides its handle number and the version of the
 * matched service when/if it the client is matched with a service during
 * discovery.
 *
 * @param clientContext Maintains status for each client instance.
 * @param handle Handle number for this client.
 * @param serviceVersion Version of the matched service.
 *
 * @return True if client is compatible and successfully initialized.
 */
static bool chppMsgClientInit(void *clientContext, uint8_t handle,
                              struct ChppVersion serviceVersion) {
  UNUSED_VAR(serviceVersion);

  struct ChppMsgClientState *messageClientContext =
      (struct ChppMsgClientState *)clientContext;
  chppClientInit(&messageClientContext->client, handle);

  return true;
}

/**
 * Deinitializes the client.
 *
 * @param clientContext Maintains status for each client instance.
 */
static void chppMsgClientDeinit(void *clientContext) {
  struct ChppMsgClientState *messageClientContext =
      (struct ChppMsgClientState *)clientContext;
  chppClientDeinit(&messageClientContext->client);
}

/**
 * Notifies the client of an incoming reset.
 *
 * @param clientContext Maintains status for each client instance.
 */
static void chppMsgClientNotifyReset(void *clientContext) {
  struct ChppMsgClientState *messageClientContext =
      (struct ChppMsgClientState *)clientContext;

  chppClientCloseOpenRequests(&messageClientContext->client, &kMsgClientConfig,
                              false /* clearOnly */);

  // TODO(b/453756093) handle CHRE/AOC resets.
  // This is called if CHRE/AOC resets. In case that happens, we should
  // probably close any opened/re-open sessions and re-publish services.
  //
  if (messageClientContext->client.openState != CHPP_OPEN_STATE_OPENED &&
      !messageClientContext->client.pseudoOpen) {
    CHPP_LOGW("Message client reset but wasn't open");
  } else {
    CHPP_LOGI("Message client reopening from state=%" PRIu8,
              messageClientContext->client.openState);
    chppClientSendOpenRequest(
        &messageClientContext->client,
        &messageClientContext->outReqStates[CHPP_MESSAGE_OPEN],
        CHPP_MESSAGE_OPEN,
        /*blocking=*/false);
  }
}

/**
 * Notifies the client of being matched to a service.
 *
 * @param clientContext Maintains status for each client instance.
 */
static void chppMsgClientNotifyMatch(void *clientContext) {
  struct ChppMsgClientState *messageClientContext =
      (struct ChppMsgClientState *)clientContext;

  if (messageClientContext->client.pseudoOpen) {
    CHPP_LOGD("Pseudo-open Message client opening");
    chppClientSendOpenRequest(
        &messageClientContext->client,
        &messageClientContext->outReqStates[CHPP_MESSAGE_OPEN],
        CHPP_MESSAGE_OPEN,
        /*blocking=*/false);
  }
}

/**
 * Handles the service response for the close client request.
 *
 * This function is called from chppDispatchContextResponse().
 *
 * @param clientContext Maintains status for each client instance.
 * @param buf Input data. Cannot be null, already verified in the dispatcher.
 * @param len Length of input data in bytes.
 */
static void chppMsgCloseResult(struct ChppMsgClientState *clientContext,
                               uint8_t *buf, size_t len) {
  // TODO(b/453756093)
  UNUSED_VAR(clientContext);
  UNUSED_VAR(buf);
  UNUSED_VAR(len);
}

/**
 * Initializes the Message client upon an open request from CHRE and responds
 * with the result.
 *
 * @param systemApi CHRE system function pointers.
 * @param callbacks CHRE entry points.
 *
 * @return True if successful. False otherwise.
 */
static bool chppMsgClientOpen(
    const struct chrePalSystemApi *systemApi,
    const struct chppMsgEndpointCallbacks *callbacks) {
  CHPP_DEBUG_NOT_NULL(systemApi);
  CHPP_DEBUG_NOT_NULL(callbacks);

  bool result = false;
  gSystemApi = systemApi;
  gCallbacks = callbacks;

  if (gMsgClientContext.client.appContext == NULL) {
    CHPP_LOGE("Message client app is null");
  } else {
    // Wait for discovery to complete for "open" call to succeed
    if (chppWaitForDiscoveryComplete(gMsgClientContext.client.appContext,
                                     CHPP_MESSAGE_DISCOVERY_TIMEOUT_MS)) {
      CHPP_LOGI("Message client discovery successful");
      result = chppClientSendOpenRequest(
          &gMsgClientContext.client,
          &gMsgClientContext.outReqStates[CHPP_MESSAGE_OPEN], CHPP_MESSAGE_OPEN,
          /*blocking=*/true);
    } else {
      CHPP_LOGE("Message client discovery failed");
    }
  }

  return result;
}

/**
 * Deinitializes the Message client.
 */
static void chppMsgClientClose(void) {
  // Remote
  struct ChppAppHeader *request = chppAllocClientRequestCommand(
      &gMsgClientContext.client, CHPP_MESSAGE_CLOSE);

  if (request == NULL) {
    CHPP_LOG_OOM();
  } else if (chppClientSendTimestampedRequestAndWait(
                 &gMsgClientContext.client,
                 &gMsgClientContext.outReqStates[CHPP_MESSAGE_CLOSE], request,
                 sizeof(*request))) {
    gMsgClientContext.client.openState = CHPP_OPEN_STATE_CLOSED;
    chppClientCloseOpenRequests(&gMsgClientContext.client, &kMsgClientConfig,
                                true /* clearOnly */);
  }
}

/************************************************
 *  chppMsgEndpointApi Functions
 ***********************************************/
static bool chppMsgClientPublishServices(
    const struct chreMsgServiceInfo *services, size_t numServices) {
  // TODO(b/453756093)
  UNUSED_VAR(services);
  UNUSED_VAR(numServices);
  return false;
}

static bool chppMsgClientConfigureEndpointReadyEvents(uint64_t fromEndpointId,
                                                      uint64_t hubId,
                                                      uint64_t endpointId,
                                                      bool enable) {
  bool result = false;
  CHPP_LOGI("%s", __func__);

  struct ChppMsgConfigureEndpointReadyRequest *request =
      chppAllocClientRequestFixed(&gMsgClientContext.client,
                                  struct ChppMsgConfigureEndpointReadyRequest);

  if (request == NULL) {
    CHPP_LOG_OOM();
  } else {
    request->header.command = CHPP_MESSAGE_CONFIGURE_ENDPOINT_READY_EVENTS;
    request->params.fromEndpointId = fromEndpointId;
    request->params.hubId = hubId;
    request->params.endpointId = endpointId;
    request->params.enable = enable;

    result = chppClientSendTimestampedRequestOrFail(
        &gMsgClientContext.client,
        &gMsgClientContext
             .outReqStates[CHPP_MESSAGE_CONFIGURE_ENDPOINT_READY_EVENTS],
        request, sizeof(*request), CHRE_MESSAGE_ASYNC_RESULT_TIMEOUT_NS);
  }

  CHPP_LOGI("%s result %d", __func__, result);
  return result;
}

static bool chppMsgClientConfigureServiceReadyEvents(
    uint64_t fromEndpointId, uint64_t hubId, const char *serviceDescriptor,
    bool enable) {
  CHPP_LOGI("%s not implemented yet", __func__);
  UNUSED_VAR(fromEndpointId);
  UNUSED_VAR(hubId);
  UNUSED_VAR(serviceDescriptor);
  UNUSED_VAR(enable);
  return false;
}

static bool chppMsgClientOpenSession(uint64_t fromEndpointId, uint64_t hubId,
                                     uint64_t endpointId,
                                     const char *serviceDescriptor) {
  bool result = false;

  struct ChppMsgOpenSessionRequest *request = chppAllocClientRequestFixed(
      &gMsgClientContext.client, struct ChppMsgOpenSessionRequest);

  if (request == NULL) {
    CHPP_LOG_OOM();
  } else {
    request->header.command = CHPP_MESSAGE_OPEN_SESSION;
    request->params.fromEndpointId = fromEndpointId;
    request->params.hubId = hubId;
    request->params.endpointId = endpointId;
    strncpy(request->params.serviceDescriptor, serviceDescriptor,
            sizeof(request->params.serviceDescriptor) - 1);
    request->params
        .serviceDescriptor[sizeof(request->params.serviceDescriptor) - 1] = 0;

    result = chppClientSendTimestampedRequestOrFail(
        &gMsgClientContext.client,
        &gMsgClientContext.outReqStates[CHPP_MESSAGE_OPEN_SESSION], request,
        sizeof(*request), CHRE_MESSAGE_ASYNC_RESULT_TIMEOUT_NS);
  }

  return result;
}

static bool simpleSessionRequest(uint16_t command, uint16_t sessionId) {
  bool result = false;

  struct ChppMsgSessionRequest *request = chppAllocClientRequestFixed(
      &gMsgClientContext.client, struct ChppMsgSessionRequest);

  if (request == NULL) {
    CHPP_LOG_OOM();
  } else {
    request->header.command = command;
    request->params.sessionId = sessionId;

    result = chppClientSendTimestampedRequestOrFail(
        &gMsgClientContext.client, &gMsgClientContext.outReqStates[command],
        request, sizeof(*request), CHRE_MESSAGE_ASYNC_RESULT_TIMEOUT_NS);
  }

  return result;
}

static bool chppMsgClientCloseSession(uint16_t sessionId) {
  return simpleSessionRequest(CHPP_MESSAGE_CLOSE_SESSION, sessionId);
}

static bool chppMsgClientOpenSessionComplete(uint16_t sessionId) {
  return simpleSessionRequest(CHPP_MESSAGE_OPEN_SESSION_COMPLETE, sessionId);
}

static bool chppMsgClientSendMessage(pw::UniquePtr<std::byte[]> &&message,
                                     uint32_t messageType,
                                     uint32_t messagePermissions,
                                     uint16_t sessionId,
                                     uint64_t fromEndpointId) {
  bool result = false;

  struct ChppMsgSendMessageRequest *request = chppAllocClientRequestTypedArray(
      &gMsgClientContext.client, struct ChppMsgSendMessageRequest,
      message.size() - 1, params.data);

  if (request == NULL) {
    CHPP_LOG_OOM();
  } else {
    request->header.command = CHPP_MESSAGE_SEND_MESSAGE;
    request->params.fromEndpointId = fromEndpointId;
    request->params.messageType = messageType;
    request->params.messagePermissions = messagePermissions;
    request->params.sessionId = sessionId;
    request->params.size = static_cast<uint32_t>(message.size());
    memcpy(request->params.data, message.get(), message.size());

    result = chppClientSendTimestampedRequestOrFail(
        &gMsgClientContext.client,
        &gMsgClientContext.outReqStates[CHPP_MESSAGE_SEND_MESSAGE], request,
        sizeof(*request) + request->params.size - 1,
        CHRE_MESSAGE_ASYNC_RESULT_TIMEOUT_NS);
  }

  return result;
}

/************************************************
 *  chppMsgEndpointCallbacks Functions
 ***********************************************/
static enum ChppAppErrorCode chppMsgOnEndpointReadyNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len) {
  UNUSED_VAR(clientContext);
  if (len < sizeof(struct ChppMsgEndpointReadyParameters)) {
    return CHPP_APP_ERROR_INVALID_ARG;
  }

  struct ChppMsgEndpointReadyParameters *param =
      (struct ChppMsgEndpointReadyParameters *)buf;
  gCallbacks->onEndpointReady(param->hubId, param->endpointId);
  return CHPP_APP_ERROR_NONE;
}

static enum ChppAppErrorCode chppMsgOnServiceReadyNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len) {
  UNUSED_VAR(clientContext);
  if (len < sizeof(struct ChppMsgServiceReadyParameters)) {
    return CHPP_APP_ERROR_INVALID_ARG;
  }

  struct ChppMsgServiceReadyParameters *param =
      (struct ChppMsgServiceReadyParameters *)buf;
  gCallbacks->onServiceReady(param->hubId, param->endpointId,
                             param->serviceDescriptor);
  return CHPP_APP_ERROR_NONE;
}

static enum ChppAppErrorCode chppMsgHandleSessionNotification(
    void (*func)(const struct chreMsgSessionInfo &session), uint8_t *buf,
    size_t len) {
  if (len < sizeof(struct ChppMsgSessionNotificationParameters)) {
    return CHPP_APP_ERROR_INVALID_ARG;
  }

  struct ChppMsgSessionNotificationParameters *param =
      (struct ChppMsgSessionNotificationParameters *)buf;
  struct chreMsgSessionInfo session = {.hubId = param->hubId,
                                       .endpointId = param->endpointId,
                                       .sessionId = param->sessionId,
                                       .reason = param->reason};
  strncpy(session.serviceDescriptor, param->serviceDescriptor,
          sizeof(session.serviceDescriptor) - 1);
  session.serviceDescriptor[sizeof(session.serviceDescriptor) - 1] = 0;
  func(session);
  return CHPP_APP_ERROR_NONE;
}

static enum ChppAppErrorCode chppMsgOnSessionOpenedNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len) {
  UNUSED_VAR(clientContext);
  return chppMsgHandleSessionNotification(gCallbacks->onSessionOpened, buf,
                                          len);
}

static enum ChppAppErrorCode chppMsgOnSessionClosedNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len) {
  UNUSED_VAR(clientContext);
  return chppMsgHandleSessionNotification(gCallbacks->onSessionClosed, buf,
                                          len);
}

static enum ChppAppErrorCode chppMsgOnSessionOpenRequestNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len) {
  UNUSED_VAR(clientContext);
  return chppMsgHandleSessionNotification(gCallbacks->onSessionOpenRequest, buf,
                                          len);
}

static enum ChppAppErrorCode chppMsgOnMessageReceivedNotification(
    struct ChppMsgClientState *clientContext, uint8_t *buf, size_t len) {
  CHPP_LOGI("%s: received data len=%" PRIuSIZE, __func__, len);
  UNUSED_VAR(clientContext);
  if (len < sizeof(struct ChppMsgDataParameters)) {
    return CHPP_APP_ERROR_INVALID_ARG;
  }

  struct ChppMsgDataParameters *parameters =
      (struct ChppMsgDataParameters *)buf;
  static ChrePalPwAllocator allocator(gSystemApi);
  pw::UniquePtr<std::byte[]> messageData =
      allocator.MakeUniqueArray<std::byte>(parameters->size);
  memcpy(messageData.get(), parameters->data, parameters->size);

  gCallbacks->onMessageReceived(std::move(messageData), parameters->messageType,
                                parameters->messagePermissions,
                                parameters->sessionId);
  return CHPP_APP_ERROR_NONE;
}

/************************************************
 *  Public Functions
 ***********************************************/

void chppRegisterMessageClient(struct ChppAppState *appContext) {
  CHPP_LOGI("%s", __func__);
  memset(&gMsgClientContext, 0, sizeof(gMsgClientContext));
  chppRegisterClient(appContext, (void *)&gMsgClientContext,
                     &gMsgClientContext.client, gMsgClientContext.outReqStates,
                     &kMsgClientConfig);
}

void chppDeregisterMessageClient(struct ChppAppState *appContext) {
  CHPP_LOGI("%s", __func__);
  UNUSED_VAR(appContext);
}

struct ChppEndpointState *getChppMsgClientState(void) {
  return &gMsgClientContext.client;
}

const struct chppMsgEndpointApi *chppMsgEndpointGetApi(
    uint32_t /* requestedApiVersion */) {
  static const struct chppMsgEndpointApi api = {
      .open = chppMsgClientOpen,
      .close = chppMsgClientClose,
      .publishServices = chppMsgClientPublishServices,
      .configureEndpointReadyEvents = chppMsgClientConfigureEndpointReadyEvents,
      .configureServiceReadyEvents = chppMsgClientConfigureServiceReadyEvents,
      .openSession = chppMsgClientOpenSession,
      .closeSession = chppMsgClientCloseSession,
      .openSessionComplete = chppMsgClientOpenSessionComplete,
      .sendMessage = chppMsgClientSendMessage,
  };

  return &api;
}

#endif
