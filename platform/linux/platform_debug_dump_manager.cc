/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "chre/platform/platform_debug_dump_manager.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>

namespace chre {

namespace {
std::string gDebugDumpBuffer;
std::mutex gDebugDumpMutex;
std::condition_variable gDebugDumpCv;
bool gDebugDumpComplete = false;
}  // anonymous namespace

std::string getDebugDumpStringBlocking(uint32_t timeoutMs) {
  LOGD("getDebugDumpStringBlocking: timeoutMs: %d", timeoutMs);
  std::string dump;
  std::unique_lock<std::mutex> lock(gDebugDumpMutex);
  if (gDebugDumpCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                            [] { return gDebugDumpComplete; })) {
    dump = gDebugDumpBuffer;
  }
  return dump;
}

PlatformDebugDumpManagerBase::PlatformDebugDumpManagerBase() {}

PlatformDebugDumpManagerBase::~PlatformDebugDumpManagerBase() {}

void PlatformDebugDumpManager::sendDebugDump(const char *debugStr,
                                             bool complete) {
  LOGD("sendDebugDump: complete: %d current debug size %zu", complete,
       gDebugDumpBuffer.size());
  std::lock_guard<std::mutex> lock(gDebugDumpMutex);
  if (gDebugDumpComplete) {
    gDebugDumpBuffer.clear();
    gDebugDumpComplete = false;
  }
  gDebugDumpBuffer += debugStr;
  if (complete) {
    gDebugDumpComplete = true;
    gDebugDumpCv.notify_one();
  }
}

void PlatformDebugDumpManager::logStateToBuffer(
    DebugDumpWrapper & /* debugDump */) {}

}  // namespace chre
