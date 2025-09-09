#!/bin/bash

# Signs a QSH nanoapp binary using the Hexagon SDK elfsigner.py script.
#
# Usage:
#   sign_qsh.sh <input_file> <output_file>
#
# This script requires the HEXAGON_SDK_PREFIX environment variable to be set.
#
# Exits with 0 on success, 1 on failure.

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <input_file> <output_file>" >&2
  exit 1
fi

if [ -z "$HEXAGON_SDK_PREFIX" ]; then
  echo "Error: HEXAGON_SDK_PREFIX is not set." >&2
  exit 1
fi

input_file="$1"
output_file="$2"
# elfsigner.py takes an output directory, not a file.
output_dir=$(dirname "$output_file")
elfsigner_path="$HEXAGON_SDK_PREFIX/tools/elfsigner/elfsigner.py"

if [ ! -f "$elfsigner_path" ]; then
  echo "Error: elfsigner.py not found at $elfsigner_path" >&2
  exit 1
fi

python3 "$elfsigner_path" --no_disclaimer -i "$input_file" -o "$output_dir"
exit $?
