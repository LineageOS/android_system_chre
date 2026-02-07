include_guard(GLOBAL)

include ($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_backend_variable(chre.test.extension_BACKEND DEFAULT_BACKEND google_contexthub.test.unit_test_extension)
