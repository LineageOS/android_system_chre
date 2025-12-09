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

#ifndef CHRE_HOST_FBS_MESSAGE_SUMMARY_H_
#define CHRE_HOST_FBS_MESSAGE_SUMMARY_H_

#include <optional>
#include <string>
#include <vector>

#include "chre_host/generated/host_messages_generated.h"

namespace android::chre {

namespace fbs = ::chre::fbs;

/** Summary of key information of a fbs message for better logging purpose. */
class FbsMessageSummary {
 public:
  [[nodiscard]] std::string toString() const;

  FbsMessageSummary &setProcessingTime(int64_t timeMs) {
    mProcessingTimeMs = timeMs;
    return *this;
  }

  FbsMessageSummary &setError(const std::string &errString) {
    mError = errString;
    return *this;
  }

  static FbsMessageSummary fromRawMessage(const std::vector<uint8_t> &message) {
    return fromRawMessage(message.data(), message.size());
  }

  static FbsMessageSummary fromRawMessage(const uint8_t *data,
                                          const size_t &size);

 private:
  fbs::ChreMessage mType = fbs::ChreMessage::NONE;
  size_t mSize = 0;
  int mClientId = 0;
  // The host endpoint ID, if it's a nanoapp message.
  std::optional<uint16_t> mEndpointId;
  // The wall clock time when the summary was created.
  std::optional<std::string> mRecordTime;
  // The time it took to process the message, in milliseconds.
  std::optional<int64_t> mProcessingTimeMs;
  // A string describing any error encountered while parsing or processing.
  std::optional<std::string> mError;
};
}  // namespace android::chre

#endif  // CHRE_HOST_FBS_MESSAGE_SUMMARY_H_