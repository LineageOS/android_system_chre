#!/usr/bin/env python3

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

"""Generates a CMakeLists.txt file from a build command's dry-run output.

This script executes a given build command (typically `make -n ...`) to capture
the compilation steps without actually building the project. It then parses this
output to extract source files, include directories, compiler flags, and macro
definitions. Finally, it writes a `CMakeLists.txt` file that can be used by IDEs
such as CLion or by CMake to generate  `compile_commands.json` for code
completion
and analysis.

The build command must include the dry-run option (`-n`) and must not specify
parallel execution greater than 1 (e.g., `-j2`, `-j4`).
"""

import argparse
import os
import re
import shlex
import subprocess
from shell_util import fatal_error, init_file


def _validate_build_command(command: str):
  """Validates that the build command is safe for parsing.

  Args:
    command: The build command string.
  """
  tokens = shlex.split(command)

  # Check for dry-run option
  if '-n' not in tokens:
    fatal_error("The build command must include the '-n' (dry-run) option.")

  # Check for parallel execution
  for token in tokens:
    if token.startswith('-j'):
      if token == '-j':
        # If -j is separate, the next token might be the number
        try:
          idx = tokens.index(token)
          if idx + 1 < len(tokens):
            jobs = int(tokens[idx + 1])
            if jobs > 1:
              fatal_error('Parallel execution (-j > 1) is not supported.')
          else:
            fatal_error('Parallel execution (unlimited jobs) is not supported.')
        except ValueError:
          # -j without number means unlimited
          fatal_error('Parallel execution (unlimited jobs) is not supported.')
      else:
        # -jX case
        try:
          jobs = int(token[2:])
          if jobs > 1:
            fatal_error(f'Parallel execution ({token}) is not supported.')
        except ValueError:
          pass  # Not a number, assuming something else starts with -j


def _write_header(output, project_name):
  """Writes the initial boilerplate for a CMakeLists.txt file.

  Args:
    output: A file-like object to write to.
    project_name: The name of the CMake project.
  """
  output.write('cmake_minimum_required(VERSION 3.12)\n')
  output.write(f'project({project_name})\n')
  output.write('set(CMAKE_C_COMPILER "/usr/bin/clang")\n')
  output.write('set(CMAKE_CXX_COMPILER "/usr/bin/clang++")\n')
  output.write('\n')


def _find_header_files(root_path):
  """Traverses a given path and its subfolders to find all *.h files.

  Args:
      root_path (str): The root directory to start the traversal from.

  Returns:
      list: A list containing the real paths of all *.h files found.
            Returns an empty list if no *.h files are found or if the
            root path is invalid.
  """
  header_files = []
  if not os.path.isdir(root_path):
    print(f"Error: '{root_path}' is not a valid directory.")
    return header_files

  for dirpath, dirnames, filenames in os.walk(root_path):
    for filename in filenames:
      if filename.endswith('.h'):
        real_path = os.path.realpath(os.path.join(dirpath, filename))
        header_files.append(real_path)
  return header_files


def _write_src_files(output, src_files):
  """Writes the list of source files to the CMakeLists.txt file.

  Args:
    output: A file-like object to write to.
    src_files: A list of source file paths.
  """
  if not src_files:
    return

  output.write('list (APPEND SOURCE_FILES\n    ')
  output.write('\n    '.join(src_files))
  output.write('\n)\n\n')


def _write_inc_paths(output, inc_paths):
  """Writes the include directories to the CMakeLists.txt file.

  Args:
    output: A file-like object to write to.
    inc_paths: A list of include directory paths.
  """
  output.write('include_directories(\n    ')
  output.write('\n    '.join(inc_paths))
  output.write('\n)\n\n')


def _write_flags(output, flags):
  """Writes the C and CXX compiler flags to the CMakeLists.txt file.

  Args:
    output: A file-like object to write to.
    flags: A dictionary of compiler flags.
  """
  for key, val in flags.items():
    output.write(f'set(CMAKE_C_FLAGS "${{CMAKE_C_FLAGS}} {key}{val}")\n')

  for key, val in flags.items():
    output.write(f'set(CMAKE_CXX_FLAGS "${{CMAKE_CXX_FLAGS}} {key}{val}")\n')
  output.write('\n')


