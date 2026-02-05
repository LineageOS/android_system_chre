#
# Context Hub Data Flow Makefile
#

ifeq ($(CHRE_DATA_FLOW_SUPPORT_ENABLED), true)

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -I$(CHRE_PREFIX)/data_flow/include
COMMON_CFLAGS += -I$(CHRE_PREFIX)/platform/include

# Common Source Files ##########################################################

COMMON_SRCS += $(CHRE_PREFIX)/data_flow/queue.cc

# Location of Pigweed modules  #########################################

PIGWEED_DIR = $(ANDROID_BUILD_TOP)/external/pigweed
PIGWEED_CHRE_DIR = $(ANDROID_BUILD_TOP)/system/chre/external/pigweed

# Pigweed ######################################################################

COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_allocator/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_assert/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_bytes/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_containers/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_function/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_intrusive_ptr/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_log/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_result/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_span/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/pw_status/public
COMMON_CFLAGS += -I$(PIGWEED_DIR)/third_party/fuchsia/repo/sdk/lib/fit/include
COMMON_CFLAGS += -I$(PIGWEED_DIR)/third_party/fuchsia/repo/sdk/lib/stdcompat/include

COMMON_SRCS += $(PIGWEED_DIR)/pw_allocator/allocator.cc
COMMON_SRCS += $(PIGWEED_DIR)/pw_containers/intrusive_item.cc

endif  # $(CHRE_DATA_FLOW_SUPPORT_ENABLED) == true
