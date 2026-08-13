add_executable(manumesh_basic_simplify basic_simplify.cpp)
target_link_libraries(manumesh_basic_simplify
  PRIVATE ManuMesh::manumesh
)
manumesh_apply_warnings(manumesh_basic_simplify)
manumesh_copy_runtime_dependencies(manumesh_basic_simplify)

add_executable(manumesh_c_api_basic c_api_basic.c)
set_target_properties(manumesh_c_api_basic PROPERTIES
  LINKER_LANGUAGE CXX
)
target_link_libraries(manumesh_c_api_basic
  PRIVATE ManuMesh::c_api
)
manumesh_apply_warnings(manumesh_c_api_basic)
manumesh_copy_runtime_dependencies(manumesh_c_api_basic)

add_executable(manumesh_feature_workflow_demo feature_workflow_demo.cpp)
target_link_libraries(manumesh_feature_workflow_demo
  PRIVATE ManuMesh::manumesh
)
manumesh_apply_warnings(manumesh_feature_workflow_demo)
manumesh_copy_runtime_dependencies(manumesh_feature_workflow_demo)

if(BUILD_TESTING)
  add_test(NAME manumesh_example_basic_simplify
    COMMAND manumesh_basic_simplify
  )
  add_test(NAME manumesh_example_c_api_basic
    COMMAND manumesh_c_api_basic
  )
  add_test(NAME manumesh_example_feature_workflow
    COMMAND manumesh_feature_workflow_demo
  )
endif()