def _write_macros(output, macros):
  """Writes the preprocessor macro definitions to the CMakeLists.txt file.

  Args:
    output: A file-like object to write to.
    macros: A dictionary of macro definitions.
  """
  for key, val in macros.items():
    output.write(f'add_compile_definitions({key}{val})\n')
  output.write('\n')


def _clean_term(term):
  """Cleans a single term from the compiler command line for parsing.

  Escapes parentheses.

  Args:
    term: A string representing a single argument from the command line.

  Returns:
    The cleaned term.
  """
  term = re.sub(r'\(', '\\(', term)
  term = re.sub(r'\)', '\\)', term)
  return term


def _convert_to_abs_path(path: str, cwd: str) -> str:
  """Converts a path to an absolute path if it isn't one already.

  Args:
    path: The path to convert.
    cwd: The current working directory to resolve relative paths against.

  Returns:
    The absolute path.
  """
  if not os.path.isabs(path):
    path = os.path.join(cwd, path)
  return os.path.realpath(path)


def _update_dir_stack(line: str, dir_stack: list[str]) -> str:
  """Updates the directory stack based on the build output line.

  Tracks directory changes via 'make: Entering directory' and
  'make: Leaving directory'.

  Args:
    line: A line from the build command's output.
    dir_stack: The directory stack.

  Returns:
    The current working directory.
  """
  current_cwd = dir_stack[-1] if dir_stack else os.getcwd()

  # Track current working directory changes via 'make: Entering directory'
  # which is standard when make runs with -w/--print-directory.
  entering_dir_match = re.search(
      r"make(?:\[\d+])?: Entering directory ['\"](.+)['\"]", line
  )
  if entering_dir_match:
    new_dir = _convert_to_abs_path(entering_dir_match.group(1), current_cwd)
    dir_stack.append(new_dir)
    print(f'Entering directory: {new_dir}', flush=True)
    return new_dir

  # Track leaving directory
  leaving_dir_match = re.search(
      r"make(?:\[\d+])?: Leaving directory ['\"](.+)['\"]", line
  )
  if leaving_dir_match:
    if len(dir_stack) > 1:
      left_dir = dir_stack.pop()
      print(f'Leaving directory: {left_dir}', flush=True)
    return dir_stack[-1] if dir_stack else os.getcwd()

  try:
    tokens = shlex.split(line)
  except ValueError:
    return current_cwd

  # Track current working directory changes via 'make -C', 'env -C', etc.
  # Note: GNU make allows -C dir, -Cdir, --directory=dir, --directory dir.
  # Multiple -C options are cumulative.
  is_wrapper = any(
      t == 'make'
      or t == 'gmake'
      or t == 'env'
      or t.endswith('/make')
      or t.endswith('/gmake')
      or t.endswith('/env')
      for t in tokens
  )

  updated_cwd = current_cwd
  if is_wrapper:
    i = 0
    while i < len(tokens):
      token = tokens[i]
      if token == '-C' or token == '--directory':
        if i + 1 < len(tokens):
          updated_cwd = _convert_to_abs_path(tokens[i + 1], updated_cwd)
          print(f'Updated current_cwd to: {updated_cwd}', flush=True)
          i += 1
      elif token.startswith('-C'):
        updated_cwd = _convert_to_abs_path(token[2:], updated_cwd)
        print(f'Updated current_cwd to: {updated_cwd}', flush=True)
      elif token.startswith('--directory='):
        updated_cwd = _convert_to_abs_path(
            token[len('--directory=') :], updated_cwd
        )
        print(f'Updated current_cwd to: {updated_cwd}', flush=True)
      i += 1

  if updated_cwd != current_cwd:
    dir_stack.append(updated_cwd)
  return dir_stack[-1] if dir_stack else os.getcwd()


