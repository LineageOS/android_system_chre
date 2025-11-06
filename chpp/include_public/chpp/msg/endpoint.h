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

#ifndef CHPP_MSG_ENDPOINT_H_
#define CHPP_MSG_ENDPOINT_H_

/**
 * @file
 * CHPP Message Service provides a mechanism for components connected
 * via CHPP to tap into the Context Hub Session-Based Messaging network,
 * enabling communication with other endpoints in the network, like nanoapps
 * and GMS Core. It's a bidirectional/peer-to-peer interface, however by
 * convention the service is exposed on a side with the primary CHRE instance,
 * and the clients on the peripherial side of CHPP. Clients should integrate
 * by implementing the API and callbacks defined here.
 *
 * See chre_api/chre/msg.h for key concepts and usage flows.
 * Also see android.hardware.contexthub.HubEndpoint and related Android APIs
 * for more details.
 */

#include <stdbool.h>
#include <stdint.h>

#include "chre/pal/system.h"
#include "chre/pal/version.h"
#include "chre_api/chre/msg.h"
#include "pw_allocator/unique_ptr.h"

// C implementation, remove after migrating to C++ implementation below
struct chppMsgEndpointCallbacks {
  /**
   * Callback when requested endpoint is ready.
   *
   * @param hubId
   * @param endpointId
   */
  void (*onEndpointReady)(uint64_t hubId, uint64_t endpointId);

  /**
   * Callback when requested service is ready.
   *
   * @param hubId
   * @param endpointId
   * @param serviceDescriptor
   */
  void (*onServiceReady)(uint64_t hubId, uint64_t endpointId,
                         const char *serviceDescriptor);

  /**
   * Callback when session is opened.
   *
   * @param session
   */
  void (*onSessionOpened)(const struct chreMsgSessionInfo &session);

  /**
   * Callback when session is closed.
   *
   * @param session
   */
  void (*onSessionClosed)(const struct chreMsgSessionInfo &session);

  /**
   * Callback when session is requested. Use openSessionComplete to accept
   * or closeSession to decline.
   *
   * @param session
   */
  void (*onSessionOpenRequest)(const struct chreMsgSessionInfo &session);

  /**
   * Callback used to pass a message to the CHPP client, e.g. modem FW.
   *
   * @param data Event data to distribute to clients.
   * @param messageType.
   * @param messagePermissions.
   * @param sessionId
   */
  void (*onMessageReceived)(pw::UniquePtr<std::byte[]> &&data,
                            uint32_t messageType, uint32_t messagePermissions,
                            uint16_t sessionId);
};

struct chppMsgEndpointApi {
  /**
   * Initializes the Message module. Initialization must complete synchronously.
   *
   * @param systemApi Structure containing CHRE system function pointers which
   *        the PAL implementation should prefer to use over equivalent
   *        functionality exposed by the underlying platform. The module does
   *        not need to deep-copy this structure; its memory remains
   *        accessible at least until after close() is called.
   * @param callbacks Structure containing entry points to the core CHRE
   *        system. The module does not need to deep-copy this structure; its
   *        memory remains accessible at least until after close() is called.
   * @param appContext Optional. ChppAppState if available.
   *
   * @return true if initialization was successful, false otherwise
   */
  bool (*open)(const struct chrePalSystemApi *systemApi,
               const struct chppMsgEndpointCallbacks *callbacks);

  /**
   * Performs clean shutdown of the Message module, usually done in preparation
   * for stopping the CHRE. The Message module must ensure that it will not
   * invoke any callbacks past this point, and complete any relevant teardown
   * activities before returning from this function.
   */
  void (*close)(void);

  /**
   * Publishes services exposed by this endpoint, which will be included with
   * the endpoint metadata visible to other endpoints in the system.
   *
   * @param services A non-null pointer to the list of services to publish.
   * @param numServices The number of services to publish, i.e. the length of
   * the services array.
   *
   * @return true if the publishing is successful.
   */
  bool (*publishServices)(const struct chreMsgServiceInfo *services,
                          size_t numServices);

