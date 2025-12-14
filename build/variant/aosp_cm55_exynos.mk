include $(CHRE_PREFIX)/build/clean_build_template_args.mk

TARGET_NAME = aosp_cm55_exynos

ifneq ($(filter $(TARGET_NAME)% all, $(MAKECMDGOALS)),)

  ifneq ($(IS_NANOAPP_BUILD),)
    # Disable tokenized logging for nanoapps for now
    # Disable -fno-threadsafe-statics option
    COMMON_CFLAGS := $(filter-out -DCHRE_NANOAPP_TOKENIZED_LOGGING_ENABLED -fno-threadsafe-statics, $(COMMON_CFLAGS))
  endif

  CORTEXM_ARCH := m55
  TARGET_CFLAGS += -DSOC=$(SOC)
  TARGET_CFLAGS += -DFREERTOS

  # This option determines whether to declare mutex static or dynamic in FREERTOS.
  # Currently, the static option is turned off in FreeRTOS, so the option is added.
  TARGET_CFLAGS += -DCHRE_CREATE_MUTEX_ON_HEAP

  # Sized based on the buffer allocated in the host daemon (4096 bytes), minus
  # FlatBuffer overhead (max 88 bytes), minus some extra space to make a nice
  # round number and allow for addition of new fields to the FlatBuffer.
  TARGET_CFLAGS += -DCHRE_MESSAGE_TO_HOST_MAX_SIZE=4000

  # Word size for this architecture
  TARGET_CFLAGS += -DCHRE_32_BIT_WORD_SIZE

  # Temporarily need the following define until the logcat redirection is implemented.
  TARGET_CFLAGS += -D__int64_t_defined

  # Temporarily disable implicit double promotion warnings until logcat
  # redirection is implemented.
  TARGET_CFLAGS += -Wno-double-promotion

  # GCC is unnecessarily strict with shadow warnings in legal C++ constructor
  # syntax.
  TARGET_CFLAGS += -Wno-shadow

  TARGET_CFLAGS += -DCHRE_FIRST_SUPPORTED_API_VERSION=CHRE_API_VERSION_1_10
  TARGET_CFLAGS += -I$(CHRE_PREFIX)/platform/shared/include/chre/platform/shared/libc
  TARGET_CFLAGS += $(DSO_SUPPORT_LIB_CFLAGS)

  TARGET_VARIANT_SRCS += $(DSO_SUPPORT_LIB_SRCS)

  TARGET_PLATFORM_ID = 0x476F6F676C002000

  ifeq ($(IS_ARCHIVE_ONLY_BUILD),)
    GCC_RTLIB=$(CORTEXM_TOOLS_PREFIX)/lib/gcc/arm-none-eabi/12.3.1/thumb/v8-m.main+fp/hard/
    TARGET_SO_LATE_LIBS += -L$(GCC_RTLIB)
    TARGET_SO_LATE_LIBS += -lgcc
  endif

  include $(CHRE_PREFIX)/build/arch/cortexm.mk
  include $(CHRE_PREFIX)/build/build_template.mk
endif
