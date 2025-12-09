#!/usr/bin/python3

import re
import sys
import time

import shutil


def check_dependencies(required_programs: list[str]):
  """
  Checks if all required command-line tools are installed.
  If a tool is missing, it prints an error and exits the script.
  """
  missing_programs = []

  for program in required_programs:
    if shutil.which(program) is None:
      missing_programs.append(program)

  if missing_programs:
    log_e("ERROR: The following required programs are not installed or not in your PATH:\n")
    log_e("  " + " ".join(missing_programs))

    log_e("\nPlease install them and/or add them to your PATH")
    sys.exit(1)  # Exit with a non-zero status code to indicate an error

  log_i("All required programs are found")


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


def get_input_from_shell(prompt: str) -> str:
  """Prompts the user for input from the shell and returns the response.

  Args:
    prompt: The prompt message to display to the user.

  Returns:
    The string entered by the user.
  """
  print(prompt, end="", file=sys.stderr, flush=True)
  return input()


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
      env = {"SCRIPT_ONLY": "yes"}
    self.cmd_width = cmd_width  # Used for pretty printing
    self.session = pexpect.spawn(shell_cmd, env=env)
    self.session.expect(r"(.*)[$#>] ")
    self.prompt = self.session.match.group(1).decode()

  # When the keyword argument timeout is -1 (default), then TIMEOUT exception
  # will be raised after the default value specified by the class timeout
  # attribute ( 30s). When it is None, TIMEOUT exception will not be raised and may
  # block indefinitely until match.
  def run(
      self, cmd, is_successful=None, timeout=None, show_output=False
  ) -> str:
    print(cmd.ljust(90), end="", flush=True)
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
    print(f"{cmd:<{self.cmd_width}}", end="", flush=True)
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
