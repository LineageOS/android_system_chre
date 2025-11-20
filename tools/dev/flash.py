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

"""Flashes a built CHRE target to a connected Android device.

This script is invoked by the `chre_flash` shell function. It uses `adb` to push
the compiled and signed binary (.so file) and, for nanoapps, the corresponding
.napp_header file to the device.
"""
import argparse
import os
from pathlib import Path
import re
import struct
import time
from shell_util import ShellSession, fatal_error, find_unique_file, has, not_have, success, warning


def get_nanoapp_id(header_file):
  with open(header_file, "rb") as f:
    # The format of the header is defined in
    # host/common/include/chre_host/napp_header.h
    # Define header_format corresponding to the NanoAppBinaryHeader struct.
    header_format = "<IIQIIQBB6x"
    data = f.read(struct.calcsize(header_format))
    _, _, app_id, _, _, _, _, _ = struct.unpack(header_format, data)
    return f"{app_id:016x}"


def verify_nanoapp(session: ShellSession, header_file: str):
  """Verifies the nanoapp installation by checking dumpsys output.

  Args:
    session: The ShellSession object to execute adb commands.
    header_file: The path to the .napp_header file.
  """
  nanoapp_id = get_nanoapp_id(header_file)
  dumpsys_output = ""
  # Retry for a few seconds as the nanoapp may take time to load
  for i in range(5):
    dumpsys_output = session.run(
        "adb shell dumpsys android.hardware.contexthub.IContextHub/default",
        show_output=False,
    )
    if nanoapp_id in dumpsys_output:
      success(
          f"Nanoapp {nanoapp_id} found in the output of dumpsys of"
          " contexthub HAL"
      )
      return
    print(f"  Verification attempt {i + 1}/5...")
    time.sleep(1)

  warning(
      f"Verification failed: Nanoapp ID {nanoapp_id} not found in dumpsys of"
      " contexthub HAL."
  )
  print("Last dumpsys output:")
  print(dumpsys_output)


def root(session: ShellSession):
  """Ensures the device has root access and the system is remounted.

  This function runs 'adb root' and 'adb remount'. If the device reboots
  as part of this process, it waits for the device to come back online.

  Args:
    session: The ShellSession object to execute adb commands.
  """
  output = session.run("adb root && adb remount -R", not_have("no devices"))
  if "Rebooting device for new settings to take effect" in output:
    print("rebooted to remount")
    session.run("adb wait-for-device")
    session.run("root", has("Remount succeeded"))


if __name__ == "__main__":
  if not os.getenv("TARGET_INSTALL_LOCATION"):
    fatal_error(
        "TARGET_INSTALL_LOCATION is not set so the installation will not"
        " proceed"
    )

  arg_parser = argparse.ArgumentParser(
      description="Flash the device with a signed binary"
  )

  arg_parser.add_argument(
      "-R",
      "--reboot",
      action="store_true",
      help="Reboot after the installation",
  )

  args = arg_parser.parse_args()

  install_location = os.getenv("TARGET_INSTALL_LOCATION")
  build_target = os.getenv("CHRE_BUILD_TARGET")
  target_type = os.getenv("CHRE_TARGET_TYPE")
  so_file = find_unique_file(f"./out/{build_target}/signed/*.so")
  header_file = ""

  bash = ShellSession(cmd_width=len(so_file) + 40)
  root(bash)
  run = bash.run

  run(f"adb shell mkdir -p {install_location}")
  run(f"adb push {so_file} {install_location}", has("1 file pushed"))

  if target_type == "nanoapp":
    header_file = find_unique_file(f"./out/{build_target}/*.napp_header")
    run(f"adb push {header_file} {install_location}", has("1 file pushed"))

  is_flashed = True
  if args.reboot:
    run("adb reboot")
    run("adb wait-for-device")
    root(bash)
  elif os.getenv("QUICK_FLASH_COMMAND"):
    run(os.getenv("QUICK_FLASH_COMMAND"))
  elif target_type == "nanoapp" and re.search(
      "chre_aidl_hal_client\r\n",
      run("adb shell ls /vendor/bin/chre_aidl_hal_client"),
  ):
    run(
        "adb shell /vendor/bin/chre_aidl_hal_client unload 0x{}".format(
            get_nanoapp_id(header_file)
        ),
        show_output=True,
    )
    run(
        "adb shell /vendor/bin/chre_aidl_hal_client load {}".format(
            Path(so_file).stem
        ),
        show_output=True,
    )
  else:
    is_flashed = False
    warning("Please reboot the device to complete the installation")

  if is_flashed:
    if target_type == "nanoapp":
      verify_nanoapp(bash, header_file)
    print("Installation is complete")
