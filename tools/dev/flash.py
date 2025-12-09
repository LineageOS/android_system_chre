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
from shell_util import ShellSession, not_have, has, fatal_error, warning


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


if __name__ == '__main__':
  if not os.getenv("TARGET_INSTALL_LOCATION"):
    fatal_error("TARGET_INSTALL_LOCATION is not set so the installation will not proceed")

  arg_parser = argparse.ArgumentParser(
    description='Flash the device with a signed binary'
  )

  arg_parser.add_argument('-R', '--reboot', action='store_true',
                          help="Reboot after the installation")

  args = arg_parser.parse_args()

  bash = ShellSession()
  root(bash)
  run = bash.run

  install_location = os.getenv('TARGET_INSTALL_LOCATION')
  build_target = os.getenv('CHRE_BUILD_TARGET')
  target_type = os.getenv('CHRE_TARGET_TYPE')

  run(f"adb shell mkdir -p {install_location}")
  run(f"adb push ./out/{build_target}/signed/*.so {install_location}",
      not_have("error"),
      show_output=True)

  if "nanoapp" == target_type:
    run(
      f"adb push ./out/{build_target}/*.napp_header {install_location}",
      not_have("error"), show_output=True)

  if args.reboot:
    run("adb reboot")
    run("adb wait-for-device")
    root(bash)
    print("Installation is complete")
  else:
    warning("Please reboot the device to complete the installation")
