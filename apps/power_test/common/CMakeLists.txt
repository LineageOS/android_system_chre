include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_flatbuffers/flatbuffers.cmake)

pw_flatbuffer_library(chre.apps.power_test.common.power_test_generated
  SOURCES
    idl/chre_power_test.fbs
  FLATC_FLAGS
    --scoped-enums
    --cpp-ptr-type chre::UniquePtr
  DEPS
    chre_third_party.flatbuffers
  PREFIX
    generated/
  STRIP_PREFIX
  ${CMAKE_CURRENT_SOURCE_DIR}/idl
)

pw_add_library(chre.apps.power_test.common.power_test STATIC
  SOURCES
    power_test.cc
    request_manager.cc
  PRIVATE_INCLUDES
    include
  PRIVATE_DEPS
    chre.apps.power_test.common.power_test_generated.cpp
    chre.chre_api
    chre.platform.static_nanoapp_init
    chre.util
    chre.util.flatbuffers
    chre.util.nanoapp
    chre.util.system
    chre_third_party.flatbuffers
  PRIVATE_DEFINES
    CHRE_NANOAPP_INTERNAL
)
