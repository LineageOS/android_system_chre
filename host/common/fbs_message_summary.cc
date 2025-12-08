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
#include <sstream>

#include "chre_host/fbs_message_summary.h"
#include "chre_host/generated/host_messages_generated.h"
#include "chre_host/host_protocol_host.h"
#include "chre_host/time_util.h"

namespace android::chre {

std::string FbsMessageSummary::toString() const {
  if (mSize == 0) {
    return "[EMPTY]";
  }
  std::ostringstream stream;
  if (mRecordTime) {
    stream << " [" << mRecordTime.value() << "]";
  }
  stream << " client=" << mClientId << " type=" << static_cast<uint16_t>(mType)
         << " size=" << mSize;
  if (mEndpointId) {
    stream << " endpoint=0x" << std::hex << mEndpointId.value() << std::dec;
  }
  if (mError) {
    stream << " error=" << mError.value();
  }
  if (mProcessingTimeMs) {
    stream << " processingTime=" << mProcessingTimeMs.value() << "ms";
  }
  return stream.str();
}

FbsMessageSummary FbsMessageSummary::fromRawMessage(const uint8_t *data,
                                                    const size_t &size) {
  if (size == 0 || data == nullptr) {
    return {};
  }
  FbsMessageSummary summary;
  summary.mSize = size;
  summary.mRecordTime = getWallclockTime();
  if (!HostProtocolHost::verifyMessage(data, size)) {
    summary.mError = "Invalid";
    return summary;
  }
  std::unique_ptr<fbs::MessageContainerT> container =
      fbs::UnPackMessageContainer(data);
  summary.mType = container->message.type;
  summary.mClientId = container->host_addr->client_id();
  if (summary.mType == fbs::ChreMessage::NanoappMessage) {
    summary.mEndpointId = container->message.AsNanoappMessage()->host_endpoint;
  }
  return summary;
}

}  // namespace android::chre