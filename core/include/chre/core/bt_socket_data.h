/*
 * Copyright (C) 2026 The Android Open Source Project
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

#include <cstdint>

/**
 * Parameters for a direction of packet flow in an RFCOMM channel socket.
 */
struct RfcommChannelConfig {
  //! Channel identifier of the endpoint.
  uint16_t cid;

  //! Maximum transmission unit.
  uint16_t mtu;

  //! Maximum frame size.
  uint16_t maxFrameSize;

  //! Initial credits for sending or receiving K-frames in Credit Based Flow
  //! Control mode.
  uint8_t credits;
};

/**
 * Data for the offloaded RFCOMM channel socket.
 */
struct BtRfcommChannelSocketData {
  //! Unique identifier for this socket connection. This ID in the offload
  //! domain matches the ID used on the host side. It is valid only while the
  //! socket is connected.
  uint64_t socketId;

  //! The ID of the Hub endpoint for hardware offload data path.
  uint64_t endpointId;

  //! ACL connection handle for the socket.
  uint16_t connectionHandle;

  //! DLCI for the RFCOMM channel.
  uint8_t dlci;

  //! Whether this endpoint is the initiator of the RFCOMM multiplexer.
  uint8_t muxInitiator;

  RfcommChannelConfig rxConfig;

  RfcommChannelConfig txConfig;
};
