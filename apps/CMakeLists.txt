add_subdirectory(gnss_world)
add_subdirectory(power_test)

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_library(chre.apps.apps INTERFACE
  HEADERS
    include/chre/apps/apps.h
  PUBLIC_INCLUDES
    include
  PUBLIC_DEPS
    chre.core
    chre.util
)
