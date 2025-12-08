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

"""Checks a nanoapp's shared object file for unresolvable external symbols.

This script inspects a given nanoapp .so file to ensure that it only references
symbols that are explicitly allowed. It builds a comprehensive list of allowed
symbols by:
1.  Parsing standard CHRE API header files.
2.  Parsing platform-specific extension headers.
3.  Reading platform-specific lists of exported symbols.
4.  Reading an optional user-provided file of additional allowed symbols.

It then compares the nanoapp's undefined symbols (extracted using the corresponding
elf reader of the target) against this allowed list and reports any discrepancies,
exiting with an error if unresolvable symbols are found.
"""

import argparse
import os
import re
import subprocess
import warnings
from os import listdir
from os.path import join

import pyclibrary

from shell_util import warning, log_w, success

# The number of rows to discard from the output of the elf reader
NUM_ROWS_TO_DISCARD = 4


def _get_env_list(env_var_name: str):
  return os.getenv(env_var_name).split(":") if env_var_name in os.environ else []


def _get_known_exported_functions() -> list:
  """Extracts function names from ADD_EXPORTED_SYMBOL or ADD_EXPORTED_C_SYMBOL macros.

  Returns:
    A list of unique exported function names.
  """
  exported_names = []

  # Regex to match both macro forms:
  # 1. ADD_EXPORTED_SYMBOL(internal_name, "external_name") - Captures 'external_name'
  # 2. ADD_EXPORTED_C_SYMBOL(function_name) - Captures 'function_name'
  regex = r'ADD_EXPORTED_SYMBOL\s*\([^,]+,\s*"([^"]+)"\)|' \
          r'ADD_EXPORTED_C_SYMBOL\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)'

  exported_macro_files = _get_env_list("EXPORTED_MACRO_FILES")
  for file in exported_macro_files:
    symbols = []
    with open(file, 'r') as f:
      for line in f:
        matches = re.search(regex, line)
        if matches:
          name_from_symbol, name_from_c_symbol = matches.groups()
          symbols.append(name_from_symbol if name_from_symbol else name_from_c_symbol)

    print(f"{len(symbols)} dynamic symbols found in {file}")
    exported_names.extend(symbols)
  return exported_names


def _get_allowed_symbols() -> list:
  """Gathers all allowed symbols from CHRE API and platform sources.

  This function aggregates symbols from several sources:
  - Standard CHRE API header files.
  - Platform-specific extension header files defined in PLATFORM_INFO.
  - Symbols exported via macros, found by _get_known_exported_functions.
  - A platform-specific symbol list file (e.g., dl_base_symbols.lst).

  Returns:
      A list of all allowed symbol names.
  """
  chre_api_path = f"{os.environ['ANDROID_BUILD_TOP']}/system/chre/chre_api/include/chre_api"
  header_files = []
  for f in listdir(chre_api_path + '/chre'):
    header_files.append(join(chre_api_path + '/chre', f))
  header_files.append(join(chre_api_path, 'chre.h'))
  header_files.extend(_get_env_list('EXTERNAL_SYMBOL_DECLARATIONS'))
  fnames = []

  # suppress warnings from pyclibrary parsing headers
  warnings.simplefilter('ignore', SyntaxWarning)
  try:
    for h in header_files:
      # pyclibrary cannot find functions that
      #  - have a vararg parameter
      #  - have a trailing macro
      # so removing them from the header file before parsing
      pyc_parser = pyclibrary.CParser(h, replace={r', \.\.\.': '', r'[A-Z_]+;': ';'})
      fnames.extend(list(pyc_parser.defs['functions'].keys()))
  finally:
    warnings.simplefilter('default', SyntaxWarning)

  print(f"{len(fnames)} dynamic symbols found in chre header files")

  fnames.extend(_get_known_exported_functions())

  lst_files = _get_env_list("EXTERNAL_SYMBOL_LISTS")
  for lst_file in lst_files:
    with open(lst_file) as f:
      platform_external_symbols = [s.strip() for s in f.readlines()]
      print(
        f"{len(platform_external_symbols)} dynamic symbols found in {lst_file}")
      fnames.extend(platform_external_symbols)
  return fnames


def _get_symbols_from_nanoapp(file_name: str) -> list:
  """Extracts undefined dynamic symbols from a nanoapp .so file.

  Uses the ELF reader specified by the CHRE_TARGET_ELF_READER environment
  variable to inspect the dynamic symbol table of the given file.

  Args:
    file_name: The path to the nanoapp .so file.

  Returns:
    A list of undefined symbol names found in the nanoapp.
  """
  elf_reader = os.environ['CHRE_TARGET_ELF_READER']
  readelf_cmd = f"{elf_reader} --dyn-syms --wide {file_name}"
  out = subprocess.check_output(readelf_cmd.split(), text=True)

  symbols = []
  for line in out.splitlines()[NUM_ROWS_TO_DISCARD:]:
    words = line.split()
    idx_type, symbol_name = words[-2:]
    if "UND" == idx_type:
      symbols.append(symbol_name)
  print(f"{len(symbols)} dynamic symbols found in {file_name}")
  return symbols


def _get_allowed_symbols_from_file(filename: str) -> list:
  """Reads a list of allowed symbols from a text file.

  Each line in the file is treated as a single symbol.

  Args:
    filename: The path to the file containing allowed symbols.

  Returns:
    A list of symbol names.
  """
  with open(filename) as f:
    return [s.strip() for s in f.readlines()]


def _disallowed_symbols(observed_symbols: list, allowed_symbols: list) -> list:
  """Compares observed symbols against a list of allowed symbols.

  Calculates the set difference between observed and allowed symbols. It also
  handles basic wildcard matching for allowed symbols that end with '*'.

  Args:
    observed_symbols: A list of symbols found in the nanoapp.
    allowed_symbols: A list of all allowed symbols.

  Returns:
    A list of symbols that are observed but not allowed.
  """
  diff_list = list(set(observed_symbols) - set(allowed_symbols))
  # See if any of the wildcard ending allowed symbols will cause some more of
  # the observed symbols to be allowed
  for allowed in allowed_symbols:
    if '*' in allowed:
      prefix = allowed[:allowed.find('*')]
      diff_list = [sym for sym in diff_list if not sym.startswith(prefix)]
  return diff_list


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
    description="Check nanoapp for illegal" +
                " symbols in file provided by --nanoapp argument")
  parser.add_argument('--nanoapp', type=str, help="nanoapp file", required=True)
  parser.add_argument('--allowed_symbols_file', type=str,
                      help="a file containing a list of allowed symbols", required=False)
  args = parser.parse_args()
  nanoapp_filename = args.nanoapp

  print("\n------- Checking unresolvable external symbols -------")

  specific_allowed_symbols_file = args.allowed_symbols_file

  observed_symbols = _get_symbols_from_nanoapp(nanoapp_filename)
  allowed_symbols = _get_allowed_symbols()

  if specific_allowed_symbols_file is not None:
    allowed_symbols += _get_allowed_symbols_from_file(
      specific_allowed_symbols_file)

  disallowed_symbols_list = _disallowed_symbols(observed_symbols,
                                                allowed_symbols)

  if len(disallowed_symbols_list) > 0:
    warning(f"{len(disallowed_symbols_list)} unresolvable symbol(s) found:\n")
    for sym in disallowed_symbols_list:
      log_w(f" - {sym}")
  else:
    success(f"All the dynamic symbols are resolvable!")
