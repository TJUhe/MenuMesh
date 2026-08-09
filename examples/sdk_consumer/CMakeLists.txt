cmake_minimum_required(VERSION 3.18)

project(manumesh_sdk_consumer
  LANGUAGES C CXX
)

set(MANUMESH_SDK_ROOT "" CACHE PATH "已安装 manumesh SDK 根目录")
if(NOT MANUMESH_SDK_ROOT)
  message(FATAL_ERROR "请将 MANUMESH_SDK_ROOT 设置为已安装的 manumesh SDK 根目录。")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${MANUMESH_SDK_ROOT}")
find_package(ManuMesh CONFIG REQUIRED)

add_executable(sdk_cpp_simplify sdk_cpp_simplify.cpp)
target_compile_features(sdk_cpp_simplify PRIVATE cxx_std_17)
target_link_libraries(sdk_cpp_simplify PRIVATE ManuMesh::manumesh)
manumesh_copy_runtime_dependencies(sdk_cpp_simplify)

add_executable(sdk_c_abi_basic sdk_c_abi_basic.c)
set_target_properties(sdk_c_abi_basic PROPERTIES
  LINKER_LANGUAGE CXX
)
target_link_libraries(sdk_c_abi_basic PRIVATE ManuMesh::manumesh)
manumesh_copy_runtime_dependencies(sdk_c_abi_basic)

add_executable(sdk_c_abi_legacy_init sdk_c_abi_legacy_init.c)
set_target_properties(sdk_c_abi_legacy_init PROPERTIES
  LINKER_LANGUAGE CXX
)
target_link_libraries(sdk_c_abi_legacy_init PRIVATE ManuMesh::manumesh)
manumesh_copy_runtime_dependencies(sdk_c_abi_legacy_init)

enable_testing()
add_test(NAME sdk_cpp_simplify COMMAND sdk_cpp_simplify)
add_test(NAME sdk_c_abi_basic COMMAND sdk_c_abi_basic)
add_test(NAME sdk_c_abi_legacy_init COMMAND sdk_c_abi_legacy_init)
