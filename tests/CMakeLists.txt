add_executable(line_quadrics_qem_tests
  c_api_tests.cpp
  simplifier_tests.cpp
)
target_link_libraries(line_quadrics_qem_tests
  PRIVATE
    line_quadrics_qem::line_quadrics_qem
    GTest::gtest_main
)
target_compile_definitions(line_quadrics_qem_tests
  PRIVATE
    LQ_TEST_EXTERNAL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/external"
)
lq_apply_warnings(line_quadrics_qem_tests)
lq_copy_runtime_dependencies(line_quadrics_qem_tests)

include(GoogleTest)
gtest_discover_tests(line_quadrics_qem_tests)

if(LQ_BUILD_PERFORMANCE_TESTS)
  add_executable(line_quadrics_qem_performance_tests
    performance_tests.cpp
  )
  target_link_libraries(line_quadrics_qem_performance_tests
    PRIVATE
      line_quadrics_qem::line_quadrics_qem
      GTest::gtest_main
  )
  target_compile_definitions(line_quadrics_qem_performance_tests
    PRIVATE
      LQ_TEST_EXTERNAL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/external"
  )
  lq_apply_warnings(line_quadrics_qem_performance_tests)
  lq_copy_runtime_dependencies(line_quadrics_qem_performance_tests)

  gtest_discover_tests(line_quadrics_qem_performance_tests
    PROPERTIES
      LABELS performance
      RUN_SERIAL TRUE
    DISCOVERY_TIMEOUT 60
  )

  add_custom_target(performance-tests
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C "$<CONFIG>" -L performance --output-on-failure
    DEPENDS line_quadrics_qem_performance_tests
    COMMENT "Running large mesh performance tests"
    VERBATIM
  )
endif()
