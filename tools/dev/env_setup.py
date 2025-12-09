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

When run with a platform-target combination (e.g., `python env_setup.py -p <platform>-<target>`),
it interactively prompts the user for necessary environment variables based on
the env_config.json file. It then generates a series of <name>-<value> pairs that can be
exported by the shell to configure the environment. If a `default_action` is provided with a
default value together the action will be incurred before exporting the default value.

The result is printed to stdout and piped into the shell to set the environment variables.
"""
import argparse
import json
import os
import re
import subprocess
import sys
from typing import Any

from shell_util import log_i, log_w, fatal_error, check_dependencies


def _print_env_var_pair(env_var: str):
  env_name, env_value = re.match(r"(.*)=(.*)", env_var).groups()
  print(f"\033[32m{env_name}\033[0m = {env_value}", file=sys.stderr)


def _get_canonical_path(path: str):
  return os.path.expanduser(os.path.expandvars(path))


def _init_file(file_path_str: str):
  from pathlib import Path

  # Define the full path to your desired file
  file_path = Path(file_path_str)

  # Create the parent directories
  # parents=True: creates all missing parent folders (like mkdir -p)
  # exist_ok=True: doesn't raise an error if the directory already exists
  file_path.parent.mkdir(parents=True, exist_ok=True)

  # Create the file itself
  # This creates an empty file if it doesn't exist.
  file_path.touch()


class _CustomArgumentParser(argparse.ArgumentParser):
  """A custom argument parser to override the default error handling."""

  def error(self, message):
    """Overrides the default error method to prevent printing errors to console"""
    fatal_error(
      f"an argument in the format of <platform_name-target_name> must be provided.\n{message}"
    )


def _get_input_from_shell(prompt: str, color: str = None) -> str:
  """Prompts the user for input from the shell and returns the response.

  Args:
    prompt: The prompt message to display to the user.

  Returns:
    The string entered by the user.
  """
  if color == 'green':
    prompt = f"\033[32m{prompt}\033[0m"
  elif color == 'yellow':
    prompt = f"\033[33m{prompt}\033[0m"
  elif color == 'red':
    prompt = f"\033[31m{prompt}\033[0m"
  else:
    pass
  print(prompt, end="", file=sys.stderr, flush=True)
  return input()


def _action_clone_repo(url: str, branch: str, dest: str) -> None:
  """Clones a Git repository from 'url' with a specific 'branch' into 'dest'.

  Args:
      url (str): The URL of the Git repository.
      branch (str): The branch to clone.
      dest (str): The destination directory to clone the repository into.
  """
  if os.path.exists(dest):
    answer = _get_input_from_shell(
      f"{dest} already exists. Shall we override it? (y/N):", color="yellow")
    if not answer or answer.lower() == 'n':
      log_i(f"Skipping clone operation for {dest}")
      return
    log_i(f"Removing existing directory: {dest}")
    subprocess.run(['rm', '-rf', dest], check=True)

  try:
    # The --single-branch option fetches only the specified branch, saving time and space.
    subprocess.run(
      ['git', 'clone', '--branch', branch, '--single-branch', url, dest],
      check=True
    )
    log_i(f"Successfully cloned {url} (branch: {branch}) into {dest}")
  except subprocess.CalledProcessError:
    fatal_error('Error cloning repository')
  except FileNotFoundError:
    fatal_error(
      "Error: 'git' command not found. Please ensure Git is installed and in your PATH.")


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

  def _check_single_value(value, v_type):
    _expanded_value = _get_canonical_path(value)
    if v_type == "path":
      if not os.path.isdir(_expanded_value):
        fatal_error(f"Path '{value}' does not exist.")
    elif v_type == "file":
      if not os.path.isfile(_expanded_value):
        fatal_error(f"File '{value}' does not exist.")
    elif v_type == "value":
      if not re.match(r"^[\w\-]+$", value, flags=re.ASCII):
        fatal_error(
          f"Invalid value '{value}'. Only dash and characters in [a-zA-Z0-9_] are allowed.")
    else:
      fatal_error(f"Unknown value type '{v_type}' for value '{value}'")
    return _expanded_value

  matched_list_type = re.match(r"^list\[(.*)]$", env_type)
  if matched_list_type:
    expanded_value = ":".join(
      _check_single_value(v, matched_list_type.group(1)) for v in env_value.split())
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
    config_file: The file containing the config json file. When it's not provided the path to the
    directory containing this script must be set in the CHRE_DEV_SCRIPT_PATH environment variable
    so that the default config file can be retrieved.

  Returns:
    A dictionary containing the parsed JSON configuration data.
  """

  if config_file is None:
    if not os.getenv("CHRE_DEV_SCRIPT_PATH"):
      fatal_error("CHRE_DEV_SCRIPT_PATH must be set before calling this script")
    config_file = os.path.join(os.getenv("CHRE_DEV_SCRIPT_PATH"), "env_config.json")
  else:
    config_file = _get_canonical_path(config_file)

  try:
    with open(config_file, "r") as f:
      return json.load(f)
  except FileNotFoundError:
    fatal_error(f"Error: Config file '{config_file}' not found")
  except json.JSONDecodeError as e:
    fatal_error(f"Error: Invalid JSON format in '{config_file}'\n{e}")


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
  supported_combinations = [
    f"{entry.get('platform')}-{target.get('name')}"
    for entry in config_data
    for target in entry.get("targets", [])
  ]

  if not platform_and_target or not re.match(r"^\w+-\w+$", platform_and_target):
    fatal_error(
      "platform and target must be in the format of <platform_name-target_name>\n"
      f"Supported choices are: {supported_combinations}"
    )

  platform_name, target_name = platform_and_target.split("-")
  for platform in config_data:
    try:
      if platform["platform"] != platform_name:
        continue
      for target in platform["targets"]:
        if target["name"] != target_name:
          continue
        env_map = {"CHRE_PLATFORM": platform_name,
                   "CHRE_TARGET_TYPE": target_name,
                   "CHRE_BUILD_TARGET": target["build_target"],
                   "CHRE_DEV_PATH": _get_canonical_path(f"~/.chre_dev/{platform_name}-{target_name}")
                   }
        if platform.get("python_version"):
          env_map["CHRE_PYTHON_VERSION"] = platform.get("python_version")
        if target.get("install_location"):
          env_map["TARGET_INSTALL_LOCATION"] = target["install_location"]
        envs = platform.get("common_env_variables", []) + target.get(
          "env_variables", [])
        return env_map, envs
    except KeyError as e:
      fatal_error(
        f"Malformed config for {platform_name}-{target_name}: '{e.args[0]}' field is not defined")

  fatal_error(
    f"No platform-target combination found for '{platform_name}-{target_name}'\n"
    f"Supported choices are: {supported_combinations}"
  )


