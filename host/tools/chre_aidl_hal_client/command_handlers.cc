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
#include "context_hub_callback.h"
#include "nanoapp_helper.h"
#include "utils.h"

#include <aidl/android/hardware/contexthub/BnContextHubCallback.h>
#include <aidl/android/hardware/contexthub/IContextHub.h>
#include <aidl/android/hardware/contexthub/NanoappBinary.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <future>
#include <stdexcept>
#include <string>
#include <vector>

#include "chre_host/file_stream.h"
#include "chre_host/hal_client.h"
#include "chre_host/log.h"
#include "chre_host/napp_header.h"
#include "endpoint_callback.h"

namespace android::chre::chre_aidl_hal_client {

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

// Session based messaging related variables.
std::shared_ptr<IEndpointCallback> gEndpointCallback = nullptr;
std::shared_ptr<IEndpointCommunication> gCommunication = nullptr;

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

void getAllHubs() {
  std::vector<HubInfo> hubs{};
  if (const auto status = getContextHub()->getHubs(&hubs); !status.isOk()) {
    std::cerr << "Failed to get hubs: " << status.getMessage() << std::endl;
    return;
  }
  if (hubs.empty()) {
    std::cerr << "No hubs found" << std::endl;
    return;
  }
  for (const auto &[hubId, hubDetails] : hubs) {
    std::cout << "Hub id: 0x" << std::hex << hubId << " "
              << hubDetails.toString() << std::endl;
  }
}

void getAllEndpoints() {
  std::vector<EndpointInfo> endpoints{};
  if (const auto status = getContextHub()->getEndpoints(&endpoints);
      !status.isOk()) {
    std::cerr << "Failed to get endpoints: " << status.getMessage()
              << std::endl;
    return;
  }
  EndpointHelper::printEndpoints(endpoints);
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

void executeHalClientCommand(HalClient *halClient,
                             const std::vector<std::string> &cmdLine) {
  if (auto func = CommandHelper::parseCommand(cmdLine, kHalClientCommands)) {
    try {
      func(halClient, cmdLine);
    } catch (std::system_error &e) {
      std::cerr << e.what() << std::endl;
    }
  } else {
    CommandHelper::printUsage(kHalClientCommands);
  }
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

  while (true) {
    auto cmdLine = CommandHelper::getCommandLine();
    if (cmdLine.empty()) {
      continue;
    }
    executeHalClientCommand(halClient.get(), cmdLine);
  }
}

void halClientConnectEndpoint(HalClient *halClient,
                              const std::string &hexEndpointId) {
  HostEndpointInfo info = createHostEndpointInfo(hexEndpointId);
  verifyStatus(/* operation= */ "connect endpoint",
               halClient->connectEndpoint(info));
}

void halClientDisconnectEndpoint(HalClient *halClient,
                                 const std::string &hexEndpointId) {
  uint16_t hostEndpointId = verifyAndConvertEndpointHexId(hexEndpointId);
  verifyStatus(/* operation= */ "disconnect endpoint",
               halClient->disconnectEndpoint(hostEndpointId));
}

void halClientGetEndpoints(HalClient *halClient) {
  std::vector<EndpointInfo> endpoints{};
  verifyStatus(/* operation= */ "get session-based endpoints",
               halClient->getEndpoints(&endpoints));
  EndpointHelper::printEndpoints(endpoints);
}

void halClientGetHubs(HalClient *halClient) {
  std::vector<HubInfo> hubs{};
  verifyStatus(/*operation= */ "Get session-based hubs",
               halClient->getHubs(&hubs));
  if (hubs.empty()) {
    std::cerr << "No hubs found" << std::endl;
    return;
  }
  for (const auto &[hubId, hubDetails] : hubs) {
    std::cout << "Hub id: 0x" << std::hex << hubId << " "
              << hubDetails.toString() << std::endl;
  }
}

void halClientQuery(HalClient *halClient) {
  verifyStatusAndSignal(/* operation= */ "querying nanoapps",
                        halClient->queryNanoapps(),
                        gCallback->promise.get_future());
}

void halClientSendMessage(HalClient *halClient,
                          const std::vector<std::string> &cmdLine) {
  std::string appIdOrName = cmdLine[2];
  ContextHubMessage message = createContextHubMessage(
      /* hexHostEndpointId= */ cmdLine[1], appIdOrName,
      /* hexPayload= */ cmdLine[3]);
  verifyStatusAndSignal(
      /* operation= */ "sending a message to " + cmdLine[2],
      halClient->sendMessage(message), gCallback->promise.get_future());
}

void halClientRegisterHub(HalClient *halClient) {
  gEndpointCallback = EndpointCallback::make<EndpointCallback>();
  verifyStatus(/* operation= */ "register an endpoint hub",
               halClient->registerEndpointHub(gEndpointCallback, kHubInfo,
                                              &gCommunication));
}

std::vector<std::string> CommandHelper::getCommandLine() {
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
}  // namespace android::chre::chre_aidl_hal_client