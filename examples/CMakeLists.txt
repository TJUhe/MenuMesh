add_executable(linequadrics_basic_simplify basic_simplify.cpp)
target_link_libraries(linequadrics_basic_simplify
  PRIVATE line_quadrics_qem::line_quadrics_qem
)
lq_apply_warnings(linequadrics_basic_simplify)
lq_copy_runtime_dependencies(linequadrics_basic_simplify)

add_executable(linequadrics_c_api_basic c_api_basic.c)
set_target_properties(linequadrics_c_api_basic PROPERTIES
  LINKER_LANGUAGE CXX
)
target_link_libraries(linequadrics_c_api_basic
  PRIVATE line_quadrics_qem::line_quadrics_qem
)
lq_apply_warnings(linequadrics_c_api_basic)
lq_copy_runtime_dependencies(linequadrics_c_api_basic)

if(BUILD_TESTING)
  add_test(NAME linequadrics_example_basic_simplify
    COMMAND linequadrics_basic_simplify
  )
  add_test(NAME linequadrics_example_c_api_basic
    COMMAND linequadrics_c_api_basic
  )
endif()
