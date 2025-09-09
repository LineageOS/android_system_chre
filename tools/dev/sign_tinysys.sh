#!/bin/bash

# Signs a Tinysys nanoapp binary using the tinysys_nanoapp_signer.py script.
#
# Usage:
#   sign_tinysys.sh <input_file> <output_file>
#
# This script requires the TEST_SIGN_KEY and CHRE_DEV_SCRIPT_PATH environment
# variables to be set.
#
# Exits with 0 on success, 1 on failure.

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <input_file> <output_file>" >&2
  exit 1
fi

if [ -z "$TEST_SIGN_KEY" ]; then
  echo "Error: TEST_SIGN_KEY is not set." >&2
  exit 1
fi

if [ -z "$CHRE_DEV_SCRIPT_PATH" ]; then
  echo "Error: CHRE_DEV_SCRIPT_PATH is not set." >&2
  exit 1
fi

input_file="$1"
output_file="$2"
output_dir=$(dirname "$output_file")
signer_script="$CHRE_DEV_SCRIPT_PATH/tinysys_nanoapp_signer.py"

if [ ! -f "$signer_script" ]; then
  echo "Error: tinysys_nanoapp_signer.py not found at $signer_script" >&2
  exit 1
fi

python3 "$signer_script" "$TEST_SIGN_KEY" "$input_file" "$output_dir"
exit $?
