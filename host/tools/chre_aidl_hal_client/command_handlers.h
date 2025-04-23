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

#ifndef ANDROID_CHRE_AIDL_HAL_CLIENT_COMMANDS_H
#define ANDROID_CHRE_AIDL_HAL_CLIENT_COMMANDS_H

#include "nanoapp_helper.h"

#include <sys/types.h>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "chre_host/hal_client.h"

namespace android::chre::chre_aidl_hal_client {

/** Handlers that run directly after connecting to HAL. */
void onEndpointConnected(const std::string &hexEndpointId);
void changeSetting(const std::string &setting, bool enabled);
void disableTestModeOnContextHub();
void onEndpointDisconnected(const std::string &hexEndpointId);
void enableTestModeOnContextHub();
void getAllEndpoints();
void getAllContextHubs();
void getAllHubs();
void getAllPreloadedNanoappIds();
void loadNanoapp(std::string &pathAndName);
void queryNanoapps();
void registerHostCallback();
void sendMessageToNanoapp(const std::string &hexHostEndpointId,
                          std::string &appIdOrName,
                          const std::string &hexPayload);
void unloadNanoapp(std::string &appIdOrName);

/** The handler that connects to HAL using hal_client library. */
void connectToHal();

/** Handlers for commands that can only be run after connecting to HAL. */
void halClientConnectEndpoint(HalClient *halClient,
                              const std::string &hexEndpointId);
void halClientDisconnectEndpoint(HalClient *halClient,
                                 const std::string &hexEndpointId);
void halClientGetEndpoints(HalClient *halClient);
void halClientGetHubs(HalClient *halClient);
void halClientQuery(HalClient *halClient);
void halClientRegisterHub(HalClient *halClient);
void halClientSendMessage(HalClient *halClient,
                          const std::vector<std::string> &cmdLine);

using DirectCommandFunction =
    std::function<void(const std::vector<std::string> &)>;
using HalClientCommandFunction =
    std::function<void(HalClient *halClient, const std::vector<std::string> &)>;

/** Holds metadata associated with a specific command. */
template <typename FuncType>
struct CommandInfo {
  /** The command string. */
  std::string cmd;

  /**
   * Number of arguments expected *after* the command name.
   * For example, if the command is "load <app_name>", numOfArgs is 1.
   * If the command is "query", numOfArgs is 0.
   */
  u_int8_t numOfArgs;

  /**
   * A string describing the expected arguments format (e.g.,
   * "<HEX_ENDPOINT_ID>"). Empty if no arguments are expected.
   */
  std::string argsFormat;

  /** A brief description of what the command does. */
  std::string usage;

