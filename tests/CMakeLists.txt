set(MANUMESH_UNIT_TEST_SOURCES
  support/TestSupport.cpp
  unit/api/c_api_tests.cpp
  unit/feature_detection/feature_detection_tests.cpp
  unit/simplification/qem_parameter_tests.cpp
  unit/simplification/simplifier_tests.cpp
)

add_executable(manumesh_tests ${MANUMESH_UNIT_TEST_SOURCES})
target_include_directories(manumesh_tests
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
)
target_link_libraries(manumesh_tests
  PRIVATE
    ManuMesh::manumesh
    GTest::gtest_main
)
target_compile_definitions(manumesh_tests
  PRIVATE
    MANUMESH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    MANUMESH_TEST_EXTERNAL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/external"
)
manumesh_apply_warnings(manumesh_tests)
manumesh_copy_runtime_dependencies(manumesh_tests)
manumesh_copy_gtest_runtime_dependencies(manumesh_tests)

include(GoogleTest)
gtest_add_tests(TARGET manumesh_tests
  SOURCES ${MANUMESH_UNIT_TEST_SOURCES}
  TEST_LIST MANUMESH_UNIT_TESTS
)
if(MANUMESH_UNIT_TESTS)
  set_tests_properties(${MANUMESH_UNIT_TESTS} PROPERTIES LABELS unit)
endif()

set(MANUMESH_UNIT_TEST_DEPENDS manumesh_tests)
foreach(optional_target IN ITEMS
    manumesh
    manumesh_basic_simplify
    manumesh_c_api_basic
)
  if(TARGET ${optional_target})
    list(APPEND MANUMESH_UNIT_TEST_DEPENDS ${optional_target})
  endif()
endforeach()

add_custom_target(unit-tests
  COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_BINARY_DIR}"
    "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -LE performance --output-on-failure
  DEPENDS ${MANUMESH_UNIT_TEST_DEPENDS}
  COMMENT "Running non-performance regression tests"
  VERBATIM
)

if(MANUMESH_BUILD_PERFORMANCE_TESTS)
  set(MANUMESH_PERFORMANCE_TEST_SOURCES
    support/TestSupport.cpp
    performance/qem_dataset_tests.cpp
  )

  add_executable(manumesh_performance_tests ${MANUMESH_PERFORMANCE_TEST_SOURCES})
  target_include_directories(manumesh_performance_tests
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/support"
  )
  target_link_libraries(manumesh_performance_tests
    PRIVATE
      ManuMesh::manumesh
      GTest::gtest_main
  )
  target_compile_definitions(manumesh_performance_tests
    PRIVATE
      MANUMESH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
      MANUMESH_TEST_EXTERNAL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/external"
  )
  manumesh_apply_warnings(manumesh_performance_tests)
  manumesh_copy_runtime_dependencies(manumesh_performance_tests)
  manumesh_copy_gtest_runtime_dependencies(manumesh_performance_tests)

  gtest_add_tests(TARGET manumesh_performance_tests
    SOURCES ${MANUMESH_PERFORMANCE_TEST_SOURCES}
    TEST_LIST MANUMESH_PERFORMANCE_TESTS
  )
  if(MANUMESH_PERFORMANCE_TESTS)
    set_tests_properties(${MANUMESH_PERFORMANCE_TESTS}
      PROPERTIES
        LABELS performance
        RUN_SERIAL TRUE
    )
  endif()

  add_custom_target(performance-tests
    COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_BINARY_DIR}"
      "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -L performance --output-on-failure
    DEPENDS manumesh_performance_tests
    COMMENT "Running large mesh performance tests"
    VERBATIM
  )
endif()
