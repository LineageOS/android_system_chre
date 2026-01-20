/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <cinttypes>
#include <string>

namespace chre {

/**
 * Blocks until the debug dump is complete or a timeout occurs. This is only
 * used for testing the debug dump code in gtest.
 *
 * @param timeoutMs The maximum time to wait for the debug dump to complete in
 *        milliseconds.
 *
 * @return The debug dump string if complete within the timeout, otherwise an
 *         empty string.
 */
std::string getDebugDumpStringBlocking(uint32_t timeoutMs);

}  // namespace chre