def _parse_compilation_output(args: argparse.Namespace, result: list[str]):
  """Parses the build command output and generates the CMakeLists.txt file.

  Iterates through each line of the build output, extracting source files,
  include paths, compiler flags, and macro definitions. It then calls the
  appropriate writer functions to construct the final CMakeLists.txt file.

  Args:
    args: The parsed command-line arguments.
    result: A list of strings, where each string is a line from the build
      command's stdout.
  """
  src_files = []
  inc_paths = set()
  macros = dict()
  flags = dict()
  output_file = os.path.join(args.output_path, 'CMakeLists.txt')
  dir_stack = [args.src_path]

  init_file(output_file)
  with open(output_file, 'w') as output:
    _write_header(output, args.project_name)
    for line in result:
      current_cwd = _update_dir_stack(line, dir_stack)

      # Only parse lines that appear to be compilation commands for a source file.
      if ' -c ' not in line or ' -o ' not in line:
        continue

      try:
        tokens = shlex.split(line)
      except ValueError:
        continue

      src_file_path = None
      i = 0
      while i < len(tokens):
        token = tokens[i]

        # Source file
        if not token.startswith('-') and re.search(r'\.(c|cc|cpp)$', token):
          src_file_path = token

        # include paths
        elif token == '-I' or token == '-isystem':
          if i + 1 < len(tokens):
            inc_paths.add(
                '"{}"'.format(_convert_to_abs_path(tokens[i + 1], current_cwd))
            )
            i += 1
        elif token.startswith('-I'):
          inc_paths.add(
              '"{}"'.format(_convert_to_abs_path(token[2:], current_cwd))
          )
        elif token.startswith('-isystem'):
          inc_paths.add(
              '"{}"'.format(_convert_to_abs_path(token[8:], current_cwd))
          )

        # macros and flags
        elif token.startswith('-D') or token.startswith('-W'):
          cleaned_token = _clean_term(token)
          idx = cleaned_token.find('=')
          key, val = (
              (cleaned_token[:idx], cleaned_token[idx:])
              if idx > 0
              else (cleaned_token, '')
          )
          if cleaned_token.startswith('-D'):
            macros[key[2:]] = val
          elif cleaned_token.startswith('-W'):
            flags[key] = val
        i += 1

      if not src_file_path:
        continue
      else:
        print('Found src file: ' + src_file_path, flush=True)

      # Add source files
      src_files.append(_convert_to_abs_path(src_file_path, current_cwd))
    header_files = _find_header_files(args.src_path)
    print(f'{len(header_files)} header files')
    _write_src_files(output, sorted(header_files))
    _write_src_files(output, sorted(src_files))
    print(f'{len(src_files)} src files')

    _write_inc_paths(output, sorted(inc_paths))
    print(f'{len(inc_paths)} inc paths')

    _write_macros(output, macros)
    print(f'{len(macros)} macros')

    _write_flags(output, flags)
    print(f'{len(flags)} flags')

    output.write(
        f'add_executable({os.environ.get("CHRE_PLATFORM")}_{os.environ.get("CHRE_TARGET_TYPE")}'
        ' ${SOURCE_FILES})\n'
    )


def main():
  """The main entry point for the script.

  Parses command-line arguments, executes the provided build command, and
  initiates the parsing of the output to generate the CMakeLists.txt file.
  """
  arg_parser = argparse.ArgumentParser(description='CMakeLists.txt generator')

  cwd = os.getcwd()
  print('cwd: ' + cwd)

  arg_parser.add_argument(
      '-c', '--command', type=str, required=True, help='The building command'
  )

  arg_parser.add_argument(
      '-s',
      '--src_path',
      type=str,
      default=cwd,
      help='The dir where the source files are located',
  )

  arg_parser.add_argument(
      '-o',
      '--output_path',
      type=str,
      default='',
      help='The path to store the CMakeLists.txt generated',
  )

  arg_parser.add_argument(
      '-n',
      '--project_name',
      type=str,
      default=cwd,
      help='The name of the project',
  )

  args = arg_parser.parse_args()
  args.output_path = os.path.expanduser(args.output_path)
  args.src_path = os.path.expanduser(args.src_path)

  _validate_build_command(args.command)

  print(args)
  print('command: ' + args.command)

  # build path should always be the current path. Source path can be different.
  result = subprocess.run(
      shlex.split(args.command),
      stdout=subprocess.PIPE,
      cwd=os.getcwd(),
      stderr=subprocess.STDOUT,
      check=True,
      text=True,
  ).stdout.splitlines()

  if result[-1].endswith('Stop.'):
    fatal_error(f'Failed to build the project:\n{result[-1]}\n')

  _parse_compilation_output(args, result)

  return


if __name__ == '__main__':
  main()
