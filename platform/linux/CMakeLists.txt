pw_add_library(chre.platform.linux.context STATIC
  SOURCES
    context.cc
  PRIVATE_DEPS
    chre.core
    chre.platform.context.facade
    chre.platform.linux.thread_context
    chre.variant.config
)

pw_add_library(chre.platform.linux.fatal_error STATIC
  HEADERS
    public_fatal_error/chre/target_platform/fatal_error.h
  PUBLIC_INCLUDES
    public_fatal_error
  SOURCES
    fatal_error.cc
  PRIVATE_DEPS
    chre.platform.linux.platform_log
)

pw_add_library(chre.platform.linux.host_link STATIC
  HEADERS
    public_host_link/chre/target_platform/host_link_base.h
  PUBLIC_INCLUDES
    public_host_link
  SOURCES
    host_link.cc
  PRIVATE_DEPS
    chre.core
    chre.platform.host_link.facade
    chre.platform.linux.pal_ble
)

pw_add_library(chre.platform.linux.memory_manager STATIC
  SOURCES
    memory_manager.cc
  PRIVATE_DEPS
    chre.platform.memory_manager.facade
    chre.platform.shared.memory_manager
    chre.util
)

pw_add_library(chre.platform.linux.mutex INTERFACE
  HEADERS
    include/chre/target_platform/mutex_base_impl.h
    include/chre/target_platform/mutex_base.h
  PUBLIC_INCLUDES
    include
  PUBLIC_DEPS
    chre.platform.mutex.facade
)

pw_add_library(chre.platform.linux.pal_ble STATIC
  HEADERS
    include/chre/platform/linux/pal_ble.h
  PUBLIC_INCLUDES
    include
  SOURCES
    pal_ble.cc
  PRIVATE_DEPS
    chre.chre_api
    chre.pal.ble.facade
    chre.platfrom.linux.task_util.task_manager
    chre.util
)

pw_add_library(chre.platform.linux.platform_debug_dump_manager STATIC
  SOURCES
    platform_debug_dump_manager.cc
  PRIVATE_DEPS
    chre.platform.platform_debug_dump_manager.facade
)

pw_add_library(chre.platform.linux.platform_log STATIC
  HEADERS
    include/chre/platform/linux/platform_log.h
  PUBLIC_INCLUDES
    include
  PUBLIC_DEPS
    chre.chre_api
    chre.util
  SOURCES
    platform_log.cc
  PRIVATE_DEPS
    # The Linux platform_log uses fatal_error, but the Linux fatal error also
    # uses platform_log, so we need to use the facade target here to break the
    # cycle even though this target isn't a fatal_error backend.
    chre.platform.fatal_error.facade
    chre.platform.shared.bt_snoop_log.facade
)

pw_add_library(chre.platform.linux.platform_nanoapp STATIC
  HEADERS
    public_platform_nanoapp/chre/target_platform/platform_nanoapp_base.h
  PUBLIC_INCLUDES
    public_platform_nanoapp
  PUBLIC_DEPS
    chre.platform.shared.nanoapp_support_lib_dso
  SOURCES
    platform_nanoapp.cc
  PRIVATE_DEPS
    chre.chre_api
    chre.platform.assert
    chre.platform.log
    chre.util.system
)

pw_add_library(chre.platform.linux.power_control_manager STATIC
  HEADERS
    public_power_control_manager/chre/target_platform/power_control_manager_base.h
  PUBLIC_INCLUDES
    public_power_control_manager
  SOURCES
    power_control_manager.cc
  PRIVATE_DEPS
    chre.platform.power_control_manager.facade
)

pw_add_library(chre.platform.linux.static_nanoapp_init INTERFACE
  HEADERS
    include/chre/target_platform/static_nanoapp_init.h
  PUBLIC_INCLUDES
    include
  PUBLIC_DEPS
    chre.chre_api
    chre.core
    chre.platform.fatal_error
    chre.platform.shared.nanoapp_support_lib_dso
)

pw_add_library(chre.platfrom.linux.task_util.task_manager STATIC
  HEADERS
    include/chre/platform/linux/task_util/task_manager.h
  PUBLIC_INCLUDES
    include
  PUBLIC_DEPS
    chre.platform.linux.task_util.task
    chre.util
  SOURCES
    task_util/task_manager.cc
)

pw_add_library(chre.platform.linux.task_util.task STATIC
  HEADERS
    include/chre/platform/linux/task_util/task.h
  PUBLIC_INCLUDES
    include
  SOURCES
    task_util/task.cc
)


pw_add_library(chre.platform.linux.thread_context INTERFACE
  HEADERS
    include/chre/platform/linux/thread_context.h
  PUBLIC_INCLUDES
    include
  PUBLIC_DEPS
    chre.core
)

pw_add_library(chre.platform.linux.system_time STATIC
  HEADERS
    include/chre/platform/linux/system_time.h
  PUBLIC_INCLUDES
    include
  PUBLIC_DEPS
    chre.util
  SOURCES
    system_time.cc
  PRIVATE_DEPS
    chre.platform.assert
)

pw_add_library(chre.platform.linux.system_timer STATIC
  HEADERS
    public_system_timer/chre/target_platform/system_timer_base.h
  PUBLIC_INCLUDES
    public_system_timer
  SOURCES
    system_timer.cc
  PRIVATE_DEPS
    chre.platform.log
    chre.platform.system_timer.facade
    chre.util
)

pw_add_library(chre.platform.linux.assert STATIC
  HEADERS
    include/chre/target_platform/assert.h
    include/chre/platform/linux/expect_assert.h
  PUBLIC_INCLUDES
    include/
  PUBLIC_DEPS
    chre.platform.shared.assert_func
  SOURCES
    assert.cc
  PRIVATE_DEPS
    chre.platform.log
)