  /**
   * Configures whether to receive updates regarding an endpoint that is
   * connected with a message hub and a specific service.  The hubId can be
   * CHRE_MSG_HUB_ID_ANY to configure notifications for matching endpoints that
   * are connected with any message hub. The endpoint ID can be
   * CHRE_MSG_ENDPOINT_ID_ANY to configure notifications for all endpoints that
   * match the given hub.
   *
   * If succeeds, the nanoapp will receive callback notifications TBD
   *
   * If one or more endpoints matching the filter are already ready when this
   * function is called, callback will immediately happens.
   *
   * @param fromEndpointId The endpoint ID of the endpoint from which
   *     the request is made. For firmware client, use ENDPOINT_ID_ANY.
   * @param hubId The message hub ID of the endpoint for which to configure
   *     notifications for all endpoints that are connected with any message
   *     hub.
   * @param endpointId The endpoint ID of the endpoint for which to configure
   *     notifications.
   * @param enable true to enable notifications.
   *
   * @return true on success
   */
  bool (*configureEndpointReadyEvents)(uint64_t fromEndpointId, uint64_t hubId,
                                       uint64_t endpointId, bool enable);

  /**
   * Configures whether to receive updates regarding all endpoints that are
   * connected with the message hub that provide the specified service.
   *
   * If succeeds, it will receive callback notifications TBD.
   *
   * If one or more endpoints matching the filter are already ready when this
   * function is called, callback will be immediately happens.
   *
   * @param fromEndpointId The endpoint ID of the endpoint from which
   *     the request is made. For firmware client, use ENDPOINT_ID_ANY.
   * @param hubId The message hub ID of the endpoint for which to configure
   *     notifications for all endpoints that are connected with any message
   *     hub.
   * @param serviceDescriptor The descriptor of the service associated with the
   *     endpoint for which to configure notifications, a null-terminated ASCII
   *     string. If not NULL, the underlying memory must outlive the
   * notifications configuration. If NULL, this will return false.
   * @param enable true to enable notifications.
   *
   * @return true on success
   */
  bool (*configureServiceReadyEvents)(uint64_t fromEndpointId, uint64_t hubId,
                                      const char *serviceDescriptor,
                                      bool enable);

  /**
   * Opens a session with an endpoint.
   *
   * If this function returns true, the result of session initiation will be
   * provided by a onSessionOpened() or onSessionClosed() callback
   * containing the same hub ID, endpoint ID, and service descriptor
   * parameters. Only one active session for each unique combination of
   * parameters is permitted at a time.
   *
   * @param fromEndpointId The endpoint ID of the endpoint from which
   *     the request is made. For firmware client, use ENDPOINT_ID_ANY.
   * @param hubId The message hub ID of the endpoint. Can be CHRE_MSG_HUB_ID_ANY
   *     to open a session with the default endpoint.
   * @param endpointId The endpoint ID of the endpoint. Can be
   *     CHRE_MSG_ENDPOINT_ID_ANY to open a session with a specified service.
   *     The service cannot be NULL in this case.
   * @param serviceDescriptor The descriptor of the service associated with the
   *     endpoint with which to open the session, a null-terminated ASCII
   * string. Can be NULL. The underlying memory must remain valid at least until
   * the session is closed - for example, it should be a pointer to a static
   * const variable hard-coded in the nanoapp. NOTE: as event data supplied to
   * nanoapps does not live beyond the nanoappHandleEvent() invocation, it is
   * NOT valid to use the serviceData array provided inside
   * chreMsgServiceReadyEvent here.
   *
   * @return true if the request was successfully dispatched, or false if a
   *     synchronous error occurred, in which case no subsequent event will be
   *     sent.
   */
  bool (*openSession)(uint64_t fromEndpointId, uint64_t hubId,
                      uint64_t endpointId, const char *serviceDescriptor);

  /**
   * Close a session or decline an open session request
   *
   * @param sessionId.
   */
  bool (*closeSession)(uint16_t sessionId);

  /**
   * Accept an open session request
   *
   * @param sessionId.
   */
  bool (*openSessionComplete)(uint16_t sessionId);

