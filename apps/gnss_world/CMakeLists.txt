include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_library(chre.apps.gnss_world.gnss_world STATIC
  SOURCES
    gnss_world.cc
  PRIVATE_DEPS
    chre.chre_api
    chre.platform.static_nanoapp_init
    chre.util
    chre.util.nanoapp
    chre.util.system
  PRIVATE_DEFINES
    CHRE_NANOAPP_INTERNAL
)
