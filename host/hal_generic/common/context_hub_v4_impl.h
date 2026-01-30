/*
 * Copyright (C) 2024 The Android Open Source Project
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

#pragma once

#include <assert.h>

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <aidl/android/hardware/contexthub/BnContextHub.h>
#include <aidl/android/hardware/contexthub/BnEndpointCommunication.h>
#include <chre_host/generated/host_messages_generated.h>
#include <flatbuffers/flatbuffers.h>

#include "message_hub_manager.h"
#include "region_allocator.h"
#include "wakelock_manager.h"

namespace android::hardware::contexthub::common::implementation {

using ::aidl::android::hardware::contexthub::BnEndpointCommunication;
using ::aidl::android::hardware::contexthub::DataFlowId;
using ::aidl::android::hardware::contexthub::DataFlowInfo;
using ::aidl::android::hardware::contexthub::DataFlowSinkRegistrationParams;
using ::aidl::android::hardware::contexthub::EndpointId;
using ::aidl::android::hardware::contexthub::EndpointInfo;
using ::aidl::android::hardware::contexthub::HubInfo;
using ::aidl::android::hardware::contexthub::IEndpointCallback;
using ::aidl::android::hardware::contexthub::IEndpointCommunication;
using ::aidl::android::hardware::contexthub::Message;
using ::aidl::android::hardware::contexthub::MessageDeliveryStatus;
using ::aidl::android::hardware::contexthub::Reason;
using ::aidl::android::hardware::contexthub::SharedDataRegion;
using ::aidl::android::hardware::contexthub::SharedDataRegionRequirements;
using ::ndk::ScopedAStatus;

/**
 * Common parts of the IContextHub V4+ interface which can be shared by
 * various HAL implementations.
 */
class ContextHubV4Impl {
 public:
  using SendMessageFn =
      std::function<bool(const flatbuffers::FlatBufferBuilder &builder)>;
  ContextHubV4Impl(SendMessageFn sendMessageFn,
                   std::shared_ptr<RegionAllocator> regionAllocator,
                   std::unique_ptr<WakelockManager> wakelockManager)
      : mManager(std::bind(&ContextHubV4Impl::unlinkDeadHostHub, this,
                           std::placeholders::_1)),
        mSendMessageFn(std::move(sendMessageFn)),
        mRegionAllocator(regionAllocator),
        mWakelockManager(std::move(wakelockManager)) {}
  explicit ContextHubV4Impl(SendMessageFn sendMessageFn)
      : ContextHubV4Impl(std::move(sendMessageFn), /*regionAllocator=*/{},
                         /*wakelockManager=*/{}) {}
  ~ContextHubV4Impl() = default;

  /**
   * Initializes the implementation.
   *
   * This should be called once a connection with CHRE has been established.
   * Requests a dump of embedded hubs and endpoints from CHRE. Initializes the
   * CHRE-side host hub proxies.
   */
  void init();

  /**
   * Closes all existing sessions and embedded endpoints.
   */
  void onChreDisconnected();

  /**
   * Sends host state to CHRE.
   *
   * This should be called once the connection with CHRE has been restored.
   */
  void onChreRestarted();

  // IContextHub (V4+) API implementation.
  ScopedAStatus getHubs(std::vector<HubInfo> *hubs);
  ScopedAStatus getEndpoints(std::vector<EndpointInfo> *endpoints);
  ScopedAStatus registerEndpointHub(
      const std::shared_ptr<IEndpointCallback> &callback,
      const HubInfo &hubInfo,
      std::shared_ptr<IEndpointCommunication> *hubInterface);

  // TODO(b/385474431): Add dump().

  /**
   * Handles a CHRE message that is part of the V4 implementation.
   *
   * @param message Validated union of the various message types.
   * @return true if the message could be handled
   */
  bool handleMessageFromChre(const ::chre::fbs::ChreMessageUnion &message);

 private:
  friend class HostHubInterface;

