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

"""Displays the metadata of nanoapp .so or .napp files.

Usage: python3 napp_xray.py path_or_file
"""

import argparse
import ctypes
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile


# From system/chre/platform/shared/include/chre/platform/shared/nanoapp_support_lib_dso.h
class NanoappInfoFlags(ctypes.LittleEndianStructure):
  _fields_ = [
      ("isSystemNanoapp", ctypes.c_uint8, 1),
      ("isTcmNanoapp", ctypes.c_uint8, 1),
      ("reservedFlags", ctypes.c_uint8, 6),
  ]


# As interpreted in system/chre/chre_api/include/chre_api/chre/version.h
class NanoappVersion(ctypes.LittleEndianStructure):
  _fields_ = [
      ("patch", ctypes.c_uint16),
      ("minor", ctypes.c_uint8),
      ("major", ctypes.c_uint8),
  ]

  def __str__(self):
    return f"{self.major}.{self.minor}.{self.patch}"


# From system/chre/platform/shared/include/chre/platform/shared/nanoapp_support_lib_dso.h
class ChreNslNanoappInfo(ctypes.LittleEndianStructure):
  _fields_ = [
      ("magic", ctypes.c_uint32),
      ("structMinorVersion", ctypes.c_uint8),
      ("flags", NanoappInfoFlags),
      ("reserved", ctypes.c_uint8),
      ("targetApiVersion", ctypes.c_uint32),
      ("vendor", ctypes.c_uint32),
      ("name", ctypes.c_uint32),
      ("appId", ctypes.c_uint64),
      ("appVersion", NanoappVersion),
      # entryPoints struct starts
      ("start", ctypes.c_uint32),
      ("handleEvent", ctypes.c_uint32),
      ("end", ctypes.c_uint32),
      # entryPoints struct ends
      ("appVersionString", ctypes.c_uint32),
      ("appPermissions", ctypes.c_uint32),
      ("minChreApiVersion", ctypes.c_uint32),
      ("requestedThreadPriority", ctypes.c_int8),
  ]


def _find_elf_offset(file_path: Path) -> int | None:
  """Searches for the ELF magic number and returns its offset."""
  elf_magic = b"\x7fELF"
  # Search within the first 8KB for the header
  search_limit = 8192
  try:
    with open(file_path, "rb") as f:
      data = f.read(search_limit)
      offset = data.find(elf_magic)
      if offset != -1:
        return offset
  except IOError:
    return None
  return None


def get_padded_mem_size(file_path: Path) -> int | None:
  """Reads an ELF file's program headers and calculates the total padded memory size."""
  reader = os.environ.get("CHRE_TARGET_ELF_READER", "readelf")

  cmd = [reader, "-l", str(file_path)]
  try:
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        check=False,
    )

    if result.returncode != 0:
      # This handles cases where the file is not a valid ELF file.
      return None

    total = 0
    for line in result.stdout.splitlines():
      if "LOAD" in line:
        parts = line.split()
        if len(parts) < 6:
          continue

        try:
          # readelf output is in hex.
          memsize = int(parts[5], 16)
          align = int(parts[-1], 16)

          # Calculate the size padded up to the next multiple of Align.
          # This is the integer-only "round up to next multiple" formula.
          quotient = (memsize + align - 1) // align
          padded = align * quotient
          total += padded
        except (ValueError, IndexError):
          # Ignore lines that are malformed
          continue

    return total if total > 0 else None

  except Exception as e:
    print(f"Error running {reader}: {e}", file=sys.stderr)
    return None


def _get_elf_class(file_path: Path) -> int | None:
  """Returns 32 or 64 based on ELF class, or None on error."""
  reader = os.environ.get("CHRE_TARGET_ELF_READER", "readelf")
  cmd = [reader, "-h", str(file_path)]
  try:
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        text=True,
        check=False,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode != 0:
      return None
    for line in result.stdout.splitlines():
      if "Class:" in line:
        if "ELF64" in line:
          return 64
        if "ELF32" in line:
          return 32
  except Exception:
    return None
  return None


def _get_symbol(file_path: Path, symbol_name: str) -> dict | None:
  """Finds a symbol in an ELF file and returns its details."""
  reader = os.environ.get("CHRE_TARGET_ELF_READER", "readelf")
  cmd = [reader, "-sW", str(file_path)]
  try:
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        text=True,
        check=False,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode != 0:
      return None

    for line in result.stdout.splitlines():
      parts = line.split()
      if len(parts) > 7 and parts[-1] == symbol_name:
        return {
            "value": int(parts[1], 16),
            "size": int(parts[2]),
            "section": parts[6],
        }
  except Exception:
    return None
  return None


