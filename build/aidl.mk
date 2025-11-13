#
# Nanoapp/CHRE AIDL Makefile
#
# Include this file to generate source files for .aidl interfaces. The following
# variables must be defined:
# - AIDL_GEN_SRCS:  The list of .aidl interface files
# - AIDL_ROOT:      The root directory of the interface
#
# Note that the .aidl interface must be defined in $AIDL_ROOT/<package>/<source>.aidl.
# For example, if an IHello interface is defined under the package hello.world, then
# the file must exist in $AIDL_ROOT/hello/world/IHello.aidl.
#
# The generated source files and includes are automatically added to COMMON_SRCS and
# COMMON_CFLAGS, respectively.

# Environment Checks ###############################################################

ifeq ($(ANDROID_BUILD_TOP),)
$(error "You must run lunch, or specify an explicit ANDROID_BUILD_TOP environment \
         variable")
endif

# Setup ############################################################################

# Currently supports the cpp language. Other languages may be added as needed.
AIDL_LANGUAGE := cpp

AIDL_GEN_PATH := $(OUT)/aidl_gen

AIDL_GEN_SRCS += $(patsubst %.aidl, \
                     $(AIDL_GEN_PATH)/%.cpp, \
                     $(AIDL_SRCS))

ifneq ($(AIDL_GEN_SRCS),)
COMMON_CFLAGS += -I$(AIDL_GEN_PATH)/$(AIDL_ROOT)
COMMON_SRCS += $(AIDL_GEN_SRCS)
endif

ifeq ($(AIDL_TOOL),)
AIDL_TOOL := $(ANDROID_BUILD_TOP)/prebuilts/build-tools/linux-x86/bin/aidl
endif

# Build ############################################################################

$(AIDL_GEN_PATH)/%.cpp: %.aidl $(AIDL_TOOL)
	@echo "[AIDL] $<"
	$(V)mkdir -p $(AIDL_GEN_PATH)/$(AIDL_ROOT)
	$(AIDL_TOOL) --lang=$(AIDL_LANGUAGE) --structured $(abspath $<) \
		-I $(AIDL_ROOT) -h $(AIDL_GEN_PATH)/$(AIDL_ROOT) -o $(AIDL_GEN_PATH)/$(AIDL_ROOT)