#!/bin/bash

# NOTE: Ensure you use flatc version 1.12.0 here!
if [[ $(flatc --version | grep -Po "(?<=flatc version )([0-9]|\.)*(?=\s|$)") != "1.12.0" ]]; then
echo "[ERROR] flatc version must be 1.12.0"
exit
fi

pushd "$(dirname "$0")" >/dev/null

# Generate the CHRE-side header file
flatc --cpp -o ../generated/include/generated/ --scoped-enums \
  --cpp-ptr-type chre::UniquePtr chre_power_test.fbs

# Generate the AP-side header file with some extra goodies
flatc --cpp -o ./ --scoped-enums --gen-mutable \
  --gen-object-api chre_power_test.fbs

popd >/dev/null
