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

#pragma once

#include "bt_offload_interface.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace chre {

class MockBtOffload : public BtOffloadInterface {
 public:
  MOCK_METHOD(void, sendToHost, (pw::bluetooth::proxy::H4PacketWithHci &&),
              (override));
  MOCK_METHOD(void, sendToController, (pw::bluetooth::proxy::H4PacketWithH4 &&),
              (override));
};

}  // namespace chre
