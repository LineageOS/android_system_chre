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

#include "location/lbs/contexthub/test_suite/chre_fake_api/helpers/re_helpers.h"

#include <cstdint>
#include <memory>

#include "chre_api/chre.h"

namespace lbs {
namespace contexthub {
namespace helpers {
namespace {

class ChreReConstantTime : public ChreApiReFunctions {
 public:
  ChreReConstantTime() = default;

  ChreReConstantTime(uint64_t current_time, int64_t host_offset)
      : current_time_(current_time), host_offset_(host_offset) {}

  uint64_t GetTime() override {
    return current_time_;
  }

  int64_t GetEstimatedHostTimeOffset() override {
    return host_offset_;
  }

 private:
  uint64_t current_time_ = kDefaultCurrentTime;
  int64_t host_offset_ = kDefaultHostOffset;
};

class ChreReEmptyClass : public ChreApiReFunctions {
 public:
  ChreReEmptyClass() = default;
  uint64_t GetTime() override {
    return 1;
  }
  int64_t GetEstimatedHostTimeOffset() override {
    return 1;
  }
  uint64_t GetAppId() override {
    return 1;
  }
  uint32_t GetInstanceId() override {
    return 1;
  }
  uint32_t TimerSet(uint64_t /* duration */, const void * /* cookie */,
                    bool /* one_shot */) override {
    return CHRE_TIMER_INVALID;
  }
  bool TimerCancel(uint32_t /* timer_id */) override {
    return false;
  }
};

}  // namespace

void SetConstantTime(uint64_t current_time, int64_t host_offset) {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreReConstantTime>(current_time, host_offset));
}

void SetHostMessageSize(uint32_t host_message_size) {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()
      ->GetChreApiReFunctions()
      ->SetMessageToHostMaxSize(host_message_size);
}

void SetEmptyRe() {
  ::lbs::contexthub::FakeChreApiProvider::GetInstance()->SetChreApiFunctions(
      std::make_unique<ChreReEmptyClass>());
}

}  // namespace helpers
}  // namespace contexthub
}  // namespace lbs