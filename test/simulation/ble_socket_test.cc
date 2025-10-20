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

#include "test_base.h"

#include <array>
#include <vector>

#include "test_util.h"

#include "chre/core/ble_l2cap_coc_socket_data.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/linux/pal_ble.h"
#include "chre/util/macros.h"
#include "chre_api/chre.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace chre {

using ::testing::_;

namespace pbe = pw::bluetooth::emboss;

namespace {
// Populate event header and return writer of the template parameter type.
template <typename EmbossT>
pw::Result<EmbossT> buildEvent(pw::bluetooth::proxy::H4PacketWithHci &h4Packet,
                               pbe::EventCode event_code) {
  size_t parameter_total_size =
      h4Packet.GetHciSpan().size() - pbe::EventHeader::IntrinsicSizeInBytes();
  h4Packet.SetH4Type(pbe::H4PacketType::EVENT);

  PW_TRY_ASSIGN(EmbossT view, pw::bluetooth::MakeEmbossWriter<EmbossT>(
                                  h4Packet.GetHciSpan()));

  view.header().event_code().Write(event_code);
  view.header().parameter_total_size().Write(parameter_total_size);

  return view;
}

// Populate command response event header and return writer of the template
// parameter type.
template <typename EmbossT>
pw::Result<EmbossT> buildCommandResponseSuccessEvent(
    pw::bluetooth::proxy::H4PacketWithHci &h4Packet,
    pbe::EventCode event_code) {
  PW_TRY_ASSIGN(EmbossT view, buildEvent<EmbossT>(h4Packet, event_code));

  view.status().Write(pbe::StatusCode::SUCCESS);

  return view;
}

class BleSocketTest : public TestBase {
 public:
  void SetUp() override {
    TestBase::SetUp();
    resetSocketVariables();
  }

  // Send an LE_Read_Buffer_Size (V2) CommandComplete event to the ProxyHost
  // so it can request the reservation of a number of LE ACL send credits.
  pw::Status sendLeReadBufferResponseFromController(uint8_t numLeAclPackets) {
    using pbe::EventCode;
    using pbe::LEReadBufferSizeV2CommandCompleteEventWriter;

    std::array<uint8_t,
               LEReadBufferSizeV2CommandCompleteEventWriter::SizeInBytes()>
        hciArray{};
    hciArray.fill(0);
    pw::bluetooth::proxy::H4PacketWithHci h4Packet{pbe::H4PacketType::EVENT,
                                                   hciArray};

    PW_TRY_ASSIGN(LEReadBufferSizeV2CommandCompleteEventWriter view,
                  buildCommandResponseSuccessEvent<
                      LEReadBufferSizeV2CommandCompleteEventWriter>(
                      h4Packet, EventCode::COMMAND_COMPLETE));

    view.command_complete().command_opcode().Write(
        pbe::OpCode::LE_READ_BUFFER_SIZE_V2);
    view.total_num_le_acl_data_packets().Write(numLeAclPackets);
    view.le_acl_data_packet_length().Write(251);

    EXPECT_TRUE(view.Ok());

    mProxyHost.value().HandleH4HciFromController(
        {h4Packet.GetH4Type(), h4Packet.GetHciSpan()});
    return pw::OkStatus();
  }

  // Send a Number_of_Completed_Packets event to ProxyHost so it will allow
  // sending more ACL packets
  pw::Status sendNumberOfCompletedPackets(uint16_t connectionHandle,
                                          uint16_t numCompletedPackets) {
    const uint16_t kNumConnections = 1;

    const size_t kH4PacketSize =
        pbe::NumberOfCompletedPacketsEvent::MinSizeInBytes() +
        kNumConnections *
            pbe::NumberOfCompletedPacketsEventData::IntrinsicSizeInBytes();
    std::array<uint8_t, kH4PacketSize> hciArray;
    hciArray.fill(0);
    pw::bluetooth::proxy::H4PacketWithHci h4Packet{pbe::H4PacketType::EVENT,
                                                   hciArray};

    PW_TRY_ASSIGN(pbe::NumberOfCompletedPacketsEventWriter view,
                  buildEvent<pbe::NumberOfCompletedPacketsEventWriter>(
                      h4Packet, pbe::EventCode::NUMBER_OF_COMPLETED_PACKETS));

    view.num_handles().Write(kNumConnections);
    view.nocp_data()[0].connection_handle().Write(connectionHandle);
    view.nocp_data()[0].num_completed_packets().Write(numCompletedPackets);

    EXPECT_TRUE(view.Ok());

    mProxyHost.value().HandleH4HciFromController(
        {h4Packet.GetH4Type(), h4Packet.GetHciSpan()});
    return pw::OkStatus();
  }

