include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_library(chre.data_flow STATIC
  HEADERS
    include/data_flow/internal/queue_internal.h
    include/data_flow/internal/SharedDataRegion.h
    include/data_flow/queue.h
    include/data_flow/queue_defs.h
    include/data_flow/untyped_queue.h
  PUBLIC_INCLUDES
    include
  PUBLIC_DEPS
    chre.platform.atomic
    chre.variant.config
    pw_allocator
    pw_assert
    pw_bytes
    pw_containers
    pw_function
    pw_intrusive_ptr
    pw_log
    pw_result
    pw_span
    pw_status
  SOURCES
    queue.cc
)
