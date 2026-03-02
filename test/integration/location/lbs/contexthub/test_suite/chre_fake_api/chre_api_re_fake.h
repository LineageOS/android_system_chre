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

#ifndef LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_RE_FAKE_H_
#define LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_RE_FAKE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace lbs {
namespace contexthub {

// Class that contains methods used to redirect CHRE API function calls.
class ChreApiReFunctions {
 public:
  virtual ~ChreApiReFunctions() = default;

  // These APIs are defined at
  // chre_api/include/chre_api/chre/re.h
  [[noreturn]] virtual void Abort(uint32_t abortCode);
  virtual uint64_t GetTime();
  virtual uint32_t GetCapabilities();
  virtual uint32_t GetMessageToHostMaxSize();
  virtual int64_t GetEstimatedHostTimeOffset();
  virtual uint64_t GetAppId();
  virtual uint32_t GetInstanceId();
  virtual uint32_t TimerSet(uint64_t duration, const void *cookie,
                            bool one_shot);
  virtual bool TimerCancel(uint32_t timer_id);

  // These APIs are defined at
  // chre_api/include/chre_api/chre/version.h
  // But bundled with other "runtime environment" functions for simplicity
  virtual uint32_t GetApiVersion();
  virtual uint32_t GetVersion();
  virtual uint64_t GetPlatformId();

  // Functions used to override the behavior of individual APIs, allowing for
  // simple compositions.
  void SetMessageToHostMaxSize(uint32_t messageToHostMaxSize);

 private:
  std::optional<uint32_t> message_to_host_max_size_override_;
};

}  // namespace contexthub
}  // namespace lbs

#endif  // LOCATION_LBS_CONTEXTHUB_TEST_SUITE_CHRE_FAKE_API_CHRE_API_RE_FAKE_H_