def _get_sections(file_path: Path) -> dict | None:
  """Gets all section headers from an ELF file."""
  reader = os.environ.get("CHRE_TARGET_ELF_READER", "readelf")
  cmd = [reader, "-SW", str(file_path)]
  try:
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        text=True,
        check=False,
        stderr=subprocess.DEVNULL,
    )
    if result.returncode != 0:
      return None

    sections = {}
    section_header_re = re.compile(
        r"\[\s*(\d+)]\s+\S+\s+\S+\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)"
    )
    for line in result.stdout.splitlines():
      match = section_header_re.match(line.strip())
      if match:
        groups = match.groups()
        index = groups[0]
        sections[index] = {
            "addr": int(groups[1], 16),
            "offset": int(groups[2], 16),
            "size": int(groups[3], 16),
        }
  except Exception:
    return None
  return sections


def _read_string_at_vaddr(f, vaddr: int, sections: dict) -> str:
  """Reads a null-terminated string from a file at a given virtual address."""
  if vaddr == 0:
    return ""

  target_section_info = None
  for sec in sections.values():
    if sec["addr"] <= vaddr < sec["addr"] + sec["size"]:
      target_section_info = sec
      break

  if target_section_info is None:
    return f"<unresolved vaddr 0x{vaddr:x}>"

  file_offset = target_section_info["offset"] + (
      vaddr - target_section_info["addr"]
  )
  f.seek(file_offset)

  byte_list = []
  while True:
    byte = f.read(1)
    if not byte or byte == b"\0":
      break
    byte_list.append(byte)
  return b"".join(byte_list).decode("utf-8", "ignore")


def get_chre_nanoapp_info(file_path: Path) -> dict | None:
  """Parses the chreNslNanoappInfo struct from a nanoapp .so file."""
  symbol_name = "_chreNslDsoNanoappInfo"
  symbol = _get_symbol(file_path, symbol_name)
  if not symbol or symbol["section"] == "UND":
    return None

  sections = _get_sections(file_path)
  if not sections:
    return None

  symbol_section_info = sections.get(symbol["section"])
  if not symbol_section_info:
    return None

  elf_class = _get_elf_class(file_path)
  if elf_class != 32:
    return None

  symbol_offset = symbol_section_info["offset"] + (
      symbol["value"] - symbol_section_info["addr"]
  )

  try:
    with open(file_path, "rb") as f:
      f.seek(symbol_offset)
      nanoapp_info_struct_data = f.read(symbol["size"])

      # Smallest struct size is at least past the magic and version
      if len(nanoapp_info_struct_data) < 8:
        print(
            f"Warning: nanoapp_info_struct_data too small for {file_path.name}",
            file=sys.stderr,
        )
        return None

      info_struct = ChreNslNanoappInfo()
      ctypes.memmove(
          ctypes.addressof(info_struct),
          nanoapp_info_struct_data,
          len(nanoapp_info_struct_data),
      )

      info = {
          "magic": info_struct.magic,
          "structMinorVersion": info_struct.structMinorVersion,
          "isSystemNanoapp": bool(info_struct.flags.isSystemNanoapp),
          "isTcmNanoapp": bool(info_struct.flags.isTcmNanoapp),
          "targetApiVersion": info_struct.targetApiVersion,
          "vendor": _read_string_at_vaddr(f, info_struct.vendor, sections),
          "name": _read_string_at_vaddr(f, info_struct.name, sections),
          "appId": info_struct.appId,
          "appVersion": info_struct.appVersion,
          "appVersionString": _read_string_at_vaddr(
              f, info_struct.appVersionString, sections
          ),
          "appPermissions": 0,
          "minChreApiVersion": 0,
          "requestedThreadPriority": 0,
      }

      if info["structMinorVersion"] >= 3:
        info["appPermissions"] = info_struct.appPermissions
      if info["structMinorVersion"] >= 4:
        info["minChreApiVersion"] = info_struct.minChreApiVersion
        info["requestedThreadPriority"] = info_struct.requestedThreadPriority

      return info

  except (IOError, struct.error, KeyError) as e:
    print(
        f"Warning: Could not parse nanoapp info for {file_path.name}: {e}",
        file=sys.stderr,
    )
    return None


