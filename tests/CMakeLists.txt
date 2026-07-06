set(LQ_UNIT_TEST_SOURCES
  src/TestSupport.cpp
  c_api_tests.cpp
  feature_detection_tests.cpp
  qem_parameter_tests.cpp
  simplifier_tests.cpp
)

add_executable(line_quadrics_qem_tests ${LQ_UNIT_TEST_SOURCES})
target_include_directories(line_quadrics_qem_tests
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(line_quadrics_qem_tests
  PRIVATE
    line_quadrics_qem::line_quadrics_qem
    GTest::gtest_main
)
target_compile_definitions(line_quadrics_qem_tests
  PRIVATE
    LQ_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    LQ_TEST_EXTERNAL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/external"
)
lq_apply_warnings(line_quadrics_qem_tests)
lq_copy_runtime_dependencies(line_quadrics_qem_tests)
lq_copy_gtest_runtime_dependencies(line_quadrics_qem_tests)

include(GoogleTest)
gtest_add_tests(TARGET line_quadrics_qem_tests
  SOURCES ${LQ_UNIT_TEST_SOURCES}
  TEST_LIST LQ_UNIT_TESTS
)
if(LQ_UNIT_TESTS)
  set_tests_properties(${LQ_UNIT_TESTS} PROPERTIES LABELS unit)
endif()

set(LQ_UNIT_TEST_DEPENDS line_quadrics_qem_tests)
foreach(optional_target IN ITEMS
    linequadrics
    linequadrics_basic_simplify
    linequadrics_c_api_basic
)
  if(TARGET ${optional_target})
    list(APPEND LQ_UNIT_TEST_DEPENDS ${optional_target})
  endif()
endforeach()

add_custom_target(unit-tests
  COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_BINARY_DIR}"
    "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -LE performance --output-on-failure
  DEPENDS ${LQ_UNIT_TEST_DEPENDS}
  COMMENT "Running non-performance regression tests"
  VERBATIM
)

if(LQ_BUILD_PERFORMANCE_TESTS)
  set(LQ_PERFORMANCE_TEST_SOURCES
    src/TestSupport.cpp
    qem_dataset_tests.cpp
  )

  add_executable(line_quadrics_qem_performance_tests ${LQ_PERFORMANCE_TEST_SOURCES})
  target_include_directories(line_quadrics_qem_performance_tests
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(line_quadrics_qem_performance_tests
    PRIVATE
      line_quadrics_qem::line_quadrics_qem
      GTest::gtest_main
  )
  target_compile_definitions(line_quadrics_qem_performance_tests
    PRIVATE
      LQ_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
      LQ_TEST_EXTERNAL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/external"
  )
  lq_apply_warnings(line_quadrics_qem_performance_tests)
  lq_copy_runtime_dependencies(line_quadrics_qem_performance_tests)
  lq_copy_gtest_runtime_dependencies(line_quadrics_qem_performance_tests)

  gtest_add_tests(TARGET line_quadrics_qem_performance_tests
    SOURCES ${LQ_PERFORMANCE_TEST_SOURCES}
    TEST_LIST LQ_PERFORMANCE_TESTS
  )
  if(LQ_PERFORMANCE_TESTS)
    set_tests_properties(${LQ_PERFORMANCE_TESTS}
      PROPERTIES
        LABELS performance
        RUN_SERIAL TRUE
    )
  endif()

  add_custom_target(performance-tests
    COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_BINARY_DIR}"
      "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -L performance --output-on-failure
    DEPENDS line_quadrics_qem_performance_tests
    COMMENT "Running large mesh performance tests"
    VERBATIM
  )
endif()
