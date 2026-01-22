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

#ifdef CHPP_SERVICE_ENABLED_MESSAGE

#include "chpp/services/message.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "chpp/common/message.h"
#include "chpp/log.h"
#include "chpp/macros.h"
#include "chpp/memory.h"
#include "chpp/services.h"
#include "chre/util/pigweed/pal_pw_allocator.h"
#include "chre/util/system/message_common.h"
#include "chre/util/unique_ptr.h"
#include "pw_allocator/unique_ptr.h"

/************************************************
 *  Prototypes
 ***********************************************/

static enum ChppAppErrorCode chppDispatchMsgRequest(void *serviceContext,
                                                    uint8_t *buf, size_t len);
static void chppMsgServiceNotifyReset(void *serviceContext);

/************************************************
 *  Private Definitions
 ***********************************************/

/**
 * Configuration parameters for this service
 */
static const struct ChppService kMsgServiceConfig = {
    .descriptor.uuid = CHPP_UUID_MESSAGE_STANDARD,

    // Human-readable name
    .descriptor.name = "Message",

    // Version
    .descriptor.version.major = 1,
    .descriptor.version.minor = 0,
    .descriptor.version.patch = 0,

    // Notifies service if CHPP is reset
    .resetNotifierFunctionPtr = &chppMsgServiceNotifyReset,

    // Client request dispatch function pointer
    .requestDispatchFunctionPtr = &chppDispatchMsgRequest,

    // Client notification dispatch function pointer
    .notificationDispatchFunctionPtr = NULL,  // Not supported

    // Min length is the entire header
    .minLength = sizeof(struct ChppAppHeader),
};

/**
 * Structure to maintain state for the message service and its Request/Response
 * (RR) functionality.
 */
struct ChppMsgServiceState {
  struct ChppEndpointState service;  // CHPP service state
  IChppMsgEndpointApi *api;

  struct ChppIncomingRequestState open;   // Service init state
  struct ChppIncomingRequestState close;  // Service deinit state
  struct ChppIncomingRequestState configureEndpointReadyEvents;
  struct ChppIncomingRequestState configureServiceReadyEvents;
  struct ChppIncomingRequestState openSession;
  struct ChppIncomingRequestState closeSession;
  struct ChppIncomingRequestState openSessionComplete;
  struct ChppIncomingRequestState sendMessage;

  struct chreMsgEndpointInfo endpointInfo;
};

// TODO(b/458485882): add support for multiple instance
// Note: This global definition of gMsgServiceContext supports only one
// instance of the CHPP message service at a time.
struct ChppMsgServiceState gMsgServiceContext;

class ChppMsgEndpointCallbacks : public IChppMsgEndpointCallbacks {
  void onEndpointInitialized(
      const struct chreMsgEndpointInfo &endpointInfo) override;
  void onEndpointReady(uint64_t hubId, uint64_t endpointId) override;
  void onServiceReady(uint64_t hubId, uint64_t endpointId,
                      const char *serviceDescriptor) override;
  void onSessionOpened(const struct chreMsgSessionInfo &session) override;
  void onSessionClosed(const struct chreMsgSessionInfo &session) override;
  void onSessionOpenRequest(const struct chreMsgSessionInfo &session) override;
  void onMessageReceived(pw::UniquePtr<std::byte[]> &&data,
                         uint32_t messageType, uint32_t messagePermissions,
                         uint16_t sessionId) override;
};

/************************************************
 *  Prototypes
 ***********************************************/