 protected:
  // Size of sdu_length field in first K-frames.
  static constexpr uint8_t kSduLengthFieldSize = 2;

  static constexpr uint8_t kFirstKFrameOverAclMinSize =
      pbe::AclDataFrameHeader::IntrinsicSizeInBytes() +
      pbe::FirstKFrame::MinSizeInBytes();

  static constexpr uint8_t kRxMps = 200;

  BleL2capCocSocketData mSocketData = {
      .socketId = 1,
      .endpointId = kDefaultTestNanoappId,
      .connectionHandle = 2,
      .rxConfig =
          L2capCocConfig{.cid = 3, .mtu = 400, .mps = kRxMps, .credits = 0},
      .txConfig =
          L2capCocConfig{.cid = 4, .mtu = 400, .mps = 200, .credits = 2}};

  chreError mExpectedSocketConnectionError = CHRE_ERROR_NONE;

  uint8_t mDefaultMessage[6] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};
};

class BleSocketTestNanoapp : public TestNanoapp {
 public:
  BleSocketTestNanoapp()
      : TestNanoapp(
            TestNanoappInfo{.perms = NanoappPermissions::CHRE_PERMS_BLE}) {}

  bool start() override {
    chreUserSettingConfigureEvents(CHRE_USER_SETTING_BLE_AVAILABLE,
                                   true /* enable */);
    return true;
  }

  void end() override {
    chreUserSettingConfigureEvents(CHRE_USER_SETTING_BLE_AVAILABLE,
                                   false /* enable */);
  }
};

struct SocketSendData {
  void *data;
  uint16_t length;
  chreBleSocketPacketFreeFunction *freeCallback;
};

}  // namespace

TEST_F(BleSocketTest, BleSocketCapabilitesTest) {
  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketCapabilitiesRequestByHost();
  BtSocketCapabilities capabilities = getSocketCapabilities();
  EXPECT_EQ(capabilities.leCocNumberOfSupportedSockets, 3);
  EXPECT_EQ(capabilities.leCocMtu, 2048);
  EXPECT_EQ(capabilities.rfcommNumberOfSupportedSockets, 0);
  EXPECT_EQ(capabilities.rfcommMaxFrameSize, 0);
}

TEST_F(BleSocketTest, BleSocketAcceptConnectionTest) {
  CREATE_CHRE_TEST_EVENT(NO_OP_EVENT, 1);

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          break;
        }

        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case NO_OP_EVENT: {
              TestEventQueueSingleton::get()->pushEvent(NO_OP_EVENT);
              break;
            }
          }
        }
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  sendLeReadBufferResponseFromController(1);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  // This ensures that the socket open response has been sent.
  sendEventToNanoapp(appId, NO_OP_EVENT);
  waitForEvent(NO_OP_EVENT);

  EXPECT_TRUE(getSocketOpenSuccess());
}

TEST_F(BleSocketTest, BleSocketNanoappNotFoundTest) {
  CREATE_CHRE_TEST_EVENT(NO_OP_EVENT, 1);

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case NO_OP_EVENT: {
              TestEventQueueSingleton::get()->pushEvent(NO_OP_EVENT);
              break;
            }
          }
        }
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  sendLeReadBufferResponseFromController(1);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  constexpr uint64_t kInvalidEndpointId = 1;
  mSocketData.endpointId = kInvalidEndpointId;
  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);

  // This ensures that the socket open response has been sent.
  sendEventToNanoapp(appId, NO_OP_EVENT);
  waitForEvent(NO_OP_EVENT);

  EXPECT_FALSE(getSocketOpenSuccess());
  EXPECT_STREQ(getSocketOpenFailureReason(), "failed to find nanoapp");
}