  /** The function to execute for this command. */
  FuncType func;
};

/**
 * The commands that can be run directly by chre_aidl_hal_client.
 *
 * <p>Please keep Command in alphabetical order.
 */
const std::vector<CommandInfo<DirectCommandFunction>> kAllDirectCommands{
    {.cmd = "connect",
     .numOfArgs = 0,
     .argsFormat = "",
     .usage = "connect to HAL using hal_client library and keep the session "
              "alive while user can execute other commands. Use 'exit' to "
              "quit the session.",
     .func =
         [](const std::vector<std::string> & /*cmdLine*/) { connectToHal(); }},

    {.cmd = "connectEndpoint",
     .numOfArgs = 1,
     .argsFormat = "<HEX_ENDPOINT_ID>",
     .usage = "associate an endpoint with the current client and notify HAL.",
     .func =
         [](const std::vector<std::string> &cmdLine) {
           onEndpointConnected(cmdLine[1]);
         }},

    {.cmd = "disableSetting",
     .numOfArgs = 1,
     .argsFormat = "<SETTING>",
     .usage = "disable a setting identified by a number defined in "
              "android/hardware/contexthub/Setting.aidl.",
     .func =
         [](const std::vector<std::string> &cmdLine) {
           changeSetting(cmdLine[1], /* enabled= */ false);
         }},

    {.cmd = "disableTestMode",
     .numOfArgs = 0,
     .argsFormat = "",
     .usage = "disable test mode.",
     .func =
         [](const std::vector<std::string> & /*cmdLine*/) {
           disableTestModeOnContextHub();
         }},

    {.cmd = "disconnectEndpoint",
     .numOfArgs = 1,
     .argsFormat = "<HEX_ENDPOINT_ID>",
     .usage = "remove an endpoint with the current client and notify HAL.",
     .func =
         [](const std::vector<std::string> &cmdLine) {
           onEndpointDisconnected(cmdLine[1]);
         }},

    {.cmd = "enableSetting",
     .numOfArgs = 1,
     .argsFormat = "<SETTING>",
     .usage = "enable a setting identified by a number defined in "
              "android/hardware/contexthub/Setting.aidl.",
     .func =
         [](const std::vector<std::string> &cmdLine) {
           changeSetting(cmdLine[1], true);
         }},

    {.cmd = "enableTestMode",
     .numOfArgs = 0,
     .argsFormat = "",
     .usage = "enable test mode.",
     .func =
         [](const std::vector<std::string> & /*cmdLine*/) {
           enableTestModeOnContextHub();
         }},

    {.cmd = "getEndpoints",
     .numOfArgs = 0,
     .argsFormat = "",
     .usage = "get all the endpoints used for session-based messaging.",
     .func =
         [](const std::vector<std::string> & /*cmdLine*/) {
           getAllEndpoints();
         }},

    {.cmd = "getContextHubs",
     .numOfArgs = 0,
     .argsFormat = "",
     .usage = "get all the context hubs.",
     .func =
         [](const std::vector<std::string> & /*cmdLine*/) {
           getAllContextHubs();
         }},

    {.cmd = "getHubs",
     .numOfArgs = 0,
     .argsFormat = "",
     .usage = "get all the hubs for session-based messaging.",
     .func =
         [](const std::vector<std::string> & /*cmdLine*/) { getAllHubs(); }},

    {.cmd = "getPreloadedNanoappIds",
     .numOfArgs = 0,
     .argsFormat = "",
     .usage = "get a list of ids for the preloaded nanoapps.",
     .func =
         [](const std::vector<std::string> & /*cmdLine*/) {
           getAllPreloadedNanoappIds();
         }},

    {.cmd = "list",
     .numOfArgs = 1,
     .argsFormat = "</PATH/TO/NANOAPPS>",
     .usage = "list all the nanoapps' header info in the path.",
     .func =
         [](const std::vector<std::string> &cmdLine) {
           NanoappHelper::listNanoappsInPath(cmdLine[1]);
         }},

    {.cmd = "load",
     .numOfArgs = 1,
     .argsFormat = "<APP_NAME | /PATH/TO/APP_NAME>",
     .usage = "load the nanoapp specified by the name. If an absolute path is "
              "not provided the default locations are searched.",
     // Need a mutable copy for findHeaderAndNormalizePath
     .func =
         [](const std::vector<std::string> &cmdLine) {
           auto appName = cmdLine[1];
           loadNanoapp(appName);
         }},

    {.cmd = "query",
     .numOfArgs = 0,
     .argsFormat = "",
     .usage = "show all loaded nanoapps (system apps excluded).",
     .func =
         [](const std::vector<std::string> & /*cmdLine*/) { queryNanoapps(); }},

    {.cmd = "registerCallback",
     .numOfArgs = 0,
     .argsFormat = "",
     .usage = "register a callback for the current client.",
     .func =
         [](const std::vector<std::string> & /*cmdLine*/) {
           registerHostCallback();
         }},

    {.cmd = "sendMessage",
     .numOfArgs = 3,
     .argsFormat = "<HEX_ENDPOINT_ID> <HEX_NANOAPP_ID | APP_NAME | "
                   "/PATH/TO/APP_NAME> <HEX_PAYLOAD>",
     .usage = "send a payload to a nanoapp. If an absolute path is not "
              "provided the default locations are searched.",
     // Need a mutable copy for getNanoappIdFrom potentially
     .func =
         [](const std::vector<std::string> &cmdLine) {
           auto appIdOrName = cmdLine[2];
           sendMessageToNanoapp(cmdLine[1], appIdOrName, cmdLine[3]);
         }},

    {.cmd = "unload",
     .numOfArgs = 1,
     .argsFormat = "<HEX_NANOAPP_ID | APP_NAME | /PATH/TO/APP_NAME>",
     .usage = "unload the nanoapp specified by either the nanoapp id or the "
              "app name. If an absolute path is not provided the default "
              "locations are searched.",
     // Need a mutable copy for getNanoappIdFrom potentially
     .func =
         [](const std::vector<std::string> &cmdLine) {
           auto appIdOrName = cmdLine[1];
           unloadNanoapp(appIdOrName);
         }},
};

/**
 * The commands that can only be run after connecting to HAL via HalClient,
 * which is what {@code connect} command does.
 *
 * Please keep Command in alphabetical order.
 */
const std::vector<CommandInfo<HalClientCommandFunction>> kHalClientCommands{
    {
        .cmd = "connectEndpoint",
        .numOfArgs = 1,
        .argsFormat = "<HEX_ENDPOINT_ID>",
        .usage =
            "associate an endpoint with the current client and notify HAL.",
        .func =
            [](HalClient *halClient, const std::vector<std::string> &cmdLine) {
              halClientConnectEndpoint(halClient, cmdLine[1]);
            },
    },

    {
        .cmd = "disconnectEndpoint",
        .numOfArgs = 1,
        .argsFormat = "<HEX_ENDPOINT_ID>",
        .usage = "remove an endpoint with the current client and notify HAL.",
        .func =
            [](HalClient *halClient, const std::vector<std::string> &cmdLine) {
              halClientDisconnectEndpoint(halClient, cmdLine[1]);
            },
    },

    {
        .cmd = "exit",
        .numOfArgs = 0,
        .argsFormat = "",
        .usage = "Quit the connection mode.",
        .func = [](HalClient * /*halClient*/,
                   const std::vector<std::string> & /*cmdLine*/) { exit(0); },
    },

    {
        .cmd = "getHubs",
        .numOfArgs = 0,
        .argsFormat = "",
        .usage = "get all the hubs for session-based messaging.",
        .func =
            [](HalClient *halClient,
               const std::vector<std::string> & /*cmdLine*/) {
              halClientGetHubs(halClient);
            },
    },

    {
        .cmd = "getEndpoints",
        .numOfArgs = 0,
        .argsFormat = "",
        .usage = "get all the endpoints used for session-based messaging.",
        .func =
            [](HalClient *halClient,
               const std::vector<std::string> & /*cmdLine*/) {
              halClientGetEndpoints(halClient);
            },
    },

    {
        .cmd = "query",
        .numOfArgs = 0,
        .argsFormat = "",
        .usage = "show all loaded nanoapps (system apps excluded).",
        .func =
            [](HalClient *halClient,
               const std::vector<std::string> & /*cmdLine*/) {
              halClientQuery(halClient);
            },
    },

    {
        .cmd = "registerHub",
        .numOfArgs = 0,
        .argsFormat = "",
        .usage = "register a hub using id and name.",
        .func =
            [](HalClient *halClient,
               const std::vector<std::string> & /*cmdLine*/) {
              halClientRegisterHub(halClient);
            },
    },

    {
        .cmd = "sendMessage",
        .numOfArgs = 3,
        .argsFormat = "<HEX_ENDPOINT_ID> <HEX_NANOAPP_ID | APP_NAME | "
                      "/PATH/TO/APP_NAME> <HEX_PAYLOAD>",
        .usage = "send a payload to a nanoapp. If an absolute path is not "
                 "provided the default locations are searched.",
        .func =
            [](HalClient *halClient, const std::vector<std::string> &cmdLine) {
              halClientSendMessage(halClient, cmdLine);
            },
    },
};

/**
 * Helper class to manage command definitions and potentially parsing/usage
 * logic.
 */
class CommandHelper {
 public:
  CommandHelper() = delete;

