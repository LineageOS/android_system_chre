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

// IWYU pragma: private, include "chre_api/chre.h"
// IWYU pragma: friend chre/.*\.h

#ifndef _CHRE_DATA_H_
#define _CHRE_DATA_H_

/**
 * @file
 * This file defines the API for CHRE data flows, which are designed for
 * efficient high-throughput data transmission between a single producer
 * and multiple consumers, which may include nanoapps and other endpoints. These
 * data flows enable the transfer of large amounts of data with minimal data
 * copies, leveraging shared memory regions. They provide a mechanism for
 * nanoapps to exchange data streams, supporting various notification and
 * overwrite policies to suit different batching use cases.
 *
 * Here is an example of a producer nanoapp that creates a data flow and adds a
 * consumer nanoapp:
 *
 * - Producer nanoapp:
 *  - Creates a region, receives async region created event
 *  - Creates a data flow synchronously
 *  - Creates a consumer and the consumer handle is sent to the consumer
 *
 * - Consumer nanoapp:
 *  - Receives the CHRE_EVENT_DATA_CONSUMER_CREATED event
 *  - Calls chreDataFlowConsumerEnable() to enable the consumer
 *
 * - Producer nanoapp:
 *  - Inserts data into the data flow
 *
 * - Consumer nanoapp:
 *  - Receives the CHRE_EVENT_DATA_NOTIFICATION event, indicating data is
 *    available (if never notify was NOT selected)
 *  - Alternatively may at any time use the consumer API to poll the data flow
 *  - Processes the data and releases the elements
 *
 * - Either nanoapp:
 *  - Can disable the consumer
 *  - Both nanoaps receive a CHRE_EVENT_DATA_NOTIFICATION event, indicating
 *    a state change in the underlying data flow (consumer is gone)
 *
 * - Producer nanoapp:
 *  - Destroys the data flow explicitly or on unload
 *  - Destroys the region explicitly or on unload
 *
 * - Producer nanoapp:
 *  - If it crashes, the consumer nanoapp will receive a
 *    CHRE_EVENT_DATA_NOTIFICATION event, indicating a state change in the
 *    underlying data flow (producer is gone).
 *
 * @since v1.12
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <chre/common.h>
#include <chre/event.h>
#include <chre/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* _CHRE_DATA_H_ */