TEST_F(BleSocketTest, BleSocketDoNotAcceptConnectionTest) {
  CREATE_CHRE_TEST_EVENT(NO_OP_EVENT, 1);

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          // Do not accept the socket.
          break;
        }
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case NO_OP_EVENT: {
              TestEventQueueSingleton::get()->pushEvent(NO_OP_EVENT);
              break;
            }
          }
        }
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  sendLeReadBufferResponseFromController(1);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  // This ensures that the socket open response has been sent.
  sendEventToNanoapp(appId, NO_OP_EVENT);
  waitForEvent(NO_OP_EVENT);

  EXPECT_FALSE(getSocketOpenSuccess());
  EXPECT_STREQ(getSocketOpenFailureReason(), "nanoapp did not accept socket");
}

TEST_F(BleSocketTest, BleSocketBasicSendTest) {
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND, 0);
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND_FREE_CALLBACK, 1);

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          mSocketId = event->socketId;
          break;
        }
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SOCKET_SEND: {
              auto data = static_cast<SocketSendData *>(event->data);
              int32_t status = chreBleSocketSend(
                  mSocketId, data->data, data->length, data->freeCallback);
              TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND, status);
              break;
            }
          }
        }
      }
    }

   private:
    uint64_t mSocketId = 0;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  sendLeReadBufferResponseFromController(2);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  // Expect chreBleSocketSend to result in sending a packet to the BT Controller
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  SocketSendData data = {
      .data = mDefaultMessage,
      .length = 6,
      .freeCallback = [](void *, uint16_t) {
        TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND_FREE_CALLBACK);
      }};
  sendEventToNanoapp(appId, SOCKET_SEND, data);
  int32_t status = 0;
  waitForEvent(SOCKET_SEND, &status);
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS);
  // Even though the multibuf is destroyed immediately in this case, the free
  // callback is handled on the event loop thread and will occur after the
  // SOCKET_SEND event
  waitForEvent(SOCKET_SEND_FREE_CALLBACK);
}

TEST_F(BleSocketTest, BleSocketSendNoSocketFoundTest) {
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND, 0);
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND_FREE_CALLBACK, 1);

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SOCKET_SEND: {
              auto data = static_cast<SocketSendData *>(event->data);
              int32_t status = chreBleSocketSend(1, data->data, data->length,
                                                 data->freeCallback);
              TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND, status);
              break;
            }
          }
        }
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  SocketSendData data = {
      .data = mDefaultMessage,
      .length = 6,
      .freeCallback = [](void *, uint16_t) {
        TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND_FREE_CALLBACK);
      }};
  sendEventToNanoapp(appId, SOCKET_SEND, data);
  // Free callback is invoked synchronously on socket send failure
  waitForEvent(SOCKET_SEND_FREE_CALLBACK);
  int32_t status = 0;
  waitForEvent(SOCKET_SEND, &status);
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_FAILURE);
}