def _parse_env_variable_fields(env_vars, predefined_envs):
  """Interactively prompts for and processes environment variables.

  Iterates through a list of environment variable definitions, prompts the user
  for values, validates them, and generates <name>=<value> pairs. If a
  default value is provided and no user input is given, the default value will
  be used. When a default action is provided, it will be executed if the default
   value is used.

  Args:
    predefined_envs: A dict of environment variables pre-defined for the platform
     and the target.
    env_vars: A list of dictionaries, where each dictionary defines an
      environment variable customizable by the user.

  Returns:
    A list of <name>=<value> pairs representing the environment variables.
  """
  all_env_names = set(predefined_envs)
  env_var_pairs = [f"{k}={v}" for k, v in predefined_envs.items()]
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
        prompt = "{} ({})".format(env_name, default_value if default_value else "EMPTY")
      print("\n{}".format(env_var.get("description", "")), file=sys.stderr)
      user_entered_value = _get_input_from_shell(f"{prompt}: ").strip()

      if user_entered_value:
        expanded_value = _assert_and_expand_env_variable(env_name, env_var["type"],
                                                         user_entered_value)
        env_var_pairs.append(f"{env_name}={expanded_value}")
        # User entered a value, skip the default action
        continue

      # Either user has to enter a value or default_value must be provided
      if default_value is None:
        fatal_error(f"{env_name} must be provided. Please try it again")

      if default_action:
        _run_action(default_action)
      # Default action is supposed to have made the default value valid
      expanded_value = _assert_and_expand_env_variable(env_name, env_var["type"],
                                                       default_value)
      env_var_pairs.append(f"{env_name}={expanded_value}")
    except KeyError as e:
      fatal_error(f"The environment variable doesn't have the field '{e.args[0]}'")

  # Create CHRE_ENVS to record all the env variables to be created
  env_var_pairs.append("CHRE_ENVS=(" + ' '.join(sorted(all_env_names)) + ")")
  return env_var_pairs



def main():
  """Parses command-line arguments and orchestrates the script's execution."""
  check_dependencies(['cmake', 'protoc', 'pyenv', 'xxd'])
  arg_parser = _CustomArgumentParser(
    description="CHRE development environment setup",
  )
  arg_parser.add_argument(
    "platform_and_target", default=None,
    type=str
  )
  arg_parser.add_argument(
    "-c", "--config", type=str
  )

  args = arg_parser.parse_args()
  config_data = _load_config(args.config)
  fixed_env_map, target_envs_configs = _parse_platform_and_target_configs(config_data,
                                                                          args.platform_and_target)
  os.environ.update(fixed_env_map)

  env_vars_file = f"{fixed_env_map['CHRE_DEV_PATH']}/env_vars.txt"

  if os.path.exists(env_vars_file):
    with open(env_vars_file, "r") as f:
      log_w("The following env variables are previously entered:\n")
      predefined_vars = []
      for env_var_pair in f:
        _print_env_var_pair(env_var_pair)
        predefined_vars.append(env_var_pair.strip())
      answer = _get_input_from_shell("\nShall we keep using them? (Y/n):", color="yellow")
      if not answer or answer.lower() == 'y':
        print("\n".join(predefined_vars))
        return
      else:
        log_w("Overriding the existing dev environment settings...\n")

  env_var_pairs = _parse_env_variable_fields(target_envs_configs, fixed_env_map)

  # env_vars_file is only created after parsing env variables successfully.
  if not os.path.exists(env_vars_file):
    _init_file(env_vars_file)

  with open(env_vars_file, "w") as f:
    f.write("\n".join(env_var_pairs))

  print("\n".join(env_var_pairs))
  return


if __name__ == "__main__":
  main()
