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

#include <sys/types.h>
#include <map>
#include <string>

namespace android::chre::chre_aidl_hal_client {

/**
 * Enumerates the supported commands for the CHRE AIDL HAL client tool.
 * Please keep Command in alphabetical order.
 */
enum Command {
  connect,
  connectEndpoint,
  disableSetting,
  disableTestMode,
  disconnectEndpoint,
  enableSetting,
  enableTestMode,
  getEndpoints,
  getContextHubs,
  getHubs,
  getPreloadedNanoappIds,
  list,
  load,
  query,
  registerCallback,
  sendMessage,
  unload,
  unsupported  // Represents an unrecognized or invalid command.
};

/** Holds metadata associated with a specific command. */
struct CommandInfo {
  /** The enum value representing the command. */
  Command cmd;

  /**
   * Number of arguments expected for the command, including the command name
   * itself.
   */
  u_int8_t numOfArgs;

  /**
   * A string describing the expected arguments format (e.g.,
   * "<HEX_ENDPOINT_ID>"). Empty if no arguments are expected besides the
   * command name.
   */
  std::string argsFormat;

  /** A brief description of what the command does. */
  std::string usage;
};

/**
 * A map associating command strings (used on the command line) with their
 * corresponding CommandInfo metadata.
 */
const std::map<std::string, CommandInfo> kAllCommands{
    {"connect",
     {.cmd = connect,
      .numOfArgs = 1,
      .argsFormat = "",
      .usage = "connect to HAL using hal_client library and keep the session "
               "alive while user can execute other commands. Use 'exit' to "
               "quit the session."}},

    {"connectEndpoint",
     {.cmd = connectEndpoint,
      .numOfArgs = 2,
      .argsFormat = "<HEX_ENDPOINT_ID>",
      .usage =
          "associate an endpoint with the current client and notify HAL."}},

    {"disableSetting",
     {.cmd = disableSetting,
      .numOfArgs = 2,
      .argsFormat = "<SETTING>",
      .usage = "disable a setting identified by a number defined in "
               "android/hardware/contexthub/Setting.aidl."}},

    {"disableTestMode",
     {.cmd = disableTestMode,
      .numOfArgs = 1,
      .argsFormat = "",
      .usage = "disable test mode."}},

    {"disconnectEndpoint",
     {.cmd = disconnectEndpoint,
      .numOfArgs = 2,
      .argsFormat = "<HEX_ENDPOINT_ID>",
      .usage = "remove an endpoint with the current client and notify HAL."}},

    {"enableSetting",
     {.cmd = enableSetting,
      .numOfArgs = 2,
      .argsFormat = "<SETTING>",
      .usage = "enable a setting identified by a number defined in "
               "android/hardware/contexthub/Setting.aidl."}},

    {"enableTestMode",
     {.cmd = enableTestMode,
      .numOfArgs = 1,
      .argsFormat = "",
      .usage = "enable test mode."}},

    {"getEndpoints",
     {.cmd = getEndpoints,
      .numOfArgs = 1,
      .argsFormat = "",
      .usage = "get all the endpoints used for session-based messaging."}},

    {"getContextHubs",
     {.cmd = getContextHubs,
      .numOfArgs = 1,
      .argsFormat = "",
      .usage = "get all the context hubs."}},

    {"getHubs",
     {.cmd = getHubs,
      .numOfArgs = 1,
      .argsFormat = "",
      .usage = "get all the hubs for session-based messaging."}},

    {"getPreloadedNanoappIds",
     {.cmd = getPreloadedNanoappIds,
      .numOfArgs = 1,
      .argsFormat = "",
      .usage = "get a list of ids for the preloaded nanoapps."}},

    {"list",
     {.cmd = list,
      .numOfArgs = 2,
      .argsFormat = "</PATH/TO/NANOAPPS>",
      .usage = "list all the nanoapps' header info in the path."}},

    {"load",
     {.cmd = load,
      .numOfArgs = 2,
      .argsFormat = "<APP_NAME | /PATH/TO/APP_NAME>",
      .usage = "load the nanoapp specified by the name. If an absolute path is "
               "not provided the default locations are searched."}},

    {"query",
     {.cmd = query,
      .numOfArgs = 1,
      .argsFormat = "",
      .usage = "show all loaded nanoapps (system apps excluded)."}},

    {"registerCallback",
     {.cmd = registerCallback,
      .numOfArgs = 1,
      .argsFormat = "",
      .usage = "register a callback for the current client."}},

    {"sendMessage",
     {.cmd = sendMessage,
      .numOfArgs = 4,
      .argsFormat = "<HEX_ENDPOINT_ID> <HEX_NANOAPP_ID | APP_NAME | "
                    "/PATH/TO/APP_NAME> <HEX_PAYLOAD>",
      .usage = "send a payload to a nanoapp. If an absolute path is not "
               "provided the default locations are searched."}},

    {"unload",
     {.cmd = unload,
      .numOfArgs = 2,
      .argsFormat = "<HEX_NANOAPP_ID | APP_NAME | /PATH/TO/APP_NAME>",
      .usage = "unload the nanoapp specified by either the nanoapp id or the "
               "app name. If an absolute path is not provided the default "
               "locations are searched."}},
};
}  // namespace android::chre::chre_aidl_hal_client
#endif  // ANDROID_CHRE_AIDL_HAL_CLIENT_COMMANDS_H