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

#include "commands.h"
#include "context_hub_callback.h"
#include "nanoapp_helper.h"
#include "utils.h"

#include <aidl/android/hardware/contexthub/BnContextHubCallback.h>
#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <aidl/android/hardware/contexthub/NanoappBinary.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <cctype>
#include <filesystem>
#include <future>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "chre_host/file_stream.h"
#include "chre_host/hal_client.h"
#include "chre_host/log.h"
#include "chre_host/napp_header.h"

namespace android::chre::chre_aidl_hal_client {
namespace {
using chre::HalClient;
using chre::NanoAppBinaryHeader;
using chre::readFileContents;

using aidl::android::hardware::contexthub::AsyncEventType;
using aidl::android::hardware::contexthub::BnContextHubCallback;
using aidl::android::hardware::contexthub::ContextHubInfo;
using aidl::android::hardware::contexthub::ContextHubMessage;
using aidl::android::hardware::contexthub::HostEndpointInfo;
using aidl::android::hardware::contexthub::IContextHub;
using aidl::android::hardware::contexthub::MessageDeliveryStatus;
using aidl::android::hardware::contexthub::NanoappBinary;
using aidl::android::hardware::contexthub::NanoappInfo;
using aidl::android::hardware::contexthub::NanSessionRequest;
using aidl::android::hardware::contexthub::Setting;
using internal::ToString;
using ndk::ScopedAStatus;

std::shared_ptr<IContextHub> gContextHub = nullptr;
std::shared_ptr<ContextHubCallback> gCallback = nullptr;

void registerHostCallback() {
  if (gCallback != nullptr) {
    gCallback.reset();
  }
  gCallback = ContextHubCallback::make<ContextHubCallback>();
  if (!gContextHub->registerCallback(kContextHubId, gCallback).isOk()) {
    throwError("Failed to register the callback");
  }
}

/** Initializes gContextHub and register gCallback. */
std::shared_ptr<IContextHub> getContextHub() {
  if (gContextHub == nullptr) {
    const auto aidlServiceName =
        std::string() + IContextHub::descriptor + "/default";
    ndk::SpAIBinder binder(
        AServiceManager_waitForService(aidlServiceName.c_str()));
    if (binder.get() == nullptr) {
      throwError("Could not find Context Hub HAL");
    }
    gContextHub = IContextHub::fromBinder(binder);
  }
  if (gCallback == nullptr) {
    registerHostCallback();
  }
  return gContextHub;
}

void verifyStatus(const std::string &operation, const ScopedAStatus &status) {
  if (!status.isOk()) {
    gCallback->resetPromise();
    throwError(operation + " fails with abnormal status " +
               ToString(status.getMessage()) + " error code " +
               ToString(status.getServiceSpecificError()));
  }
}

void verifyStatusAndSignal(const std::string &operation,
                           const ScopedAStatus &status,
                           const std::future<void> &future_signal) {
  verifyStatus(operation, status);
  std::future_status future_status =
      future_signal.wait_for(kTimeOutThresholdInSec);
  if (future_status != std::future_status::ready) {
    gCallback->resetPromise();
    throwError(operation + " doesn't finish within " +
               ToString(kTimeOutThresholdInSec.count()) + " seconds");
  }
}

void getAllContextHubs() {
  std::vector<ContextHubInfo> hubs{};
  getContextHub()->getContextHubs(&hubs);
  if (hubs.empty()) {
    std::cerr << "Failed to get any context hub." << std::endl;
    return;
  }
  for (const auto &hub : hubs) {
    std::cout << "Context Hub " << hub.id << ": " << std::endl
              << "  Name: " << hub.name << std::endl
              << "  Vendor: " << hub.vendor << std::endl
              << "  Max support message length (bytes): "
              << hub.maxSupportedMessageLengthBytes << std::endl
              << "  Version: " << static_cast<uint32_t>(hub.chreApiMajorVersion)
              << "." << static_cast<uint32_t>(hub.chreApiMinorVersion)
              << std::endl
              << "  Chre platform id: 0x" << std::hex << hub.chrePlatformId
              << std::endl;
  }
}

void loadNanoapp(std::string &pathAndName) {
  std::unique_ptr<NanoAppBinaryHeader> header =
      NanoappHelper::findHeaderAndNormalizePath(pathAndName);
  std::vector<uint8_t> soBuffer{};
  if (!readFileContents(pathAndName.c_str(), soBuffer)) {
    throwError("Failed to open the content of " + pathAndName);
  }
  NanoappBinary binary;
  binary.nanoappId = static_cast<int64_t>(header->appId);
  binary.customBinary = soBuffer;
  binary.flags = static_cast<int32_t>(header->flags);
  binary.targetChreApiMajorVersion =
      static_cast<int8_t>(header->targetChreApiMajorVersion);
  binary.targetChreApiMinorVersion =
      static_cast<int8_t>(header->targetChreApiMinorVersion);
  binary.nanoappVersion = static_cast<int32_t>(header->appVersion);

  auto status =
      getContextHub()->loadNanoapp(kContextHubId, binary, kLoadTransactionId);
  verifyStatusAndSignal(/* operation= */ "loading nanoapp " + pathAndName,
                        status, gCallback->promise.get_future());
}

void unloadNanoapp(std::string &appIdOrName) {
  auto appId = NanoappHelper::getNanoappIdFrom(appIdOrName);
  auto status = getContextHub()->unloadNanoapp(kContextHubId, appId,
                                               kUnloadTransactionId);
  verifyStatusAndSignal(/* operation= */ "unloading nanoapp " + appIdOrName,
                        status, gCallback->promise.get_future());
}

void queryNanoapps() {
  auto status = getContextHub()->queryNanoapps(kContextHubId);
  verifyStatusAndSignal(/* operation= */ "querying nanoapps", status,
                        gCallback->promise.get_future());
}

HostEndpointInfo createHostEndpointInfo(const std::string &hexEndpointId) {
  char16_t hostEndpointId = verifyAndConvertEndpointHexId(hexEndpointId);
  return {
      .hostEndpointId = hostEndpointId,
      .type = HostEndpointInfo::Type::NATIVE,
      .packageName = "chre_aidl_hal_client",
      .attributionTag{},
  };
}

void onEndpointConnected(const std::string &hexEndpointId) {
  auto contextHub = getContextHub();
  HostEndpointInfo info = createHostEndpointInfo(hexEndpointId);
  // connect the endpoint to HAL
  verifyStatus(/* operation= */ "connect endpoint",
               contextHub->onHostEndpointConnected(info));
  std::cout << "Connected." << std::endl;
}

void onEndpointDisconnected(const std::string &hexEndpointId) {
  auto contextHub = getContextHub();
  uint16_t hostEndpointId = verifyAndConvertEndpointHexId(hexEndpointId);
  // disconnect the endpoint from HAL
  verifyStatus(/* operation= */ "disconnect endpoint",
               contextHub->onHostEndpointDisconnected(hostEndpointId));
  std::cout << "Disconnected." << std::endl;
}

ContextHubMessage createContextHubMessage(const std::string &hexHostEndpointId,
                                          std::string &appIdOrName,
                                          const std::string &hexPayload) {
  if (!isValidHexNumber(hexPayload)) {
    throwError("Invalid hex payload.");
  }
  int64_t appId = NanoappHelper::getNanoappIdFrom(appIdOrName);
  char16_t hostEndpointId = verifyAndConvertEndpointHexId(hexHostEndpointId);
  ContextHubMessage contextHubMessage = {
      .nanoappId = appId,
      .hostEndPoint = hostEndpointId,
      .messageBody = {},
      .permissions = {},
  };
  // populate the payload
  for (int i = 2; i < hexPayload.size(); i += 2) {
    contextHubMessage.messageBody.push_back(
        std::stoi(hexPayload.substr(i, 2), /* idx= */ nullptr, /* base= */ 16));
  }
  return contextHubMessage;
}

/** Sends a hexPayload from hexHostEndpointId to appIdOrName. */
void sendMessageToNanoapp(const std::string &hexHostEndpointId,
                          std::string &appIdOrName,
                          const std::string &hexPayload) {
  ContextHubMessage contextHubMessage =
      createContextHubMessage(hexHostEndpointId, appIdOrName, hexPayload);
  // send the message
  auto contextHub = getContextHub();
  auto status = contextHub->sendMessageToHub(kContextHubId, contextHubMessage);
  verifyStatusAndSignal(/* operation= */ "sending a message to " + appIdOrName,
                        status, gCallback->promise.get_future());
}

void changeSetting(const std::string &setting, bool enabled) {
  auto contextHub = getContextHub();
  int settingType = std::stoi(setting);
  if (settingType < 1 || settingType > 7) {
    throwError("setting type must be within [1, 7].");
  }
  ScopedAStatus status =
      contextHub->onSettingChanged(static_cast<Setting>(settingType), enabled);
  std::cout << "onSettingChanged is called to "
            << (enabled ? "enable" : "disable") << " setting type "
            << settingType << std::endl;
  verifyStatus("change setting", status);
}

void enableTestModeOnContextHub() {
  auto status = getContextHub()->setTestMode(/* in_enable= */ true);
  verifyStatus(/* operation= */ "enabling test mode", status);
  std::cout << "Test mode is enabled" << std::endl;
}

void disableTestModeOnContextHub() {
  auto status = getContextHub()->setTestMode(/* in_enable= */ false);
  verifyStatus(/* operation= */ "disabling test mode", status);
  std::cout << "Test mode is disabled" << std::endl;
}

void getAllPreloadedNanoappIds() {
  std::vector<int64_t> appIds{};
  verifyStatus("get preloaded nanoapp ids",
               getContextHub()->getPreloadedNanoappIds(kContextHubId, &appIds));
  for (const auto &appId : appIds) {
    std::cout << "0x" << std::hex << appId << std::endl;
  }
}

void fillSupportedCommandMap(
    const std::unordered_set<std::string> &supportedCommands,
    std::map<std::string, CommandInfo> &supportedCommandMap) {
  std::ranges::copy_if(
      kAllCommands,
      std::inserter(supportedCommandMap, supportedCommandMap.begin()),
      [&](auto const &kv_pair) {
        return supportedCommands.contains(kv_pair.first);
      });
}

void printUsage(const std::map<std::string, CommandInfo> &supportedCommands) {
  constexpr uint32_t kCommandLength = 40;
  std::cout << std::left << "Usage: COMMAND [ARGUMENTS]" << std::endl;
  for (auto const &kv_pair : supportedCommands) {
    std::string cmdLine = kv_pair.first + " " + kv_pair.second.argsFormat;
    std::cout << std::setw(kCommandLength) << cmdLine;
    if (cmdLine.size() > kCommandLength) {
      std::cout << std::endl << std::string(kCommandLength, ' ');
    }
    std::cout << " - " + kv_pair.second.usage << std::endl;
  }
  std::cout << std::endl;
}

Command parseCommand(
    const std::vector<std::string> &cmdLine,
    const std::map<std::string, CommandInfo> &supportedCommandMap) {
  if (cmdLine.empty() || !supportedCommandMap.contains(cmdLine[0])) {
    return unsupported;
  }
  const auto &cmdInfo = supportedCommandMap.at(cmdLine[0]);
  return cmdLine.size() == cmdInfo.numOfArgs ? cmdInfo.cmd : unsupported;
}

void executeCommand(std::vector<std::string> cmdLine) {
  switch (parseCommand(cmdLine, kAllCommands)) {
    case connectEndpoint: {
      onEndpointConnected(cmdLine[1]);
      break;
    }
    case disableSetting: {
      changeSetting(cmdLine[1], false);
      break;
    }
    case disableTestMode: {
      disableTestModeOnContextHub();
      break;
    }
    case disconnectEndpoint: {
      onEndpointDisconnected(cmdLine[1]);
      break;
    }
    case enableSetting: {
      changeSetting(cmdLine[1], true);
      break;
    }
    case enableTestMode: {
      enableTestModeOnContextHub();
      break;
    }
    case getContextHubs: {
      getAllContextHubs();
      break;
    }
    case getPreloadedNanoappIds: {
      getAllPreloadedNanoappIds();
      break;
    }
    case list: {
      std::map<std::string, NanoAppBinaryHeader> nanoapps{};
      NanoappHelper::readNanoappHeaders(nanoapps, cmdLine[1]);
      for (const auto &entity : nanoapps) {
        std::cout << entity.first;
        NanoappHelper::printNanoappHeader(entity.second);
      }
      break;
    }
    case load: {
      loadNanoapp(cmdLine[1]);
      break;
    }
    case query: {
      queryNanoapps();
      break;
    }
    case registerCallback: {
      registerHostCallback();
      break;
    }
    case sendMessage: {
      sendMessageToNanoapp(cmdLine[1], cmdLine[2], cmdLine[3]);
      break;
    }
    case unload: {
      unloadNanoapp(cmdLine[1]);
      break;
    }
    default:
      printUsage(kAllCommands);
  }
}

std::vector<std::string> getCommandLine() {
  std::string input;
  std::cout << "> ";
  std::getline(std::cin, input);
  input.push_back('\n');
  std::vector<std::string> result{};
  for (int begin = 0, end = 0; end < input.size();) {
    if (isspace(input[begin])) {
      end = begin = begin + 1;
      continue;
    }
    if (!isspace(input[end])) {
      end += 1;
      continue;
    }
    result.push_back(input.substr(begin, end - begin));
    begin = end;
  }
  return result;
}

void connectToHal() {
  if (gCallback == nullptr) {
    gCallback = ContextHubCallback::make<ContextHubCallback>();
  }
  std::unique_ptr<HalClient> halClient = HalClient::create(gCallback);
  if (halClient == nullptr || !halClient->connect()) {
    LOGE("Failed to init the connection to HAL.");
    return;
  }
  std::unordered_set<std::string> supportedCommands = {
      "connectEndpoint", "disconnectEndpoint", "query", "sendMessage"};
  std::map<std::string, CommandInfo> supportedCommandMap{};
  fillSupportedCommandMap(supportedCommands, supportedCommandMap);

  while (true) {
    auto cmdLine = getCommandLine();
    if (cmdLine.empty()) {
      continue;
    }
    if (cmdLine.size() == 1 && cmdLine[0] == "exit") {
      break;
    }
    try {
      switch (parseCommand(cmdLine, supportedCommandMap)) {
        case connectEndpoint: {
          HostEndpointInfo info =
              createHostEndpointInfo(/* hexEndpointId= */ cmdLine[1]);
          verifyStatus(/* operation= */ "connect endpoint",
                       halClient->connectEndpoint(info));
          break;
        }

        case query: {
          verifyStatusAndSignal(/* operation= */ "querying nanoapps",
                                halClient->queryNanoapps(),
                                gCallback->promise.get_future());
          break;
        }

        case disconnectEndpoint: {
          uint16_t hostEndpointId =
              verifyAndConvertEndpointHexId(/* number= */ cmdLine[1]);
          verifyStatus(/* operation= */ "disconnect endpoint",
                       halClient->disconnectEndpoint(hostEndpointId));
          break;
        }
        case sendMessage: {
          ContextHubMessage message = createContextHubMessage(
              /* hexHostEndpointId= */ cmdLine[1],
              /* appIdOrName= */ cmdLine[2], /* hexPayload= */ cmdLine[3]);
          verifyStatusAndSignal(
              /* operation= */ "sending a message to " + cmdLine[2],
              halClient->sendMessage(message), gCallback->promise.get_future());
          break;
        }
        default:
          printUsage(supportedCommandMap);
      }
    } catch (std::system_error &e) {
      std::cerr << e.what() << std::endl;
    }
  }
}
}  // anonymous namespace
}  // namespace android::chre::chre_aidl_hal_client

int main(int argc, char *argv[]) {
  using namespace android::chre::chre_aidl_hal_client;
  // Start binder thread pool to enable callbacks.
  ABinderProcess_startThreadPool();

  std::vector<std::string> cmdLine{};
  for (int i = 1; i < argc; i++) {
    cmdLine.emplace_back(argv[i]);
  }
  try {
    if (cmdLine.size() == 1 && cmdLine[0] == "connect") {
      connectToHal();
      return 0;
    }
    executeCommand(cmdLine);
  } catch (std::system_error &e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }
  return 0;
}