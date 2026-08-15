cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING
  "MSVC runtime for ManuMesh SDK consumers; Debug and Release both use /MD."
  FORCE
)
set_property(CACHE CMAKE_MSVC_RUNTIME_LIBRARY PROPERTY STRINGS MultiThreadedDLL)

project(manumesh_sdk_consumer
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_GENERATOR MATCHES "^Visual Studio " AND
   NOT CMAKE_GENERATOR STREQUAL "Visual Studio 16 2019")
  message(FATAL_ERROR
    "The ManuMesh SDK consumer supports only the Visual Studio 16 2019 "
    "generator; detected '${CMAKE_GENERATOR}'."
  )
endif()

if(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR
   MSVC_VERSION LESS 1920 OR MSVC_VERSION GREATER_EQUAL 1930)
  message(FATAL_ERROR
    "The ManuMesh SDK consumer requires Visual Studio 2019 / MSVC v142."
  )
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT CMAKE_C_COMPILER_ARCHITECTURE_ID MATCHES "^[Xx]64$" OR
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^[Xx]64$")
  message(FATAL_ERROR
    "The ManuMesh SDK consumer requires the x64 target architecture."
  )
endif()

option(MANUMESH_SDK_C_API_ONLY
  "Build only the installed SDK's pure C ABI consumers"
  OFF
)

set(MANUMESH_SDK_ROOT "" CACHE PATH "已安装 manumesh SDK 根目录")
if(NOT MANUMESH_SDK_ROOT)
  message(FATAL_ERROR "请将 MANUMESH_SDK_ROOT 设置为已安装的 manumesh SDK 根目录。")
endif()

list(PREPEND CMAKE_PREFIX_PATH "${MANUMESH_SDK_ROOT}")
if(MANUMESH_SDK_C_API_ONLY)
  if(DEFINED CMAKE_DISABLE_FIND_PACKAGE_Eigen3)
    message(STATUS
      "Eigen package discovery disabled: ${CMAKE_DISABLE_FIND_PACKAGE_Eigen3}"
    )
  endif()
  find_package(ManuMesh CONFIG REQUIRED COMPONENTS CApi)
else()
  find_package(ManuMesh CONFIG REQUIRED COMPONENTS CXX CApi)
endif()

get_target_property(MANUMESH_C_API_LINK_LIBRARIES
  ManuMesh::c_api INTERFACE_LINK_LIBRARIES
)
if(MANUMESH_C_API_LINK_LIBRARIES MATCHES "Eigen")
  message(FATAL_ERROR "ManuMesh::c_api must not propagate Eigen")
endif()

get_target_property(MANUMESH_C_API_COMPILE_FEATURES
  ManuMesh::c_api INTERFACE_COMPILE_FEATURES
)
if(MANUMESH_C_API_COMPILE_FEATURES)
  message(FATAL_ERROR "ManuMesh::c_api must not propagate C++ compile features")
endif()

if(NOT MANUMESH_SDK_C_API_ONLY)
  if(NOT TARGET Eigen3::Eigen)
    message(FATAL_ERROR "The CXX component must provide Eigen3::Eigen")
  endif()
  get_target_property(MANUMESH_CPP_COMPILE_FEATURES
    ManuMesh::manumesh INTERFACE_COMPILE_FEATURES
  )
  if(NOT cxx_std_14 IN_LIST MANUMESH_CPP_COMPILE_FEATURES)
    message(FATAL_ERROR "ManuMesh::manumesh must propagate C++14")
  endif()
  get_target_property(MANUMESH_CPP_LINK_LIBRARIES
    ManuMesh::manumesh INTERFACE_LINK_LIBRARIES
  )
  if(NOT MANUMESH_CPP_LINK_LIBRARIES MATCHES "Eigen3::Eigen")
    message(FATAL_ERROR "ManuMesh::manumesh must propagate Eigen3::Eigen")
  endif()
  get_target_property(MANUMESH_CPP_COMPILE_DEFINITIONS
    ManuMesh::manumesh INTERFACE_COMPILE_DEFINITIONS
  )
  if(MANUMESH_SHARED AND
     NOT MANUMESH_USING_DLL IN_LIST MANUMESH_CPP_COMPILE_DEFINITIONS)
    message(FATAL_ERROR
      "The shared ManuMesh::manumesh target must propagate MANUMESH_USING_DLL"
    )
  endif()
endif()

if(MANUMESH_SDK_C_API_ONLY)
  add_executable(sdk_c_abi_package_smoke sdk_c_abi_package_smoke.c)
  set_target_properties(sdk_c_abi_package_smoke PROPERTIES
    LINKER_LANGUAGE CXX
  )
  target_link_libraries(sdk_c_abi_package_smoke PRIVATE ManuMesh::c_api)
  manumesh_configure_msvc_runtime(sdk_c_abi_package_smoke)
  manumesh_copy_runtime_dependencies(sdk_c_abi_package_smoke)
else()
  add_executable(sdk_cpp_simplify sdk_cpp_simplify.cpp)
  target_link_libraries(sdk_cpp_simplify PRIVATE ManuMesh::manumesh)
  manumesh_configure_msvc_runtime(sdk_cpp_simplify)
  manumesh_copy_runtime_dependencies(sdk_cpp_simplify)

  add_executable(sdk_cpp_c_api_feature_edges sdk_cpp_c_api_feature_edges.cpp)
  target_compile_features(sdk_cpp_c_api_feature_edges PRIVATE cxx_std_14)
  target_link_libraries(sdk_cpp_c_api_feature_edges PRIVATE ManuMesh::c_api)
  manumesh_configure_msvc_runtime(sdk_cpp_c_api_feature_edges)
  manumesh_copy_runtime_dependencies(sdk_cpp_c_api_feature_edges)

  add_executable(sdk_c_abi_basic sdk_c_abi_basic.c)
  set_target_properties(sdk_c_abi_basic PROPERTIES
    LINKER_LANGUAGE CXX
  )
  target_link_libraries(sdk_c_abi_basic PRIVATE ManuMesh::c_api)
  manumesh_configure_msvc_runtime(sdk_c_abi_basic)
  manumesh_copy_runtime_dependencies(sdk_c_abi_basic)

  add_executable(sdk_c_abi_legacy_init sdk_c_abi_legacy_init.c)
  set_target_properties(sdk_c_abi_legacy_init PROPERTIES
    LINKER_LANGUAGE CXX
  )
  target_link_libraries(sdk_c_abi_legacy_init PRIVATE ManuMesh::c_api)
  manumesh_configure_msvc_runtime(sdk_c_abi_legacy_init)
  manumesh_copy_runtime_dependencies(sdk_c_abi_legacy_init)
endif()

enable_testing()
if(MANUMESH_SDK_C_API_ONLY)
  add_test(NAME sdk_c_abi_package_smoke COMMAND sdk_c_abi_package_smoke)
else()
  add_test(NAME sdk_cpp_simplify COMMAND sdk_cpp_simplify)
  add_test(NAME sdk_cpp_c_api_feature_edges COMMAND sdk_cpp_c_api_feature_edges)
  add_test(NAME sdk_c_abi_basic COMMAND sdk_c_abi_basic)
  add_test(NAME sdk_c_abi_legacy_init COMMAND sdk_c_abi_legacy_init)
endif()