TEST_F(BleSocketTest, BleSocketSendQueueFullTest) {
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND, 0);
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND_FREE_CALLBACK, 1);
  CREATE_CHRE_TEST_EVENT(SOCKET_RETRY_SEND, 2);

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          mSocketId = event->socketId;
          break;
        }
        case CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE: {
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE);
          int32_t status =
              chreBleSocketSend(mSocketId, mSendData.data, mSendData.length,
                                mSendData.freeCallback);
          TestEventQueueSingleton::get()->pushEvent(SOCKET_RETRY_SEND, status);
          break;
        }
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SOCKET_SEND: {
              auto data = static_cast<SocketSendData *>(event->data);
              mSendData = *data;
              int32_t status = chreBleSocketSend(
                  mSocketId, data->data, data->length, data->freeCallback);
              TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND, status);
              break;
            }
          }
        }
      }
    }

   private:
    uint64_t mSocketId = 0;

    SocketSendData mSendData;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  sendLeReadBufferResponseFromController(1);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  SocketSendData data = {
      .data = mDefaultMessage,
      .length = 6,
      .freeCallback = [](void *, uint16_t) {
        TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND_FREE_CALLBACK);
      }};
  int32_t status = 0;

  // TODO(b/430672746): 5 is the hard coded queue size of an L2CAP channel.
  // Revisit this number when https://pwbug.dev/349700888 has been addressed.
  for (size_t i = 0; i < 5; i++) {
    sendEventToNanoapp(appId, SOCKET_SEND, data);
    waitForEvent(SOCKET_SEND, &status);
    EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS);
  }
  // The 6th socket send request should result in the queue full status
  sendEventToNanoapp(appId, SOCKET_SEND, data);
  waitForEvent(SOCKET_SEND, &status);
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL);

  // Send an NOCP event to the ProxyHost to restore its ACL credit. This results
  // in the L2capCoc sending the first queued packet to the BT Controller.
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);
  sendNumberOfCompletedPackets(mSocketData.connectionHandle, 1);

  // First packet in queue is sent and its freeCallback is invoked.
  waitForEvent(SOCKET_SEND_FREE_CALLBACK);
  // Callback notifying CHRE that second callback is available
  waitForEvent(CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE);
  // Nanoapp successfully re-sends packet
  waitForEvent(SOCKET_RETRY_SEND, &status);
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS);
}

TEST_F(BleSocketTest, BleSocketBasicReceiveTest) {
  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          mSocketId = event->socketId;
          break;
        }
        case CHRE_EVENT_BLE_SOCKET_PACKET: {
          auto event = static_cast<const chreBleSocketPacketEvent *>(eventData);
          mSocketReceiveData.assign(event->data, event->data + event->length);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_PACKET, &mSocketReceiveData);
        }
      }
    }

   private:
    uint64_t mSocketId = 0;

    std::vector<uint8_t> mSocketReceiveData;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());
  UNUSED_VAR(appId);

  sendLeReadBufferResponseFromController(1);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  std::array<uint8_t, 3> expectedPayload = {0xAB, 0xCD, 0xEF};

  std::array<uint8_t, kFirstKFrameOverAclMinSize + expectedPayload.size()>
      hciArray;
  hciArray.fill(0);
  pw::bluetooth::proxy::H4PacketWithHci h4Packet{pbe::H4PacketType::ACL_DATA,
                                                 hciArray};

  pw::Result<pbe::AclDataFrameWriter> acl =
      pw::bluetooth::MakeEmbossWriter<pbe::AclDataFrameWriter>(hciArray);
  acl->header().handle().Write(mSocketData.connectionHandle);
  acl->data_total_length().Write(pbe::FirstKFrame::MinSizeInBytes() +
                                 expectedPayload.size());

  pbe::FirstKFrameWriter kframe = pbe::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize + expectedPayload.size());
  kframe.channel_id().Write(mSocketData.rxConfig.cid);
  kframe.sdu_length().Write(expectedPayload.size());
  std::copy(expectedPayload.begin(), expectedPayload.end(),
            hciArray.begin() + kFirstKFrameOverAclMinSize);

  mProxyHost->HandleH4HciFromController(std::move(h4Packet));

  std::vector<uint8_t> *receiveData;
  waitForEvent(CHRE_EVENT_BLE_SOCKET_PACKET, &receiveData);
  EXPECT_TRUE(std::equal(receiveData->begin(), receiveData->end(),
                         expectedPayload.begin(), expectedPayload.end()));
}