  /**
   * Parses the command line input and finds the matching handler function.
   *
   * Checks the command name and the number of arguments.
   *
   * @param cmdLine The command line arguments as a vector of strings.
   * @param supportedCommands A vector of supported CommandInfo structs.
   * @return A CommandFunction that matches the command name and arguments.
   */
  template <typename FuncType>
  static FuncType parseCommand(
      const std::vector<std::string> &cmdLine,
      const std::vector<CommandInfo<FuncType>> &supportedCommands) {
    auto cmdInfoItor =
        std::ranges::find_if(supportedCommands.begin(), supportedCommands.end(),
                             [&](const CommandInfo<FuncType> &cmdInfo) {
                               return cmdInfo.cmd == cmdLine[0] &&
                                      cmdInfo.numOfArgs == cmdLine.size() - 1;
                             });
    return cmdInfoItor != supportedCommands.end() ? cmdInfoItor->func : nullptr;
  }

  /** Prints the usage instructions for the supported commands. */
  template <typename FuncType>
  static void printUsage(
      const std::vector<CommandInfo<FuncType>> &supportedCommands) {
    std::cout << std::left << "Usage: COMMAND [ARGUMENTS]" << std::endl;
    for (auto const &command : supportedCommands) {
      std::string cmdLine = command.cmd + " " + command.argsFormat;
      std::cout << std::setw(kCommandLength) << cmdLine;
      if (cmdLine.size() > kCommandLength) {
        std::cout << std::endl << std::string(kCommandLength, ' ');
      }
      std::cout << " - " + command.usage << std::endl;
    }
    std::cout << std::endl;
  }

  /**
   * Reads a line from standard input and parses it into command line arguments.
   *
   * @return A vector of strings representing the parsed command line arguments.
   */
  static std::vector<std::string> getCommandLine();

 private:
  static constexpr uint32_t kCommandLength = 40;
};

}  // namespace android::chre::chre_aidl_hal_client
#endif  // ANDROID_CHRE_AIDL_HAL_CLIENT_COMMANDS_H