This folder contains FlatBuffers schema used in the communications protocol
between the power test nanoapp and the host (applications processor) test code.

Use the included update.sh script to generate the header files used in CHRE,
which requires that the FlatBuffers compiler `flatc` be available in $PATH.

FlatBuffers compiler version 1.12.0 must be used since some modifications are
made to the version of flatbuffers header used by the generated code.

For more information on FlatBuffers, see https://github.com/google/flatbuffers/

Here are some instructions to install `flatc` v.1.12.0:

```
cd ~/
git clone https://github.com/google/flatbuffers.git
cd flatbuffers

# Fetch tags to make sure you have v1.12.0
git fetch --tags

# Checkout the v1.12.0 tag
git checkout v1.12.0

# Create a build directory
mkdir build
cd build

# Configure using CMake
cmake ..

# Build the compiler
make -j

# The compiler binary is now at ./flatc
./flatc --version

# Install flatc to path
You can either install it via moving the `flatc` binary under a path folder or
sudo make install
```
