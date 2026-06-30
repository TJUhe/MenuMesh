add_executable(linequadrics_basic_simplify basic_simplify.cpp)
target_link_libraries(linequadrics_basic_simplify
  PRIVATE line_quadrics_qem::line_quadrics_qem
)
lq_apply_warnings(linequadrics_basic_simplify)
lq_copy_runtime_dependencies(linequadrics_basic_simplify)

if(BUILD_TESTING)
  add_test(NAME linequadrics_example_basic_simplify
    COMMAND linequadrics_basic_simplify
  )
endif()