  /**
   * Send a message.
   *
   * @param data.
   * @param messageType.
   * @param messagePermissions.
   * @param sessionId.
   * @param fromEndpointId The endpoint ID of the endpoint from which
   *     the request is made. For firmware client, use ENDPOINT_ID_ANY.
   *
   * @return
   */
  bool (*sendMessage)(pw::UniquePtr<std::byte[]> &&data, uint32_t messageType,
                      uint32_t messagePermissions, uint16_t sessionId,
                      uint64_t fromEndpointId);
};

// C++ implementation
// TODO(b/476239875) delete the C implementation above when both
// service and client are migrated over.
class IChppMsgEndpointCallbacks {
 public:
  /**
   * Callback when this endpoint is initialized.
   *
   * @param endpoint
   */
  virtual void onEndpointInitialized(
      const struct chreMsgEndpointInfo &endpointInfo) = 0;

  /**
   * Callback when requested endpoint is ready.
   *
   * @param hubId
   * @param endpointId
   */
  virtual void onEndpointReady(uint64_t hubId, uint64_t endpointId) = 0;

  /**
   * Callback when requested service is ready.
   *
   * @param hubId
   * @param endpointId
   * @param serviceDescriptor
   */
  virtual void onServiceReady(uint64_t hubId, uint64_t endpointId,
                              const char *serviceDescriptor) = 0;

  /**
   * Callback when session is opened.
   *
   * @param session
   */
  virtual void onSessionOpened(const struct chreMsgSessionInfo &session) = 0;

  /**
   * Callback when session is closed.
   *
   * @param session
   */
  virtual void onSessionClosed(const struct chreMsgSessionInfo &session) = 0;

  /**
   * Callback when session is requested. Use openSessionComplete to accept
   * or closeSession to decline.
   *
   * @param session
   */
  virtual void onSessionOpenRequest(
      const struct chreMsgSessionInfo &session) = 0;

  /**
   * Callback used to pass a message to the CHPP client, e.g. modem FW.
   *
   * @param data Event data to distribute to clients.
   * @param messageType.
   * @param messagePermissions.
   * @param sessionId
   */
  virtual void onMessageReceived(pw::UniquePtr<std::byte[]> &&data,
                                 uint32_t messageType,
                                 uint32_t messagePermissions,
                                 uint16_t sessionId) = 0;

  virtual ~IChppMsgEndpointCallbacks() = default;
};

class IChppMsgEndpointApi {
 public:
  /**
   * Publishes services exposed by this endpoint, which will be included with
   * the endpoint metadata visible to other endpoints in the system.
   *
   * @param services A non-null pointer to the list of services to publish.
   * @param numServices The number of services to publish, i.e. the length of
   * the services array.
   *
   * @return true if the publishing is successful.
   */
  virtual bool publishServices(const struct chreMsgServiceInfo *services,
                               size_t numServices) = 0;

  /**
   * Configures whether to receive updates regarding an endpoint that is
   * connected with a message hub and a specific service.  The hubId can be
   * CHRE_MSG_HUB_ID_ANY to configure notifications for matching endpoints that
   * are connected with any message hub. The endpoint ID can be
   * CHRE_MSG_ENDPOINT_ID_ANY to configure notifications for all endpoints that
   * match the given hub.
   *
   * If succeeds, the nanoapp will receive callback notifications TBD
   *
   * If one or more endpoints matching the filter are already ready when this
   * function is called, callback will immediately happens.
   *
   * @param fromEndpointId The endpoint ID of the endpoint from which
   *     the request is made. For firmware client, use ENDPOINT_ID_ANY.
   * @param hubId The message hub ID of the endpoint for which to configure
   *     notifications for all endpoints that are connected with any message
   *     hub.
   * @param endpointId The endpoint ID of the endpoint for which to configure
   *     notifications.
   * @param enable true to enable notifications.
   *
   * @return true on success
   */
  virtual bool configureEndpointReadyEvents(uint64_t fromEndpointId,
                                            uint64_t hubId, uint64_t endpointId,
                                            bool enable) = 0;

