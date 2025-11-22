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

import glob
import os
import re
import shutil
import subprocess
import sys
import time


def check_dependencies(required_programs: list[str]):
  """Checks if all required command-line tools are installed.

  If a tool is missing, it prompts the user to install them.
  """
  missing_programs = []

  for program in required_programs:
    if shutil.which(program) is None:
      missing_programs.append(program)

  if missing_programs:
    log_i(
        "\nSome required packages are missing but can be installed by running"
        " the following command(s):\n"
    )
    for program in missing_programs:
      log_i(f"  sudo apt install {program}")

    answer = get_input_from_shell(
        "\nShall we install them for you? (Y/n): "
    ).lower()
    if answer not in ("y", ""):
      log_e("Please install them and/or add them to your PATH")
      sys.exit(1)

    for program in missing_programs:
      try:
        log_i(f"Installing {program}...")
        result = subprocess.run(
            ["sudo", "apt", "install", "-y", program],
            capture_output=True,
            text=True,
            check=True,
        )
        log_i(f"Successfully installed {program}")
      except FileNotFoundError:
        fatal_error(
            "Error: 'sudo' or 'apt' command not found. Please ensure they are"
            " installed and in your PATH."
        )
        sys.exit(1)
      except subprocess.CalledProcessError as e:
        log_e(f"Failed to install {program}.")
        log_e(f"Stderr: {e.stderr}")
        log_e("\nPlease install it manually and try again.")
        sys.exit(1)

  # Final check to be sure.
  missing_programs = []
  for program in required_programs:
    if shutil.which(program) is None:
      missing_programs.append(program)

  if missing_programs:
    log_e("Even after installation, the following programs are not found:")
    log_e("  " + " ".join(missing_programs))
    fatal_error("Please check your PATH or install them manually.")


def init_file(file_path_str: str):
  if os.path.exists(file_path_str):
    return

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


def find_unique_file(file_pattern: str):
  files = glob.glob(file_pattern)
  if not files:
    fatal_error(f"No file found matching {file_pattern}")
  if len(files) > 1:
    fatal_error(f"Multiple files found matching {file_pattern}: {files}")
  return files[0]


def fatal_error(message: str):
  """Prints an error message in red to stderr and exits the script."""
  print(f"\033[31m{message}\033[0m\n", file=sys.stderr)
  sys.exit(1)


def warning(message: str):
  """Prints a warning message flag and message in yellow to stderr."""
  print(f"\033[33m")
  print("▗▖ ▗▖ ▗▄▖ ▗▄▄▖ ▗▖  ▗▖▗▄▄▄▖▗▖  ▗▖ ▗▄▄▖")
  print("▐▌ ▐▌▐▌ ▐▌▐▌ ▐▌▐▛▚▖▐▌  █  ▐▛▚▖▐▌▐▌   ")
  print("▐▌ ▐▌▐▛▀▜▌▐▛▀▚▖▐▌ ▝▜▌  █  ▐▌ ▝▜▌▐▌▝▜▌")
  print("▐▙█▟▌▐▌ ▐▌▐▌ ▐▌▐▌  ▐▌▗▄█▄▖▐▌  ▐▌▝▚▄▞▘")
  print("")
  print(f"{message}\033[0m\n", file=sys.stderr)


def success(message: str):
  print(f"\033[32m")
  print("▗▄▄▖  ▗▄▖  ▗▄▄▖ ▗▄▄▖")
  print("▐▌ ▐▌▐▌ ▐▌▐▌   ▐▌   ")
  print("▐▛▀▘ ▐▛▀▜▌ ▝▀▚▖ ▝▀▚▖")
  print("▐▌   ▐▌ ▐▌▗▄▄▞▘▗▄▄▞▘")
  print("")
  print(f"{message}\033[0m")


def log_e(message: str):
  """Prints an error log in red to stderr."""
  print(f"\033[31m{message}\033[0m", file=sys.stderr)


def log_w(message: str):
  """Prints a warning log in yellow to stderr."""
  print(f"\033[33m{message}\033[0m", file=sys.stderr)


def log_i(message: str):
  """Prints a log to stderr."""
  print(f"{message}", file=sys.stderr)


def get_input_from_shell(prompt: str, color: str = None) -> str:
  """Prompts the user for input from the shell and returns the response.

  Args:
    prompt: The prompt message to display to the user.
    color: The color of the prompt. Typically green indicates something
      accomplished successfully, red means a blocking error and yellow indicates
      either a non-blocking error or some important information worth notice.

  Returns:
    The string entered by the user.
  """
  if color == "green":
    prompt = f"\033[32m{prompt}\033[0m"
  elif color == "yellow":
    prompt = f"\033[33m{prompt}\033[0m"
  elif color == "red":
    prompt = f"\033[31m{prompt}\033[0m"
  else:
    pass
  print(prompt, end="", file=sys.stderr, flush=True)
  return input().strip()


def not_have(pattern: str):
  return lambda output: not re.search(pattern, output, flags=re.IGNORECASE)


def has(pattern: str):
  return lambda output: re.search(pattern, output, flags=re.IGNORECASE)


class ShellSession:
  SUCCESS = "\033[32m[OK]\033[0m"
  FAILURE = "\033[31m[FAILED]\033[0m"

  def __init__(self, shell_cmd="bash", cmd_width=80, env=None):
    # Move pexpect to local import as ShellSession is the only place using it.
    import pexpect

    if env is None:
      env = os.environ.copy()
    self.cmd_width = cmd_width  # Used for pretty printing
    rows, cols = (
        24,
        180,
    )  # large enough cols to make sure a single line output fits
    self.session = pexpect.spawn(shell_cmd, env=env, dimensions=(rows, cols))
    self.session.expect(r"(.*)[$#>] ")
    self.prompt = re.escape(self.session.match.group(1).decode())

  # When the keyword argument timeout is -1 (default), then TIMEOUT exception
  # will be raised after the default value specified by the class timeout
  # attribute ( 30s). When it is None, TIMEOUT exception will not be raised and may
  # block indefinitely until match.
  def run(
      self, cmd, is_successful=None, timeout=None, show_output=False
  ) -> str:
    print(cmd.ljust(self.cmd_width), end="", flush=True)
    start_time = time.perf_counter()
    output = self._execute(cmd, timeout)
    has_result = is_successful is None or is_successful(output)
    print(
        "{:<20} {:5.2f}s".format(
            ShellSession.SUCCESS if has_result else ShellSession.FAILURE,
            time.perf_counter() - start_time,
        )
    )

    if not has_result or show_output:
      print("-" * 50)
      print(output if output else "\n**NO OUTPUT**\n", end="")
      print("-" * 50 + "\n")

    if not has_result:
      exit(-1)

    return output

  def run_until_success(
      self, cmd, is_successful, retry_interval=1, timeout=20, show_output=False
  ):
    print(cmd.ljust(self.cmd_width), end="", flush=True)
    start_time = time.perf_counter()
    output = self._execute(cmd, timeout=timeout)
    while not is_successful(output):
      time.sleep(retry_interval)
      output = self._execute(cmd, timeout)
    print(
        "{:<20} {:.2f}s".format(
            ShellSession.SUCCESS, time.perf_counter() - start_time
        )
    )
    if show_output:
      print("-" * 50)
      print(output, end="")
      print("-" * 50 + "\n")
    return output

  def _execute(self, cmd, timeout):
    self.session.sendline(cmd)
    self.session.expect(self.prompt, timeout=timeout)
    return self.session.before.decode().split("\r\n", maxsplit=1)[1]
