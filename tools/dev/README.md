# CHRE Development Tools

This directory contains the tools and configuration files required to establish a development
environment for the Context Hub Runtime Environment (CHRE). The [Usage](#usage) section details the
operational procedures, while the [Configuration](#configuration) section explains the setup of
shell environment variables via `env_config.json`. Several [Python scripts](#public) are designed
for standalone use to support specific development tasks.

## Usage

### Set up the environment

To set up the CHRE development environment, run `source env_setup.sh` to initiate shell commands,
which will enable commands like `chre_lunch`, etc. Here is a list of commands available after
sourcing the `env_setup.sh` script.

- `chre_envs`: Prints all the environment variables set up for CHRE development.
- `chre_lunch <platform-target> [-c <config_file>]`: Sets up the environment for specific platform
  and target combination. The `-c` option allows specifying an alternative configuration file
  instead of the default `env_config.json`.
- `chre_make [-C] [-s <src_path>]`: Builds the CHRE target. `-s` option allows the user to specify
  a separate source path. `-C` option generates `CMakeLists.txt` and `compile_commands.json`.
- `chre_flash [-R]`: Build the target and flash the device with a signed binary.

Now running `chre_lunch <platform-target>` will set up environment for a specific platform and
target combination. For example, to start development of nanoapp on tinysys platform, run
`chre_lunch tinysys-nanoapp`. This step is required to enable `chre_make` and `chre_flash` to work.

Any missing command-line tools that are required will be listed out immediately after running
`chre_lunch`. Currently, it's left to the user to install them as different OS has different
commands for installation.

### Build a target

After setting up the environment for a specific platform and target combination, a CHRE target
(e.g., `nanoapp` or `libchre`) can be built by running `chre_make` from the folder where the target
Makefile is defined.

The command will also sign the binary and check if there are any external symbols that are
unresolvable.

#### Use `-C` option to generate `CMakeLists.txt` and `compile_commands.json`

When `-C` option is provided, `chre_make`can generate `CMakeLists.txt` and `compile_commands.json`.
An additional source path can be provided using `-s`, which fits for the project that the build
folder is separated from the src folder. For example:

```
chre_make -C -s ../../src/general_test
```

Note that `chre_make` will only generate above two files when `-C` is provided. This is because the
dryrun output of making the target is used extract source files, include directories, compiler
flags, and macro definitions. Therefore to generate the target binary please do not add `-C`.

### Flash the target onto the device

After building the target, it can be flashed onto the device by running `chre_flash`. The location
that the binary is copied to is determined by the setting of "install_location" in the
`env_config.json`.

If `-R` option is provided, the device will reboot.

## Configuration

The configuration of the environment is done through the `env_config.json` file. It allows for
specifying different build targets, environment variables, and Python versions for various
hardware and software platforms.

### Top-Level Structure

The file consists of a JSON array, where each object in the array represents a single platform
configuration.

```json
[
  {
    "platform": "...",
    ...
  },
  {
    "platform": "...",
    ...
  }
]
```

### Platform Configuration Fields

Each platform configuration object has the following fields:

- `platform` (string, required): A unique name identifying the platform (e.g., "qsh",
  "tinysys").
- `python_version` (string, optional): The required Python version for development on this
  platform, if not specified, the default python version will be used.
- `common_env_variables` (array, optional): An array of objects where each defines a shell
  environment variables that are common to all targets within this platform.
- `targets` (array, required): An array of target-specific configuration objects.

### Target Configuration Fields

Each object within the `targets` array defines a specific build target and has the following
fields:

- `name` (string, required): A descriptive name for the target (e.g., "nanoapp", "shim").
- `build_target` (string, required): The identifier for the build system (e.g.,
  "aosp_hexagonv66_qsh").
- `install_location` (string, optional): The path where the output binary of the build will be
  installed on the connected physical device. If this is not specified, the output binary will
  not be installed.
- `env_variables` (array, optional): An array of objects where each defines a shell environment
  variables that are specific to this target.

### Environment Variable Fields

The environment variable objects, found in both `common_env_variables` and a target's
`env_variables`, are *exported* shell variables defined in the current shell and will exist in
spawned child shell sessions. Each environment variable object has the following structure:

- `name` (string, required): The name of the environment variable (e.g., "ANDROID_BUILD_TOP").
- `description` (string, required): A brief description of the variable's purpose and the expected
  value.
- `type` (string, required): The type of the variable, which can be:
    - `path`: Represents a file system path.
    - `value`: A simple string value. At this moment only `-` and characters from `[a-zA-Z0-9_]` are
      allowed.
    - `file`: Represents a path to a file.
    - `list[path|value|file]`: Represents a list of values of the specified type
- `default` (string, optional): The default value for the environment variable. This can be a
  static string or can reference other environment variables using the `$` syntax (e.g.,
  `$ANDROID_BUILD_TOP/external/nanopb-c`). Note that although using an environment variable to
  define another is supported, the variable on the right side must be defined prior to the variable
  on the left side. `common_env_variables` fields are parsed before target-specific `env_variables`
  fields.
- `default_action` (array, optional): An array of strings defining an action to be executed
  to obtain the default value if it's not already set. The first element is the action name,
  followed by its parameters.

### Actions

Actions are compound operations that are not easy to fulfil with simple shell commands. Predefined
actions listed below are supported. Behind the scene they are executed though dedicated python
functions defined in `env_setup.py`.

- `action_clone_repo`: Clones a git repository. The parameters are:
    - Repository URL (string, required): The URL of the git repository to clone.
    - Branch (string, required): The branch to clone.
    - Destination Path (string, required): The path where the repository will be cloned. For
      example,
      ```json
      "default_action": [
        "action_clone_repo",
        "https://a-link.to.your.qsh.branch",
        "$QSH_BRANCH",
        "$CHRE_DEV_PATH/mirror-qsh-$QSH_BRANCH"
      ]
      ```
      the above action will clone the qsh repository from the branch specified by the
      environment variable `QSH_BRANCH` into the directory `$CHRE_DEV_PATH/mirror-qsh-$QSH_BRANCH`.

## File Structure

The CHRE development environment maintains a structured file system to ensure consistency and
isolation between different platform-target configurations. This organization is centered around
the `CHRE_DEV_PATH` environment variable.

### `CHRE_DEV_PATH`

This is the root directory for all development files related to a specific platform-target
combination. It is automatically set to `~/.chre_dev/<platform_name>-<target_name>` when you run
`chre_lunch`. For example, if you run `chre_lunch tinysys-nanoapp`, `CHRE_DEV_PATH` will be set to
`~/.chre_dev/tinysys-nanoapp`.

### Python Virtual Environment

To maintain dependency isolation, a Python virtual environment is created within the
`CHRE_DEV_PATH`. Specifically, it is located at `$CHRE_DEV_PATH/venv`. This ensures that each
platform-target combination has its own set of Python packages, preventing version conflicts.

### `env_vars.txt`

To streamline the setup process, the environment variables you provide during the `chre_lunch`
are saved to a file named `env_vars.txt` inside the `CHRE_DEV_PATH`. The next time you run
`chre_lunch` for the same platform-target, the script will detect this file and ask if you want to
reuse the saved settings. This feature saves you from re-entering the same information repeatedly.

## Python Scripts

### Internal

Scripts listed here are used internally by the shell commands. Using them directly is not supported.

- `env_setup.py`: A helper script used to set up the CHRE development environment. It is used
  internally by the shell script `env_setup.sh`.
- `flash.py`: A tool to flash the device with a signed binary.
- `shell_util.py`: A utility library for shell scripts.

### public

Scripts listed here are can also be used as a stand-alone tool to facilitate special needs.

- `cml_gen.py`: Generates `CMakeLists.txt` and `compile commands.json` files for the corresponding
  CHRE target.
- `check_nanoapp_symbols.py`: Checks dynamic symbols and find out if any of them is unresolvable.
- `tinsys_nanoapp_signer.py`: A tool to sign nanoapps for tinysys.

## Python Packages

There are two package list files used to specifiy what packages are needed:

- `requirements.txt`: The general list of packages. New package should be added here.
- `requirements_protobuf.txt`: The list for protobuf specifically.

A reason to have a separate requirement file for protobuf is to avoid an infinite dependency
overriding loop observed when multiple packages fetch different versions of protobuf.

TODO(b/374392644) - Consider separate requirements based on different platform and target
combinations.