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

// TODO: b/376532038 - Refactor the platform layer to provide static nanoapps
// instead of having it conditionally come out of core/static_nanoapps.cc to
// ensure the build graph can properly represent chre.core.
#if defined(CHRE_INCLUDE_DEFAULT_STATIC_NANOAPPS) && \
    !defined(CHRE_USING_PURE_MAKEFILE)
// This cannot be supported due to how the build rules are set up. Ideally this
// would be part of a shared platform layer, but it's in fact part of core.
#error "CMake does not permit the built in default static nanoapps"
#endif  // defined(CHRE_INCLUDE_DEFAULT_STATIC_NANOAPPS)

// This should provide all CHRE_* configuration defines.
#include "chre/target_variant/config.h"

// The maximum number of LE COC sockets supported by the platform.
#ifndef CHRE_BLE_LE_COC_MAX_SOCKETS
#define CHRE_BLE_LE_COC_MAX_SOCKETS 2
#endif  // CHRE_BLE_LE_COC_MAX_SOCKETS

// The maximum number of RFCOMM sockets supported by the platform.
#ifndef CHRE_BT_RFCOMM_MAX_SOCKETS
#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
#define CHRE_BT_RFCOMM_MAX_SOCKETS 2
#else
#define CHRE_BT_RFCOMM_MAX_SOCKETS 0
#endif  // CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
#endif  // CHRE_BT_RFCOMM_MAX_SOCKETS

// TODO(b/430672746): The metadata needed for multibufs (10 for the RFCOMM channel tx queue + 5
// for the L2capChannel tx queue) based on the hard coded tx queue sizes for a
// pigweed L2capChannel and RFCOMM channel. When the queue size becomes
// configurable (or multibuf metadata size is reduced), consider making this
// value smaller.
#ifndef CHRE_BLE_SOCKET_TX_MULTIBUF_METADATA_SIZE
#ifdef CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
#define CHRE_BLE_SOCKET_TX_MULTIBUF_METADATA_SIZE (15 * 256)
#else
#define CHRE_BLE_SOCKET_TX_MULTIBUF_METADATA_SIZE (5 * 256)
#endif  // CHRE_BT_RFCOMM_SOCKET_SUPPORT_ENABLED
#endif  // CHRE_BLE_SOCKET_TX_MULTIBUF_METADATA_SIZE

// CHRE optionally supports the use of multiple eventloops, etc.
#ifndef CHRE_MULTI_THREADING_ENABLED
#define CHRE_MULTI_THREADING_ENABLED 0
#endif  // CHRE_MULTI_THREADING_ENABLED

// Temporary flag to disable AtomicUint8 until all platforms implement it
#ifndef CHRE_ATOMIC_UINT8_ENABLED
#define CHRE_ATOMIC_UINT8_ENABLED 0
#endif  // CHRE_ATOMIC_UINT8_ENABLED

// Temporary flag to disable AtomicUint32Ref until all platforms implement it
#ifndef CHRE_ATOMIC_UINT32_REF_ENABLED
#define CHRE_ATOMIC_UINT32_REF_ENABLED 0
#endif  // CHRE_ATOMIC_UINT32_REF_ENABLED

// The size of EventLoop::mCurrentFreeingEventStack
#ifndef CHRE_MAX_FREEING_EVENT_STACK_SIZE
#define CHRE_MAX_FREEING_EVENT_STACK_SIZE 4
#endif  // CHRE_MAX_FREEING_EVENT_STACK_SIZE

// Temporary flag to disable PlatformNanoapp::openNanoapp as a platform
// interface until all platforms implement it
#ifndef CHRE_PLATFORM_OPEN_NANOAPP_ENABLED
#define CHRE_PLATFORM_OPEN_NANOAPP_ENABLED 0
#endif  // CHRE_PLATFORM_OPEN_NANOAPP_ENABLED