  /**
   * Configures whether to receive updates regarding all endpoints that are
   * connected with the message hub that provide the specified service.
   *
   * If succeeds, it will receive callback notifications TBD.
   *
   * If one or more endpoints matching the filter are already ready when this
   * function is called, callback will be immediately happens.
   *
   * @param fromEndpointId The endpoint ID of the endpoint from which
   *     the request is made. For firmware client, use ENDPOINT_ID_ANY.
   * @param hubId The message hub ID of the endpoint for which to configure
   *     notifications for all endpoints that are connected with any message
   *     hub.
   * @param serviceDescriptor The descriptor of the service associated with the
   *     endpoint for which to configure notifications, a null-terminated ASCII
   *     string. If not NULL, the underlying memory must outlive the
   * notifications configuration. If NULL, this will return false.
   * @param enable true to enable notifications.
   *
   * @return true on success
   */
  virtual bool configureServiceReadyEvents(uint64_t fromEndpointId,
                                           uint64_t hubId,
                                           const char *serviceDescriptor,
                                           bool enable) = 0;

  /**
   * Opens a session with an endpoint.
   *
   * If this function returns true, the result of session initiation will be
   * provided by a onSessionOpened() or onSessionClosed() callback
   * containing the same hub ID, endpoint ID, and service descriptor
   * parameters. Only one active session for each unique combination of
   * parameters is permitted at a time.
   *
   * @param fromEndpointId The endpoint ID of the endpoint from which
   *     the request is made. For firmware client, use ENDPOINT_ID_ANY.
   * @param hubId The message hub ID of the endpoint. Can be CHRE_MSG_HUB_ID_ANY
   *     to open a session with the default endpoint.
   * @param endpointId The endpoint ID of the endpoint. Can be
   *     CHRE_MSG_ENDPOINT_ID_ANY to open a session with a specified service.
   *     The service cannot be NULL in this case.
   * @param serviceDescriptor The descriptor of the service associated with the
   *     endpoint with which to open the session, a null-terminated ASCII
   * string. Can be NULL. The underlying memory must remain valid at least until
   * the session is closed - for example, it should be a pointer to a static
   * const variable hard-coded in the nanoapp. NOTE: as event data supplied to
   * nanoapps does not live beyond the nanoappHandleEvent() invocation, it is
   * NOT valid to use the serviceData array provided inside
   * chreMsgServiceReadyEvent here.
   *
   * @return true if the request was successfully dispatched, or false if a
   *     synchronous error occurred, in which case no subsequent event will be
   *     sent.
   *
   * @return true on success
   */
  virtual bool openSession(uint64_t fromEndpointId, uint64_t hubId,
                           uint64_t endpointId,
                           const char *serviceDescriptor) = 0;

  /**
   * Close a session or decline an open session request
   *
   * @param sessionId.
   *
   * @return true on success
   */
  virtual bool closeSession(uint16_t sessionId) = 0;

  /**
   * Accept an open session request
   *
   * @param sessionId.
   *
   * @return true on success
   */
  virtual bool openSessionComplete(uint16_t sessionId) = 0;

  /**
   * Send a message.
   *
   * @param data.
   * @param messageType.
   * @param messagePermissions.
   * @param sessionId.
   * @param fromEndpointId The endpoint ID of the endpoint from which
   *     the request is made. For firmware client, use ENDPOINT_ID_ANY.
   *
   * @return true on success
   */
  virtual bool sendMessage(pw::UniquePtr<std::byte[]> &&data,
                           uint32_t messageType, uint32_t messagePermissions,
                           uint16_t sessionId, uint64_t fromEndpointId) = 0;

  virtual ~IChppMsgEndpointApi() = default;
};

/**
 * Register a CHPP service as message endpoint.
 *
 * @param appContext input, ChppAppState of the calling CHPP service.
 * @param callbacks input, IChppMsgEndpointCallbacks implementation of the
 *     calling CHPP service.
 *
 * @return IChppMsgEndpointApi on success, nullptr otherwise.
 */
IChppMsgEndpointApi *registerChppMsgEndpoint(
    struct ChppAppState *appContext, IChppMsgEndpointCallbacks *callbacks);

#endif  // CHPP_MSG_ENDPOINT_H_