TEST_F(BleSocketTest, BleSocketInvalidRxTest) {
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND, 0);
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND_FREE_CALLBACK, 1);

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          mSocketId = event->socketId;
          break;
        }
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SOCKET_SEND: {
              auto data = static_cast<SocketSendData *>(event->data);
              int32_t status = chreBleSocketSend(
                  mSocketId, data->data, data->length, data->freeCallback);
              TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND, status);
              break;
            }
          }
        }
      }
    }

   private:
    uint64_t mSocketId = 0;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  sendLeReadBufferResponseFromController(1);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  // Specify PDU length larger than Rx config to trigger kRxInvalid event
  constexpr uint8_t kInvalidPduSize = kRxMps + 1;

  std::array<uint8_t, kFirstKFrameOverAclMinSize + kInvalidPduSize> hciArray;
  hciArray.fill(0);
  pw::bluetooth::proxy::H4PacketWithHci h4Packet{pbe::H4PacketType::ACL_DATA,
                                                 hciArray};

  pw::Result<pbe::AclDataFrameWriter> acl =
      pw::bluetooth::MakeEmbossWriter<pbe::AclDataFrameWriter>(hciArray);
  acl->header().handle().Write(mSocketData.connectionHandle);
  acl->data_total_length().Write(pbe::FirstKFrame::MinSizeInBytes() +
                                 kInvalidPduSize);

  pbe::FirstKFrameWriter kframe = pbe::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize + kInvalidPduSize);
  kframe.channel_id().Write(mSocketData.rxConfig.cid);
  kframe.sdu_length().Write(kInvalidPduSize);

  mProxyHost->HandleH4HciFromController(std::move(h4Packet));

  SocketSendData data = {
      .data = mDefaultMessage,
      .length = 6,
      .freeCallback = [](void *, uint16_t) {
        TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND_FREE_CALLBACK);
      }};
  sendEventToNanoapp(appId, SOCKET_SEND, data);
  int32_t status = 0;
  waitForEvent(SOCKET_SEND, &status);
  // Failure due to ProxyHost stopping channel
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_FAILURE);
  // Free callback is invoked asynchronously because MultiBuf has been created
  waitForEvent(SOCKET_SEND_FREE_CALLBACK);
  // CHRE notifies Host for this type of failure
  EXPECT_EQ(getSocketClosureCount(), 1);
}

TEST_F(BleSocketTest, BleSocketBtResetTest) {
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND, 0);
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND_FREE_CALLBACK, 1);

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          mSocketId = event->socketId;
          break;
        }
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SOCKET_SEND: {
              auto data = static_cast<SocketSendData *>(event->data);
              int32_t status = chreBleSocketSend(
                  mSocketId, data->data, data->length, data->freeCallback);
              TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND, status);
              break;
            }
          }
        }
      }
    }

   private:
    uint64_t mSocketId = 0;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  sendLeReadBufferResponseFromController(1);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  mProxyHost->Reset();

  SocketSendData data = {
      .data = mDefaultMessage,
      .length = 6,
      .freeCallback = [](void *, uint16_t) {
        TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND_FREE_CALLBACK);
      }};
  sendEventToNanoapp(appId, SOCKET_SEND, data);
  int32_t status = 0;
  waitForEvent(SOCKET_SEND, &status);
  // Failure due to ProxyHost stopping channel
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_FAILURE);
  // Free callback is invoked asynchronously because MultiBuf has been created
  waitForEvent(SOCKET_SEND_FREE_CALLBACK);
  // Host not notified for this type of failure because it is notified by the
  // Android Bluetooth Stack
  EXPECT_EQ(getSocketClosureCount(), 0);
}

TEST_F(BleSocketTest, BleSocketClosedAfterUnloadTest) {
  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          break;
        }
      }
    }
  };
  uint64_t appId = loadNanoapp(MakeUnique<App>());

  sendLeReadBufferResponseFromController(1);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  unloadNanoapp(appId);
  EXPECT_EQ(getSocketClosureCount(), 1);
}

TEST_F(BleSocketTest, BleSocketClosedAfterHostMessageTest) {
  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          break;
        }
        case CHRE_EVENT_BLE_SOCKET_DISCONNECTION:
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_DISCONNECTION);
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());
  UNUSED_VAR(appId);

  sendLeReadBufferResponseFromController(1);
  // Expect the L2capCoc to send the L2capFlowControlCreditInd after the socket
  // is opened
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketOpenedByHost(mSocketData);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  EventLoopManagerSingleton::get()
      ->getBleSocketManager()
      .handleSocketClosedByHost(mSocketData.socketId);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_DISCONNECTION);
  // Host not notified because it triggered the closure
  EXPECT_EQ(getSocketClosureCount(), 0);
}

}  // namespace chre