def process_files(files_to_process: list[Path]):
  """Main function to find files, collect data, sort, and print results."""
  # --- Data Collection ---
  file_data = []
  total_size = 0
  total_padded_size = 0

  for file_path in files_to_process:
    size = file_path.stat().st_size
    filename = file_path.name
    padded_size = None
    nanoapp_info = None

    is_signed = "No"
    elf_offset = _find_elf_offset(file_path)
    if elf_offset is not None:
      # Given there is no standard way signing a nanoapp the simple heuristic
      # logic here is to see if there is any header (elf_offset). 40 is the
      # size of NanoAppBinaryHeader appended at the beginning of a .napp file.
      is_signed = "Yes" if elf_offset > 40 else "No"
      if elf_offset > 0:
        # Extract ELF to a temp file for analysis
        with tempfile.NamedTemporaryFile(
            delete=True, prefix="napp_xray_"
        ) as temp_elf:
          with open(file_path, "rb") as original_file:
            original_file.seek(elf_offset)
            temp_elf.write(original_file.read())
          temp_elf.flush()
          analysis_path = Path(temp_elf.name)
          padded_size = get_padded_mem_size(analysis_path)
          nanoapp_info = get_chre_nanoapp_info(analysis_path)
      else:  # elf_offset == 0
        # Standard ELF file
        padded_size = get_padded_mem_size(file_path)
        nanoapp_info = get_chre_nanoapp_info(file_path)

    if padded_size is not None:
      total_padded_size += padded_size

    file_data.append({
        "name": filename,
        "size": size,
        "padded": padded_size,
        "is_signed": is_signed,
        "nanoapp_name": (
            nanoapp_info.get("name", "N/A") if nanoapp_info else "N/A"
        ),
        "nanoapp_vendor": (
            nanoapp_info.get("vendor", "N/A") if nanoapp_info else "N/A"
        ),
        "nanoapp_app_id": (
            f"0x{nanoapp_info.get('appId', 0):x}" if nanoapp_info else "N/A"
        ),
        "nanoapp_app_version": (
            f"{nanoapp_info.get('appVersion', 0)}" if nanoapp_info else "N/A"
        ),
        "nanoapp_is_system": (
            "Yes"
            if nanoapp_info and nanoapp_info.get("isSystemNanoapp")
            else "No"
        ),
        "nanoapp_is_tcm": (
            "Yes" if nanoapp_info and nanoapp_info.get("isTcmNanoapp") else "No"
        ),
    })
    total_size += size

  file_count = len(file_data)

  # --- Sort the collected data ---
  # Use float('-inf') as the sort key for None values (like the no_val_marker)
  def sort_key(f):
    return f["padded"] if f["padded"] is not None else float("-inf")

  sorted_files = sorted(file_data, key=sort_key, reverse=True)

  # --- Pre-calculate column widths from sorted data for alignment ---
  header1 = "Filename"
  header2 = "Size (bytes)"
  header3 = "Padded memsize (bytes)"
  header4 = "Signed"
  header5 = "Name"
  header6 = "Vendor"
  header7 = "App ID"
  header8 = "App Version"
  header9 = "System"
  header10 = "TCM"
  total_label = f"Total ({file_count} files) :"

  # Convert all data to strings for width calculation
  col_data_str = []
  for f in sorted_files:
    col_data_str.append({
        "name": f["name"],
        "size": str(f["size"]),
        "pad": str(f["padded"]) if f["padded"] is not None else "",
        "is_signed": f["is_signed"],
        "nanoapp_name": f["nanoapp_name"],
        "nanoapp_vendor": f["nanoapp_vendor"],
        "nanoapp_app_id": f["nanoapp_app_id"],
        "nanoapp_app_version": f["nanoapp_app_version"],
        "nanoapp_is_system": f["nanoapp_is_system"],
        "nanoapp_is_tcm": f["nanoapp_is_tcm"],
    })

  # Find the max width for each column, including headers and total row
  max_name_len = max(
      [len(d["name"]) for d in col_data_str] + [len(header1), len(total_label)]
  )
  max_size_len = max(
      [len(d["size"]) for d in col_data_str]
      + [len(header2), len(str(total_size))]
  )
  max_pad_len = max(
      [len(d["pad"]) for d in col_data_str]
      + [len(header3), len(str(total_padded_size))]
  )
  max_is_signed_len = max(
      [len(d["is_signed"]) for d in col_data_str] + [len(header4)]
  )
  max_nanoapp_name_len = max(
      [len(d["nanoapp_name"]) for d in col_data_str] + [len(header5)]
  )
  max_nanoapp_vendor_len = max(
      [len(d["nanoapp_vendor"]) for d in col_data_str] + [len(header6)]
  )
  max_nanoapp_app_id_len = max(
      [len(d["nanoapp_app_id"]) for d in col_data_str] + [len(header7)]
  )
  max_nanoapp_app_version_len = max(
      [len(d["nanoapp_app_version"]) for d in col_data_str] + [len(header8)]
  )
  max_nanoapp_is_system_len = max(
      [len(d["nanoapp_is_system"]) for d in col_data_str] + [len(header9)]
  )
  max_nanoapp_is_tcm_len = max(
      [len(d["nanoapp_is_tcm"]) for d in col_data_str] + [len(header10)]
  )

  # Define column widths and formats
  columns_width = [
      max_name_len + 2,
      max(len(header2), max_size_len),
      max(len(header3), max_pad_len),
      max(len(header4), max_is_signed_len),
      max(len(header5), max_nanoapp_name_len),
      max(len(header6), max_nanoapp_vendor_len),
      max(len(header7), max_nanoapp_app_id_len),
      max(len(header8), max_nanoapp_app_version_len),
      max(len(header9), max_nanoapp_is_system_len),
      max(len(header10), max_nanoapp_is_tcm_len),
  ]

  row_fmt = "  ".join(f"{{:>{width}}}" for width in columns_width)

  total_width = sum(columns_width) + 2 * (len(columns_width) - 1)

  separator = "-" * total_width

  # --- Print Headers ---
  print(
      "\n"
      + row_fmt.format(
          header1,
          header2,
          header3,
          header4,
          header5,
          header6,
          header7,
          header8,
          header9,
          header10,
      )
  )
  print(separator)

  # --- Print Sorted Data ---
  for i, d_str in enumerate(col_data_str):
    print(
        row_fmt.format(
            d_str["name"],
            d_str["size"],
            d_str["pad"],
            d_str["is_signed"],
            d_str["nanoapp_name"],
            d_str["nanoapp_vendor"],
            d_str["nanoapp_app_id"],
            d_str["nanoapp_app_version"],
            d_str["nanoapp_is_system"],
            d_str["nanoapp_is_tcm"],
        )
    )

  # --- Print Totals ---
  if file_count > 1:
    print(separator)
    print(
        row_fmt.format(
            total_label,
            total_size,
            total_padded_size if total_padded_size > 0 else "",
            "",  # Empty for is_signed total
            "",  # Empty for nanoapp name total
            "",  # Empty for nanoapp vendor total
            "",  # Empty for nanoapp app ID total
            "",  # Empty for nanoapp app version total
            "",  # Empty for nanoapp is_system total
            "",  # Empty for nanoapp is_tcm total
        )
    )
  print()


def main():
  """Parses command-line arguments and runs the processing function."""
  parser = argparse.ArgumentParser(
      description=(
          "Extract size and general nanoapp info for CHRE nanoapp .so or .napp"
          " files."
      ),
      epilog=(
          "This script analyzes CHRE nanoapp ELF files to provide size and"
          " metadata."
      ),
  )
  parser.add_argument(
      "path_or_file", help="The directory or file name to analyze."
  )
  args = parser.parse_args()

  input_path = Path(args.path_or_file)
  if not input_path.exists():
    print(f"Error: Path/File does not exist: {input_path}", file=sys.stderr)
    sys.exit(1)

  files_to_process = []
  if input_path.is_dir():
    for pattern in ("*.so", "*.napp"):
      files_to_process.extend(sorted(list(input_path.glob(pattern))))
  elif input_path.is_file():
    if input_path.name.endswith(".so") or input_path.name.endswith(".napp"):
      files_to_process.append(input_path)

  if not files_to_process:
    print(
        "Error: No valid .so or .napp file(s) found. Input path/file:"
        f" `{input_path}`",
        file=sys.stderr,
    )
    sys.exit(1)

  process_files(files_to_process)


if __name__ == "__main__":
  main()
