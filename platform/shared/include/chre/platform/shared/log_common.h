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

#ifndef CHRE_PLATFORM_SHARED_LOG_COMMON_H_
#define CHRE_PLATFORM_SHARED_LOG_COMMON_H_

#include "chre_api/chre/re.h"

#ifdef CHRE_TOKENIZED_LOGGING_ENABLED
#include "pw_tokenizer/tokenize.h"
#endif  // CHRE_TOKENIZED_LOGGING_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Log via the CHRE LogBufferManagerSingleton vaLog method.
 *
 * @param level The log level.
 * @param format The format string.
 * @param ... The arguments to print into the final log.
 */
void chrePlatformLogToBuffer(enum chreLogLevel level, const char *format, ...);

/**
 * Store a log as pure bytes. The message may be an encoded or tokenized
 * log. The decoding pattern for this message is up to the receiver.
 *
 * @param level Logging level.
 * @param msg a byte buffer containing the encoded log message.
 * @param msgSize size of the encoded log message buffer.
 */
void chrePlatformEncodedLogToBuffer(enum chreLogLevel level, const uint8_t *msg,
                                    size_t msgSize);

#ifdef CHRE_TOKENIZED_LOGGING_ENABLED
/**
 * Handles encoding and processing of a tokenized log message.
 *
 * @param level Logging level.
 * @param token Encoded tokenized message.
 * @param types Specifies the argument types.
 * @param ... The arguments to print into the final log.
 */
void EncodeTokenizedMessage(uint32_t level, pw_tokenizer_Token token,
                            pw_tokenizer_ArgTypes types, ...);
#endif  // CHRE_TOKENIZED_LOGGING_ENABLED

#ifdef __cplusplus
}
#endif

#endif  // CHRE_PLATFORM_SHARED_LOG_COMMON_H_
