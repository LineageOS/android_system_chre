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

#ifndef CHRE_UTIL_SYSTEM_MESSAGE_HUB_CALLBACK_V2_H_
#define CHRE_UTIL_SYSTEM_MESSAGE_HUB_CALLBACK_V2_H_

#include "chre/util/system/message_common.h"
#include "chre/util/system/message_hub_callback.h"

namespace chre::message {

//! The V2 callback used to register a MessageHub with the MessageRouter.
//! This callback supports all existing functionality as well as data flow
//! APIs.
//!
//! @see chre::message::MessageHubCallback
class MessageHubCallbackV2 : public MessageHubCallback {
 public:
  virtual ~MessageHubCallbackV2() = default;

  //! Called when a data flow sink is registered.
  //! @param registration The data flow sink registration information.
  virtual void onRegisterDataFlowSink(
      DataFlowSinkRegistration &&registration) = 0;

  //! Called to report that a data flow sink has been unregistered.
  //! @param unregistration The data flow sink unregistration information. Only
  //! valid within the scope of this callback.
  virtual void onDataFlowSinkUnregistered(
      const DataFlowSinkUnregistration &unregistration) = 0;

  //! Called to report that a data flow has stopped.
  //! @param stopped The data flow stopped information. Only valid within the
  //! scope of this callback.
  virtual void onDataFlowStopped(const DataFlowStopped &stopped) = 0;

  //! Called to report a data flow alert.
  //! @param alert The data flow alert information. Only valid within the scope
  //! of this callback.
  virtual void onDataFlowAlert(const DataFlowAlert &alert) = 0;
};

}  // namespace chre::message

#endif  // CHRE_UTIL_SYSTEM_MESSAGE_HUB_CALLBACK_V2_H_