  // Callbacks for each message type from CHRE.
  void onGetMessageHubsAndEndpointsResponse(
      const ::chre::fbs::GetMessageHubsAndEndpointsResponseT &msg);
  void onRegisterMessageHub(const ::chre::fbs::RegisterMessageHubT &msg);
  void onUnregisterMessageHub(const ::chre::fbs::UnregisterMessageHubT &msg);
  void onRegisterEndpoint(const ::chre::fbs::RegisterEndpointT &msg);
  void onAddServiceToEndpoint(const ::chre::fbs::AddServiceToEndpointT &msg);
  void onEndpointReady(const ::chre::fbs::EndpointReadyT &msg);
  void onUnregisterEndpoint(const ::chre::fbs::UnregisterEndpointT &msg);
  void onOpenEndpointSessionRequest(
      const ::chre::fbs::OpenEndpointSessionRequestT &msg);
  void onEndpointSessionOpened(const ::chre::fbs::EndpointSessionOpenedT &msg);
  void onEndpointSessionClosed(const ::chre::fbs::EndpointSessionClosedT &msg);
  void onEndpointSessionMessage(
      const ::chre::fbs::EndpointSessionMessageT &msg);
  void onEndpointSessionMessageDeliveryStatus(
      const ::chre::fbs::EndpointSessionMessageDeliveryStatusT &msg);

  // Callback invoked when a HAL client associated with a host hub goes down.
  void unlinkDeadHostHub(std::function<pw::Result<int64_t>()> unlinkFn);

  // Log error and close a session.
  void handleSessionFailure(
      const std::shared_ptr<MessageHubManager::HostHub> &hub, uint16_t session,
      pw::Status status);

  MessageHubManager mManager;
  SendMessageFn mSendMessageFn;
  std::shared_ptr<RegionAllocator> mRegionAllocator;
  std::unique_ptr<WakelockManager> mWakelockManager;

  // This lock is required to be held around any operation which modifies the
  // sets of host hubs or endpoints known by mManager and then sends an update
  // message to CHRE. This ensures that init()/onChreRestarted() are atomic
  // w.r.t. registerEndpointHub(), unregister(), registerEndpoint() and
  // unregisterEndpoint(). As init() resets CHRE-side host hub state,
  // interleaving these operations could e.g. leave an existing host hub
  // inaccessible from CHRE.
  std::mutex mHostHubOpLock;
};

/**
 * Wrapper for a MessageHubManager::HostHub instance implementing
 * IEndpointCommunication so that a client can directly make calls on its
 * associated HostHub.
 */
class HostHubInterface : public BnEndpointCommunication {
 public:
  explicit HostHubInterface(std::shared_ptr<MessageHubManager::HostHub> hub,
                            ContextHubV4Impl &hal)
      : mHub(std::move(hub)), mHal(hal) {
    assert(mHub != nullptr);
  }
  ~HostHubInterface() = default;

  // Implementation of IEndpointCommunication.
  ScopedAStatus registerEndpoint(const EndpointInfo &endpoint) override;
  ScopedAStatus unregisterEndpoint(const EndpointInfo &endpoint) override;
  ScopedAStatus requestSessionIdRange(int32_t size,
                                      std::array<int32_t, 2> *ids);
  ScopedAStatus openEndpointSession(
      int32_t sessionId, const EndpointId &destination,
      const EndpointId &initiator,
      const std::optional<std::string> &serviceDescriptor) override;
  ScopedAStatus sendMessageToEndpoint(int32_t sessionId,
                                      const Message &msg) override;
  ScopedAStatus sendMessageDeliveryStatusToEndpoint(
      int32_t sessionId, const MessageDeliveryStatus &msgStatus) override;
  ScopedAStatus closeEndpointSession(int32_t sessionId, Reason reason) override;
  ScopedAStatus endpointSessionOpenComplete(int32_t sessionId) override;
  ScopedAStatus unregister() override;
  ScopedAStatus allocateSharedDataRegion(
      const SharedDataRegionRequirements &requirements,
      SharedDataRegion *region) override;
  ScopedAStatus freeSharedDataRegion(int32_t id) override;
  ScopedAStatus registerDataFlowHostSource(const EndpointId &endpoint,
                                           const DataFlowInfo &info,
                                           int32_t *id) override;
  ScopedAStatus unregisterDataFlowHostSource(int32_t id) override;
  ScopedAStatus registerDataFlowOffloadSink(
      const DataFlowSinkRegistrationParams &params,
      const std::shared_ptr<
          IEndpointCommunication::IRegisterOffloadSinkCallback> &callback)
      override;
  ScopedAStatus unregisterDataFlowHostSink(
      const EndpointId &sinkId, const DataFlowId &dataFlowId) override;

 private:
  std::shared_ptr<MessageHubManager::HostHub> mHub;
  ContextHubV4Impl &mHal;
};

}  // namespace android::hardware::contexthub::common::implementation
