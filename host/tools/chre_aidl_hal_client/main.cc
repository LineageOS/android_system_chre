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

#include "command_handlers.h"

#include <android/binder_process.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "chre_host/hal_client.h"

using namespace android::chre::chre_aidl_hal_client;

void executeCommand(const std::vector<std::string> &cmdLine) {
  if (auto func = CommandHelper::parseCommand<DirectCommandFunction>(
          cmdLine, kAllDirectCommands)) {
    func(cmdLine);
  } else {
    CommandHelper::printUsage<DirectCommandFunction>(kAllDirectCommands);
  }
}

int main(int argc, char *argv[]) {
  using namespace android::chre::chre_aidl_hal_client;
  // Start binder thread pool to enable callbacks.
  ABinderProcess_startThreadPool();

  std::vector<std::string> cmdLine{};
  for (int i = 1; i < argc; i++) {
    cmdLine.emplace_back(argv[i]);
  }
  try {
    executeCommand(cmdLine);
  } catch (std::system_error &e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }
  return 0;
}