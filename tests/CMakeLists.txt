add_executable(line_quadrics_qem_tests
  c_api_tests.cpp
  simplifier_tests.cpp
)
target_link_libraries(line_quadrics_qem_tests
  PRIVATE
    line_quadrics_qem::line_quadrics_qem
    GTest::gtest_main
)
lq_apply_warnings(line_quadrics_qem_tests)
lq_copy_runtime_dependencies(line_quadrics_qem_tests)

include(GoogleTest)
gtest_discover_tests(line_quadrics_qem_tests)
