#
# Endpoint Echo Test Nanoapp Makefile
#

# Makefile Includes ############################################################

include endpoint_echo_test.mk

# Nanoapp Configuration ########################################################

NANOAPP_NAME = endpoint_echo_test
NANOAPP_ID = 0x476f6f6754000012
NANOAPP_NAME_STRING = \"Endpoint\ Echo\ Test\"
NANOAPP_VERSION = 0x00000001

# Compiler Flags ###############################################################

# Defines
COMMON_CFLAGS += -DLOG_TAG=\"[EndpointEchoTest]\"

# Makefile Includes ############################################################

include $(CHRE_PREFIX)/build/nanoapp/app.mk
