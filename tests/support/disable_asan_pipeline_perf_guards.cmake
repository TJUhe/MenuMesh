# PRE_TEST discovery populates manumesh_tests_TESTS before this script runs.
# AddressSanitizer changes wall-clock timing substantially, so these guards are
# intentionally disabled only for the sanitizer configuration. The unmodified
# Debug and Release configurations continue to enforce their thresholds.
foreach(manumesh_ctest_name IN LISTS manumesh_tests_TESTS)
  if(manumesh_ctest_name MATCHES "^PipelinePerfGuard\\.")
    set_tests_properties("${manumesh_ctest_name}" PROPERTIES DISABLED TRUE)
  endif()
endforeach()