static enum ChppAppErrorCode chppMsgServiceOpen(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader);
static enum ChppAppErrorCode chppMsgServiceClose(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader);
static enum ChppAppErrorCode chppMsgServiceConfigureEndpointReadyEvents(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgServiceConfigureServiceReadyEvents(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgServiceOpenSession(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgServiceCloseSession(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgServiceOpenSessionComplete(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len);
static enum ChppAppErrorCode chppMsgServiceSendMessage(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len);

/************************************************
 *  Private Functions
 ***********************************************/

/**
 * Dispatches a client request from the transport layer that is determined to be
 * for the message service. If the result of the dispatch is an error, this
 * function responds to the client with the same error.
 *
 * This function is called from the app layer using its function pointer given
 * during service registration.
 *
 * @param serviceContext Maintains status for each service instance.
 * @param buf Input data. Cannot be null.
 * @param len Length of input data in bytes.
 *
 * @return Indicates the result of this function call.
 */
static enum ChppAppErrorCode chppDispatchMsgRequest(void *serviceContext,
                                                    uint8_t *buf, size_t len) {
  struct ChppAppHeader *rxHeader = (struct ChppAppHeader *)buf;
  buf += sizeof(struct ChppAppHeader);
  len -= sizeof(struct ChppAppHeader);

  struct ChppMsgServiceState *messageServiceContext =
      (struct ChppMsgServiceState *)serviceContext;
  struct ChppIncomingRequestState *inReqState = NULL;
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;
  bool dispatched = true;

  switch (rxHeader->command) {
    case CHPP_MESSAGE_OPEN: {
      inReqState = &messageServiceContext->open;
      chppTimestampIncomingRequest(inReqState, rxHeader);
      error = chppMsgServiceOpen(messageServiceContext, rxHeader);
      break;
    }

    case CHPP_MESSAGE_CLOSE: {
      inReqState = &messageServiceContext->close;
      chppTimestampIncomingRequest(inReqState, rxHeader);
      error = chppMsgServiceClose(messageServiceContext, rxHeader);
      break;
    }

    case CHPP_MESSAGE_CONFIGURE_ENDPOINT_READY_EVENTS: {
      inReqState = &messageServiceContext->configureEndpointReadyEvents;
      chppTimestampIncomingRequest(inReqState, rxHeader);
      error = chppMsgServiceConfigureEndpointReadyEvents(messageServiceContext,
                                                         rxHeader, buf, len);
      break;
    }

    case CHPP_MESSAGE_CONFIGURE_SERVICE_READY_EVENTS: {
      inReqState = &messageServiceContext->configureServiceReadyEvents;
      chppTimestampIncomingRequest(inReqState, rxHeader);
      error = chppMsgServiceConfigureServiceReadyEvents(messageServiceContext,
                                                        rxHeader, buf, len);
      break;
    }

    case CHPP_MESSAGE_OPEN_SESSION: {
      inReqState = &messageServiceContext->openSession;
      chppTimestampIncomingRequest(inReqState, rxHeader);
      error =
          chppMsgServiceOpenSession(messageServiceContext, rxHeader, buf, len);
      break;
    }

    case CHPP_MESSAGE_CLOSE_SESSION: {
      inReqState = &messageServiceContext->closeSession;
      chppTimestampIncomingRequest(inReqState, rxHeader);
      error =
          chppMsgServiceCloseSession(messageServiceContext, rxHeader, buf, len);
      break;
    }

    case CHPP_MESSAGE_OPEN_SESSION_COMPLETE: {
      inReqState = &messageServiceContext->openSessionComplete;
      chppTimestampIncomingRequest(inReqState, rxHeader);
      error = chppMsgServiceOpenSessionComplete(messageServiceContext, rxHeader,
                                                buf, len);
      break;
    }

    case CHPP_MESSAGE_SEND_MESSAGE: {
      inReqState = &messageServiceContext->sendMessage;
      chppTimestampIncomingRequest(inReqState, rxHeader);
      error =
          chppMsgServiceSendMessage(messageServiceContext, rxHeader, buf, len);
      break;
    }

    default: {
      CHPP_LOGW("Message service receives invalid command");
      dispatched = false;
      error = CHPP_APP_ERROR_INVALID_COMMAND;
      break;
    }
  }

  if (dispatched && error != CHPP_APP_ERROR_NONE) {
    // Request was dispatched but an error was returned. Close out
    // chppTimestampIncomingRequest()
    chppTimestampOutgoingResponse(inReqState);
  }

  return error;
}

/**
 * Send response for the open and close requests.
 *
 * @param serviceContext Maintains status for each service instance.
 * @param requestHeader App layer header of the request.
 *
 * @return Indicates the result of this function call.
 */
static enum ChppAppErrorCode chppMsgServiceSendResponse(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader) {
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;
  struct ChppAppHeader *response =
      chppAllocResponseFixed(requestHeader, struct ChppAppHeader);
  size_t responseLen = sizeof(*response);

  if (response == NULL) {
    CHPP_LOG_OOM();
    error = CHPP_APP_ERROR_OOM;
  } else {
    chppSendTimestampedResponseOrFail(messageServiceContext->service.appContext,
                                      &messageServiceContext->open, response,
                                      responseLen);
  }
  return error;
}

/**
 * Initializes the Message service upon an open request from the client and
 * responds to the client with the result.
 *
 * @param serviceContext Maintains status for each service instance.
 * @param requestHeader App layer header of the request.
 *
 * @return Indicates the result of this function call.
 */
static enum ChppAppErrorCode chppMsgServiceOpen(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader) {
  CHPP_LOGI("%s", __func__);
  // The open function is called if firmware resets. In case that happens,
  // we should probably close any opened sessions.
  return chppMsgServiceSendResponse(messageServiceContext, requestHeader);
}

/**
 * Deinitializes the Message service.
 *
 * @param serviceContext Maintains status for each service instance.
 * @param requestHeader App layer header of the request.
 *
 * @return Indicates the result of this function call.
 */
static enum ChppAppErrorCode chppMsgServiceClose(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader) {
  return chppMsgServiceSendResponse(messageServiceContext, requestHeader);
}

/**
 * Notifies the service of an incoming reset.
 *
 * @param serviceContext Maintains status for each service instance.
 */
static void chppMsgServiceNotifyReset(void * /* serviceContext */) {
  // TODO(b/453756093) handle firmware resets.
  // This is called if firmware resets. In case that happens, we should
  // probably close any opened sessions and unpublish services.
  // Will need additional API support from the message router.
}

/************************************************
 *  chppMsgEndpointApi Functions
 ***********************************************/
static enum ChppAppErrorCode chppMsgServiceConfigureEndpointReadyEvents(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len) {
  CHPP_LOGI("%s", __func__);
  UNUSED_VAR(requestHeader);
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;

  if (len < sizeof(struct ChppMsgEndpointReadyParameters)) {
    error = CHPP_APP_ERROR_INVALID_ARG;
  } else {
    struct ChppMsgEndpointReadyParameters *parameters =
        (struct ChppMsgEndpointReadyParameters *)buf;

    if (!messageServiceContext->api->configureEndpointReadyEvents(
            messageServiceContext->endpointInfo.endpointId, parameters->hubId,
            parameters->endpointId, parameters->enable)) {
      error = CHPP_APP_ERROR_UNSPECIFIED;
    }
  }
  return error;
}

static enum ChppAppErrorCode chppMsgServiceConfigureServiceReadyEvents(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len) {
  UNUSED_VAR(requestHeader);
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;

  if (len < sizeof(struct ChppMsgServiceReadyParameters)) {
    error = CHPP_APP_ERROR_INVALID_ARG;
  } else {
    struct ChppMsgServiceReadyParameters *parameters =
        (struct ChppMsgServiceReadyParameters *)buf;

    if (!messageServiceContext->api->configureServiceReadyEvents(
            messageServiceContext->endpointInfo.endpointId, parameters->hubId,
            parameters->serviceDescriptor, parameters->enable)) {
      error = CHPP_APP_ERROR_UNSPECIFIED;
    }
  }

  return error;
}

static enum ChppAppErrorCode chppMsgServiceOpenSession(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len) {
  UNUSED_VAR(requestHeader);
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;

  if (len < sizeof(struct ChppMsgOpenSessionParameters)) {
    error = CHPP_APP_ERROR_INVALID_ARG;
  } else {
    struct ChppMsgOpenSessionParameters *parameters =
        (struct ChppMsgOpenSessionParameters *)buf;

    if (!messageServiceContext->api->openSession(
            messageServiceContext->endpointInfo.endpointId, parameters->hubId,
            parameters->endpointId, parameters->serviceDescriptor)) {
      error = CHPP_APP_ERROR_UNSPECIFIED;
    }
  }

  return error;
}

static enum ChppAppErrorCode chppMsgServiceCloseSession(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len) {
  UNUSED_VAR(requestHeader);
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;

  if (len < sizeof(struct ChppMsgSessionRequestParameters)) {
    error = CHPP_APP_ERROR_INVALID_ARG;
  } else {
    struct ChppMsgSessionRequestParameters *parameters =
        (struct ChppMsgSessionRequestParameters *)buf;

    if (!messageServiceContext->api->closeSession(parameters->sessionId)) {
      error = CHPP_APP_ERROR_UNSPECIFIED;
    }
  }

  return error;
}

static enum ChppAppErrorCode chppMsgServiceOpenSessionComplete(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len) {
  UNUSED_VAR(requestHeader);
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;

  if (len < sizeof(struct ChppMsgSessionRequestParameters)) {
    error = CHPP_APP_ERROR_INVALID_ARG;
  } else {
    struct ChppMsgSessionRequestParameters *parameters =
        (struct ChppMsgSessionRequestParameters *)buf;

    if (!messageServiceContext->api->openSessionComplete(
            parameters->sessionId)) {
      error = CHPP_APP_ERROR_UNSPECIFIED;
    }
  }

  return error;
}

static enum ChppAppErrorCode chppMsgServiceSendMessage(
    struct ChppMsgServiceState *messageServiceContext,
    struct ChppAppHeader *requestHeader, uint8_t *buf, size_t len) {
  UNUSED_VAR(requestHeader);
  enum ChppAppErrorCode error = CHPP_APP_ERROR_NONE;

  if (len < sizeof(struct ChppMsgDataParameters)) {
    error = CHPP_APP_ERROR_INVALID_ARG;
  } else {
    struct ChppMsgDataParameters *parameters =
        (struct ChppMsgDataParameters *)buf;
    static ChrePalPwAllocator allocator(
        messageServiceContext->service.appContext->systemApi);
    pw::UniquePtr<std::byte[]> messageData =
        allocator.MakeUniqueArray<std::byte>(parameters->size);
    memcpy(messageData.get(), parameters->data, parameters->size);

    if (!messageServiceContext->api->sendMessage(
            std::move(messageData), parameters->messageType,
            parameters->messagePermissions, parameters->sessionId,
            messageServiceContext->endpointInfo.endpointId)) {
      error = CHPP_APP_ERROR_UNSPECIFIED;
    }
  }

  return error;
}

/************************************************
 *  chppMsgEndpointCallbacks Functions
 ***********************************************/
void ChppMsgEndpointCallbacks::onEndpointInitialized(
    const struct chreMsgEndpointInfo &endpointInfo) {
  CHPP_LOGI("%s: hub 0x%" PRIx64 ", endpoint 0x%" PRIx64, __func__,
            endpointInfo.hubId, endpointInfo.endpointId);
  gMsgServiceContext.endpointInfo = endpointInfo;
}

void ChppMsgEndpointCallbacks::onEndpointReady(uint64_t hubId,
                                               uint64_t endpointId) {
  CHPP_LOGI("%s: hub 0x%" PRIx64 ", endpoint 0x%" PRIx64, __func__, hubId,
            endpointId);
  struct ChppMsgOnEndpointReadyNotification *notification =
      chppAllocServiceNotificationFixed(
          struct ChppMsgOnEndpointReadyNotification);

  if (notification == NULL) {
    CHPP_LOG_OOM();
  } else {
    notification->header.handle = gMsgServiceContext.service.handle;
    notification->header.type = CHPP_MESSAGE_TYPE_SERVICE_NOTIFICATION;
    notification->header.transaction = 0;
    notification->header.error = CHPP_APP_ERROR_NONE;

    notification->header.command = CHPP_MESSAGE_ON_ENDPOINT_READY;
    notification->params.hubId = hubId;
    notification->params.endpointId = endpointId;

    chppEnqueueTxDatagramOrFail(
        gMsgServiceContext.service.appContext->transportContext, notification,
        sizeof(*notification));
  }
}

void ChppMsgEndpointCallbacks::onServiceReady(uint64_t hubId,
                                              uint64_t endpointId,
                                              const char *serviceDescriptor) {
  CHPP_LOGI("%s: hub %" PRIu64 ", endpoint %" PRIu64 ", service %s", __func__,
            hubId, endpointId, serviceDescriptor);
  struct ChppMsgOnServiceReadyNotification *notification =
      chppAllocServiceNotificationFixed(
          struct ChppMsgOnServiceReadyNotification);

  if (notification == NULL) {
    CHPP_LOG_OOM();
  } else {
    notification->header.handle = gMsgServiceContext.service.handle;
    notification->header.type = CHPP_MESSAGE_TYPE_SERVICE_NOTIFICATION;
    notification->header.transaction = 0;
    notification->header.error = CHPP_APP_ERROR_NONE;

    notification->header.command = CHPP_MESSAGE_ON_SERVICE_READY;
    notification->params.hubId = hubId;
    notification->params.endpointId = endpointId;
    strncpy(notification->params.serviceDescriptor, serviceDescriptor,
            sizeof(notification->params.serviceDescriptor) - 1);
    notification->params
        .serviceDescriptor[sizeof(notification->params.serviceDescriptor) - 1] =
        0;

    chppEnqueueTxDatagramOrFail(
        gMsgServiceContext.service.appContext->transportContext, notification,
        sizeof(*notification));
  }
}

static void chppMsgServiceHandleSessionNotification(
    const struct chreMsgSessionInfo &session, ChppMsgCommands command) {
  CHPP_LOGI("%s: command %d", __func__, command);
  struct ChppMsgSessionNotification *notification =
      chppAllocServiceNotificationFixed(struct ChppMsgSessionNotification);

  if (notification == NULL) {
    CHPP_LOG_OOM();
  } else {
    notification->header.handle = gMsgServiceContext.service.handle;
    notification->header.type = CHPP_MESSAGE_TYPE_SERVICE_NOTIFICATION;
    notification->header.transaction = 0;
    notification->header.error = CHPP_APP_ERROR_NONE;

    notification->header.command = command;
    notification->params.sessionId = session.sessionId;
    notification->params.hubId = session.hubId;
    notification->params.endpointId = session.endpointId;
    notification->params.reason = session.reason;
    strncpy(notification->params.serviceDescriptor, session.serviceDescriptor,
            sizeof(notification->params.serviceDescriptor) - 1);
    notification->params
        .serviceDescriptor[sizeof(notification->params.serviceDescriptor) - 1] =
        0;

    chppEnqueueTxDatagramOrFail(
        gMsgServiceContext.service.appContext->transportContext, notification,
        sizeof(*notification));
  }
}

void ChppMsgEndpointCallbacks::onSessionOpened(
    const struct chreMsgSessionInfo &session) {
  chppMsgServiceHandleSessionNotification(session,
                                          CHPP_MESSAGE_ON_SESSION_OPENED);
}

void ChppMsgEndpointCallbacks::onSessionClosed(
    const struct chreMsgSessionInfo &session) {
  chppMsgServiceHandleSessionNotification(session,
                                          CHPP_MESSAGE_ON_SESSION_CLOSED);
}

void ChppMsgEndpointCallbacks::onSessionOpenRequest(
    const struct chreMsgSessionInfo &session) {
  chppMsgServiceHandleSessionNotification(session,
                                          CHPP_MESSAGE_ON_SESSION_OPEN_REQUEST);
}

void ChppMsgEndpointCallbacks::onMessageReceived(
    pw::UniquePtr<std::byte[]> &&data, uint32_t messageType,
    uint32_t messagePermissions, uint16_t sessionId) {
  CHPP_LOGI("%s: size %zu, session %" PRIu8, __func__, data.size(), sessionId);
  struct ChppMsgOnMessageReceivedNotification *notification =
      chppAllocServiceNotificationTypedArray(
          struct ChppMsgOnMessageReceivedNotification, data.size() - 1,
          params.data);

  if (notification == NULL) {
    CHPP_LOG_OOM();
  } else {
    notification->header.handle = gMsgServiceContext.service.handle;
    notification->header.type = CHPP_MESSAGE_TYPE_SERVICE_NOTIFICATION;
    notification->header.transaction = 0;
    notification->header.error = CHPP_APP_ERROR_NONE;

    notification->header.command = CHPP_MESSAGE_ON_MESSAGE_RECEIVED;
    notification->params.messageType = messageType;
    notification->params.messagePermissions = messagePermissions;
    notification->params.sessionId = sessionId;
    notification->params.size = static_cast<uint32_t>(data.size());
    memcpy(notification->params.data, data.get(), data.size());

    chppEnqueueTxDatagramOrFail(
        gMsgServiceContext.service.appContext->transportContext, notification,
        sizeof(*notification) + notification->params.size - 1);
  }
}

/************************************************
 *  Public Functions
 ***********************************************/

void chppRegisterMessageService(struct ChppAppState *appContext) {
  static ChppMsgEndpointCallbacks chppMsgEndpointCallbacks;

  // registerChppMessageEndpoint will generates an endpoint ID
  gMsgServiceContext.api =
      registerChppMsgEndpoint(appContext, &chppMsgEndpointCallbacks);

  if (gMsgServiceContext.api == nullptr) {
    CHPP_DEBUG_ASSERT_LOG(false,
                          "Failed to register CHPP Msg Endpoint service");
  } else {
    // register CHPP service
    chppRegisterService(appContext, (void *)&gMsgServiceContext,
                        &gMsgServiceContext.service, NULL /*outReqStates*/,
                        &kMsgServiceConfig);
    CHPP_DEBUG_ASSERT(gMsgServiceContext.service.handle);

    // the service is always open
    gMsgServiceContext.service.openState = CHPP_OPEN_STATE_OPENED;
  }
}

void chppDeregisterMessageService(struct ChppAppState * /* appContext */) {
  // no-op
}

#endif
