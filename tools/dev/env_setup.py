#
# Copyright 2025, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""A script to set up the CHRE development environment.

This script should only be incurred by a shell script.

When run with a platform-target combination (e.g., `python env_setup.py -p
<platform>-<target>`), it interactively prompts the user for necessary
environment variables based on the env_config.json file. It then generates a
series of <name>-<value> pairs that can be exported by the shell to configure
the environment. If a `default_action` is provided with a default value together
the action will be incurred before exporting the default value.

The result is printed to stdout and piped into the shell to set the environment
variables.
"""

from argparse import ArgumentParser
import json
import os
import re
import subprocess
import sys
from typing import Any

from shell_util import check_dependencies, fatal_error, get_input_from_shell, init_file, log_e, log_i, log_w


def _print_env_var_pair(env_var: str):
  env_name, env_value = re.match(r"(.*)=(.*)", env_var).groups()
  print(f"\033[32m{env_name}\033[0m = {env_value}", file=sys.stderr)


def _get_canonical_path(path: str):
  return os.path.expanduser(os.path.expandvars(path))


def _action_clone_repo(url: str, branch: str, dest: str) -> None:
  """Clones a Git repository from 'url' with a specific 'branch' into 'dest'.

  Args:
      url (str): The URL of the Git repository.
      branch (str): The branch to clone.
      dest (str): The destination directory to clone the repository into.
  """
  if os.path.exists(dest):
    answer = get_input_from_shell(
        f"{dest} already exists. Shall we override it? (y/N):", color="yellow"
    )
    if not answer or answer.lower() == "n":
      log_i(f"Skipping clone operation for {dest}")
      return
    log_i(f"Removing existing directory: {dest}")
    subprocess.run(["rm", "-rf", dest], check=True)

  try:
    # The --single-branch option fetches only the specified branch, saving time and space.
    subprocess.run(
        ["git", "clone", "--branch", branch, "--single-branch", url, dest],
        check=True,
    )
    log_i(f"Successfully cloned {url} (branch: {branch}) into {dest}")
  except subprocess.CalledProcessError:
    fatal_error("Error cloning repository")
  except FileNotFoundError:
    fatal_error(
        "Error: 'git' command not found. Please ensure Git is installed and in"
        " your PATH."
    )


def _check_single_value(value, v_type):
  _expanded_value = _get_canonical_path(value)
  if v_type == "path":
    if not os.path.isdir(_expanded_value):
      raise ValueError(f"Path '{value}' does not exist")
  elif v_type == "file":
    if not os.path.isfile(_expanded_value):
      raise ValueError(f"File '{value}' does not exist")
  elif v_type == "value":
    if not re.match(r"^[\w\-]+$", _expanded_value, flags=re.ASCII):
      raise ValueError(
          f"Invalid value '{_expanded_value}'. Only dash and characters in"
          " [a-zA-Z0-9_] are allowed"
      )
  else:
    raise ValueError(f"Unknown value type '{v_type}' for value '{value}'")
  return _expanded_value


def _assert_and_expand_env_variable(env_name, env_type: str, env_value: str):
  """Expands and validates an environment variable, then sets it.

  This function performs the following steps:
  1. Expands shell variables (like ~ and $HOME) in the provided value.
  2. Validates the expanded value based on its type.
  3. Updates os.environ with the validated environment variable.

  Args:
    env_name: The name of the environment variable.
    env_type: The type of the variable for validation.
    env_value: The value of the environment variable.

  Returns:
    The expanded value of the environment variable.
  """
  matched_list_type = re.match(r"^list\[(.*)]$", env_type)
  if matched_list_type:
    expanded_value = ":".join(
        _check_single_value(v, matched_list_type.group(1))
        for v in env_value.split()
    )
  else:
    expanded_value = _check_single_value(env_value, env_type)
  os.environ[env_name] = expanded_value
  return expanded_value


def _run_action(action_and_args):
  try:
    func = getattr(sys.modules[__name__], "_" + action_and_args[0])
    expanded_args = [_get_canonical_path(arg) for arg in action_and_args[1:]]
    func(*expanded_args)
  except AttributeError:
    fatal_error(f"Unknown action: '{action_and_args[0]}'")


def _load_config(config_file: str = None) -> Any:
  """Loads and parses the configuration file.

  Args:
    config_file: The file containing the config json file. When it's not
      provided the path to the directory containing this script must be set in
      the CHRE_DEV_SCRIPT_PATH environment variable so that the default config
      file can be retrieved.

  Returns:
    A dictionary containing the parsed JSON configuration data.
  """

  if config_file is None:
    if not os.getenv("CHRE_DEV_SCRIPT_PATH"):
      fatal_error("CHRE_DEV_SCRIPT_PATH must be set before calling this script")
    config_file = os.path.join(
        os.getenv("CHRE_DEV_SCRIPT_PATH"), "env_config.json"
    )
  else:
    config_file = _get_canonical_path(config_file)

  try:
    with open(config_file, "r") as f:
      return json.load(f), config_file
  except FileNotFoundError:
    fatal_error(f"Error: Config file '{config_file}' not found")
  except json.JSONDecodeError as e:
    fatal_error(f"Error: Invalid JSON format in '{config_file}'\n{e}")


def _get_supported_combinations(config_data: Any) -> list[str]:
  """Returns a sorted list of supported platform-target combinations."""
  return sorted([
      f"{entry.get('platform')}-{target.get('name')}"
      for entry in config_data
      for target in entry.get("targets", [])
  ])


def _parse_platform_and_target_configs(
    config_data: Any, platform_and_target: str
):
  """Parses config data to find predefined env vars for a platform-target.

  Args:
    config_data: The parsed JSON configuration data.
    platform_and_target: A string in the format "<platform_name>-<target_name>".

  Returns:
    A tuple containing:
      - A dictionary of predefined environment variables.
      - A list of environment variable definitions to be processed further.
  """
  supported_combinations = _get_supported_combinations(config_data)

  if not platform_and_target or not re.match(r"^\w+-\w+$", platform_and_target):
    fatal_error(
        "platform and target must be in the format of"
        " <platform_name-target_name>\nSupported choices are:"
        f" {supported_combinations}"
    )

  platform_name, target_name = platform_and_target.split("-")
  for platform in config_data:
    try:
      if platform["platform"] != platform_name:
        continue
      for target in platform["targets"]:
        if target["name"] != target_name:
          continue
        # Required configurations
        env_map = {
            "CHRE_PLATFORM": platform_name,
            "CHRE_TARGET_TYPE": target_name,
            "CHRE_BUILD_TARGET": target["build_target"],
            "CHRE_DEV_PATH": _get_canonical_path(
                f"~/.chre_dev/{platform_name}-{target_name}"
            ),
        }
        # Optional configurations
        if platform.get("python_version"):
          env_map["CHRE_PYTHON_VERSION"] = platform.get("python_version")
        if platform.get("signer"):
          env_map["CHRE_SIGNER_PATH"] = _get_canonical_path(
              platform.get("signer")
          )
        if target.get("install_location"):
          env_map["TARGET_INSTALL_LOCATION"] = target["install_location"]
        if target.get("quick_flash_command"):
          env_map["QUICK_FLASH_COMMAND"] = target["quick_flash_command"]
        envs = platform.get("common_env_variables", []) + target.get(
            "env_variables", []
        )
        return env_map, envs
    except KeyError as e:
      fatal_error(
          f"Malformed config for {platform_name}-{target_name}: '{e.args[0]}'"
          " field is not defined"
      )

  fatal_error(
      "No platform-target combination found for"
      f" '{platform_name}-{target_name}'\nSupported choices are:"
      f" {supported_combinations}"
  )


def _parse_env_variable_fields(env_vars, predefined_envs, interactive: bool):
  """Interactively prompts for and processes environment variables.

  Iterates through a list of environment variable definitions, prompts the user
  for values, validates them, and generates <name>=<value> pairs. If a
  default value is provided and no user input is given, the default value will
  be used. When a default action is provided, it will be executed if the default
   value is used.

  Args:
    predefined_envs: A dict of environment variables pre-defined for the
      platform and the target.
    env_vars: A list of dictionaries, where each dictionary defines an
      environment variable customizable by the user.
    interactive: A boolean indicating whether to run in interactive mode.

  Returns:
    A list of <name>=<value> pairs representing the environment variables.
  """
  all_env_names = set(predefined_envs)
  env_var_pairs = [f'{k}="{v}"' for k, v in predefined_envs.items()]
  for env_var in env_vars:
    try:
      env_name = env_var["name"]
      if env_name in all_env_names:
        fatal_error(f"Duplicate env variable name: {env_name}")
      all_env_names.add(f"{env_name}")
      default_value = env_var.get("default")
      default_action = env_var.get("default_action", [])
      prompt = f"{env_name}"
      if default_value is not None:
        prompt = "{} ({})".format(
            env_name, default_value if default_value else "EMPTY"
        )

      if interactive or default_value is None:
        print("\n{}".format(env_var.get("description", "")), file=sys.stderr)
        while True:
          user_entered_value = get_input_from_shell(f"{prompt}: ")
          if not user_entered_value and default_value:
            user_entered_value = default_value
            break
          try:
            _check_single_value(user_entered_value, env_var["type"])
            break
          except ValueError as e:
            log_w(f"{e.args[0]}. Try again.")
      else:
        # Take default action if the default value is not valid
        try:
          _check_single_value(default_value, env_var["type"])
        except ValueError:
          if default_action:
            _run_action(default_action)
        user_entered_value = default_value

      if user_entered_value:
        expanded_value = _assert_and_expand_env_variable(
            env_name, env_var["type"], user_entered_value
        )
        env_var_pairs.append(f'{env_name}="{expanded_value}"')
        continue

      # Either user has to enter a value or default_value must be provided
      if default_value is None and not user_entered_value:
        fatal_error(f"{env_name} must be provided. Please try it again")

      # Default action is supposed to have made the default value valid
      expanded_value = _assert_and_expand_env_variable(
          env_name, env_var["type"], default_value
      )
      env_var_pairs.append(f'{env_name}="{expanded_value}"')
    except KeyError as e:
      fatal_error(
          f"The environment variable doesn't have the field '{e.args[0]}'"
      )

  # Create CHRE_ENVS to record all the env variables to be created
  env_var_pairs.append("CHRE_ENVS=(" + " ".join(sorted(all_env_names)) + ")")
  return env_var_pairs


def main():
  """Parses command-line arguments and orchestrates the script's execution."""
  check_dependencies(["cmake", "llvm-strip", "protoc", "pyenv", "xxd"])
  arg_parser = ArgumentParser(
      description="CHRE development environment setup",
  )
  arg_parser.add_argument(
      "platform_and_target", nargs="?", default=None, type=str
  )
  arg_parser.add_argument("-c", "--config", type=str)
  arg_parser.add_argument(
      "-i",
      "--interactive",
      action="store_true",
      help="Enable interactive mode to confirm default values.",
  )
  args = arg_parser.parse_args()
  config_data, config_file_path = _load_config(args.config)
  if config_file_path:
    os.environ["CHRE_CONFIG_DIR"] = os.path.dirname(config_file_path)

  if args.platform_and_target is None:
    supported_combinations = _get_supported_combinations(config_data)
    log_e(
        "A platform-target combination must be provided. Usage: chre_lunch"
        " <platform-target>"
    )
    fatal_error(f"Supported choices are: {supported_combinations}")

  fixed_env_map, target_envs_configs = _parse_platform_and_target_configs(
      config_data, args.platform_and_target
  )
  os.environ.update(fixed_env_map)

  env_vars_file = f"{fixed_env_map['CHRE_DEV_PATH']}/env_vars.txt"

  if os.path.exists(env_vars_file):
    with open(env_vars_file, "r") as f:
      log_w("The following env variables are previously entered:\n")
      predefined_vars = []
      for env_var_pair in f:
        _print_env_var_pair(env_var_pair)
        predefined_vars.append(env_var_pair.strip())
      answer = get_input_from_shell(
          "\nShall we keep using them? (Y/n):", color="yellow"
      )
      if not answer or answer.lower() == "y":
        print("\n".join(predefined_vars))
        return
      else:
        log_w("Overriding the existing dev environment settings...\n")

  env_var_pairs = _parse_env_variable_fields(
      target_envs_configs, fixed_env_map, args.interactive
  )

  # env_vars_file is only created after parsing env variables successfully.
  if not os.path.exists(env_vars_file):
    init_file(env_vars_file)

  with open(env_vars_file, "w") as f:
    f.write("\n".join(env_var_pairs))

  print("\n".join(env_var_pairs))
  return


if __name__ == "__main__":
  main()
