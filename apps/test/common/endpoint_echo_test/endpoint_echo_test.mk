#
# Endpoint Echo Test Nanoapp Makefile
#
# Environment Checks ###########################################################
ifeq ($(CHRE_PREFIX),)
  ifneq ($(ANDROID_BUILD_TOP),)
    CHRE_PREFIX = $(ANDROID_BUILD_TOP)/system/chre
  else
    $(error "You must run 'lunch' to setup ANDROID_BUILD_TOP, or explicitly \
    define the CHRE_PREFIX environment variable to point to the CHRE root \
    directory.")
  endif
endif

# Nanoapp Configuration ########################################################

NANOAPP_PATH = $(CHRE_PREFIX)/apps/test/common/endpoint_echo_test

# Source Code ##################################################################

COMMON_SRCS += $(NANOAPP_PATH)/src/endpoint_echo_test_manager.cc
COMMON_SRCS += $(NANOAPP_PATH)/src/endpoint_echo_test.cc

# Utilities ####################################################################

# Compiler Flags ###############################################################

# Defines
COMMON_CFLAGS += -DNANOAPP_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_DEBUG
COMMON_CFLAGS += -DCHRE_ASSERTIONS_ENABLED

# Includes
COMMON_CFLAGS += -I$(NANOAPP_PATH)/inc

# Permission declarations ######################################################

# PW RPC protos ################################################################
