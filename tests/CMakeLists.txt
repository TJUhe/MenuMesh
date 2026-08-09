include(FetchContent)

option(MANUMESH_BUILD_PERFORMANCE_TESTS "构建大型网格性能测试" OFF)

set(MANUMESH_GOOGLETEST_PROVIDER "auto" CACHE STRING
  "测试的 GoogleTest 提供方式：auto、source、prebuilt、system 或 fetch"
)
set_property(CACHE MANUMESH_GOOGLETEST_PROVIDER PROPERTY STRINGS
  auto
  source
  prebuilt
  system
  fetch
)
option(MANUMESH_MINGW_AUTO_GOOGLETEST_FROM_SOURCE
  "MinGW 下 MANUMESH_GOOGLETEST_PROVIDER=auto 时不使用预构建 GoogleTest DLL，而是为当前编译器构建 GoogleTest。"
  ON
)
set(MANUMESH_GOOGLETEST_SOURCE_DIR
  "${PROJECT_SOURCE_DIR}/thirdParty/googletest/source/googletest"
  CACHE PATH "包含 CMakeLists.txt 的 GoogleTest 源码树路径"
)
set(MANUMESH_DEFAULT_GOOGLETEST_PREBUILT_DIR
  "${PROJECT_SOURCE_DIR}/thirdParty/googletest/prebuilt/mingw-x64-shared"
)
if(MSVC)
  set(MANUMESH_DEFAULT_GOOGLETEST_PREBUILT_DIR
    "${PROJECT_SOURCE_DIR}/thirdParty/googletest/prebuilt/msvc-x64-static"
  )
endif()
set(MANUMESH_GOOGLETEST_PREBUILT_DIR
  "${MANUMESH_DEFAULT_GOOGLETEST_PREBUILT_DIR}"
  CACHE PATH "包含 include、lib 和 bin 目录的预构建 GoogleTest 包根目录"
)
set(MANUMESH_GTEST_INCLUDE_DIR "" CACHE PATH "预构建 GoogleTest 头文件路径")
set(MANUMESH_GTEST_LIBRARY "" CACHE FILEPATH "预构建 gtest 库路径")
set(MANUMESH_GTEST_MAIN_LIBRARY "" CACHE FILEPATH
  "预构建 gtest_main 库路径"
)
set(MANUMESH_GTEST_RUNTIME_DIR "" CACHE PATH
  "预构建 GoogleTest 运行时 DLL/共享库目录（可选）"
)

function(manumesh_copy_gtest_runtime_dependencies target)
  if(MANUMESH_RESOLVED_GTEST_RUNTIME_DIR AND
     EXISTS "${MANUMESH_RESOLVED_GTEST_RUNTIME_DIR}")
    file(GLOB MANUMESH_GTEST_RUNTIME_FILES CONFIGURE_DEPENDS
      "${MANUMESH_RESOLVED_GTEST_RUNTIME_DIR}/*.dll"
      "${MANUMESH_RESOLVED_GTEST_RUNTIME_DIR}/*.so"
      "${MANUMESH_RESOLVED_GTEST_RUNTIME_DIR}/*.dylib"
    )
    foreach(gtest_runtime_file IN LISTS MANUMESH_GTEST_RUNTIME_FILES)
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${gtest_runtime_file}"
          "$<TARGET_FILE_DIR:${target}>"
    COMMENT "正在将预构建 GoogleTest 运行时复制到 ${target} 旁边"
        VERBATIM
      )
    endforeach()
  endif()
endfunction()

function(manumesh_import_prebuilt_gtest bundle_root)
  if(TARGET GTest::gtest_main)
    return()
  endif()

  set(gtest_include "${MANUMESH_GTEST_INCLUDE_DIR}")
  set(gtest_library "${MANUMESH_GTEST_LIBRARY}")
  set(gtest_main_library "${MANUMESH_GTEST_MAIN_LIBRARY}")
  set(gtest_runtime_dir "${MANUMESH_GTEST_RUNTIME_DIR}")
  set(gtest_library_debug "")
  set(gtest_library_release "")
  set(gtest_main_library_debug "")
  set(gtest_main_library_release "")

  if(NOT gtest_include)
    set(gtest_include "${bundle_root}/include")
  endif()

  if(NOT gtest_library)
    if(EXISTS "${bundle_root}/lib/Debug/gtest.lib")
      set(gtest_library_debug "${bundle_root}/lib/Debug/gtest.lib")
    endif()
    if(EXISTS "${bundle_root}/lib/Release/gtest.lib")
      set(gtest_library_release "${bundle_root}/lib/Release/gtest.lib")
    endif()
    foreach(candidate IN ITEMS
        "${bundle_root}/lib/gtest.lib"
        "${bundle_root}/lib/libgtest.lib"
        "${bundle_root}/lib/libgtest.dll.a"
        "${bundle_root}/lib/libgtest.a")
      if(EXISTS "${candidate}")
        set(gtest_library "${candidate}")
        break()
      endif()
    endforeach()
  endif()

  if(NOT gtest_main_library)
    if(EXISTS "${bundle_root}/lib/Debug/gtest_main.lib")
      set(gtest_main_library_debug "${bundle_root}/lib/Debug/gtest_main.lib")
    endif()
    if(EXISTS "${bundle_root}/lib/Release/gtest_main.lib")
      set(gtest_main_library_release "${bundle_root}/lib/Release/gtest_main.lib")
    endif()
    foreach(candidate IN ITEMS
        "${bundle_root}/lib/gtest_main.lib"
        "${bundle_root}/lib/libgtest_main.lib"
        "${bundle_root}/lib/libgtest_main.dll.a"
        "${bundle_root}/lib/libgtest_main.a")
      if(EXISTS "${candidate}")
        set(gtest_main_library "${candidate}")
        break()
      endif()
    endforeach()
  endif()

  if(NOT gtest_runtime_dir)
    set(gtest_runtime_dir "${bundle_root}/bin")
  endif()

  if(NOT EXISTS "${gtest_include}/gtest/gtest.h" OR
     ((NOT gtest_library OR NOT gtest_main_library) AND
      (NOT gtest_library_debug OR NOT gtest_main_library_debug)))
    return()
  endif()

  add_library(GTest::gtest UNKNOWN IMPORTED)
  set_target_properties(GTest::gtest PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${gtest_include}"
  )
  if(gtest_library_debug OR gtest_library_release)
    if(NOT gtest_library_release)
      set(gtest_library_release "${gtest_library_debug}")
    endif()
    set_target_properties(GTest::gtest PROPERTIES
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE;RELWITHDEBINFO;MINSIZEREL"
      IMPORTED_LOCATION_DEBUG "${gtest_library_debug}"
      IMPORTED_LOCATION_RELEASE "${gtest_library_release}"
      IMPORTED_LOCATION_RELWITHDEBINFO "${gtest_library_release}"
      IMPORTED_LOCATION_MINSIZEREL "${gtest_library_release}"
    )
  else()
    set_target_properties(GTest::gtest PROPERTIES
      IMPORTED_LOCATION "${gtest_library}"
    )
  endif()

  add_library(GTest::gtest_main UNKNOWN IMPORTED)
  set_target_properties(GTest::gtest_main PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${gtest_include}"
    INTERFACE_LINK_LIBRARIES GTest::gtest
  )
  if(gtest_main_library_debug OR gtest_main_library_release)
    if(NOT gtest_main_library_release)
      set(gtest_main_library_release "${gtest_main_library_debug}")
    endif()
    set_target_properties(GTest::gtest_main PROPERTIES
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE;RELWITHDEBINFO;MINSIZEREL"
      IMPORTED_LOCATION_DEBUG "${gtest_main_library_debug}"
      IMPORTED_LOCATION_RELEASE "${gtest_main_library_release}"
      IMPORTED_LOCATION_RELWITHDEBINFO "${gtest_main_library_release}"
      IMPORTED_LOCATION_MINSIZEREL "${gtest_main_library_release}"
    )
  else()
    set_target_properties(GTest::gtest_main PROPERTIES
      IMPORTED_LOCATION "${gtest_main_library}"
    )
  endif()
  set(MANUMESH_RESOLVED_GTEST_RUNTIME_DIR "${gtest_runtime_dir}" PARENT_SCOPE)
endfunction()

function(manumesh_add_source_gtest source_dir)
  if(TARGET GTest::gtest_main)
    return()
  endif()
  if(NOT EXISTS "${source_dir}/CMakeLists.txt")
    return()
  endif()

  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_build_samples OFF CACHE BOOL "" FORCE)
  set(gtest_build_tests OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  if(NOT DEFINED GOOGLETEST_VERSION)
    set(GOOGLETEST_VERSION 1.15.2)
  endif()
  add_subdirectory(
    "${source_dir}"
    "${CMAKE_BINARY_DIR}/_deps/manumesh-googletest-build"
    EXCLUDE_FROM_ALL
  )
  if(TARGET gtest AND NOT TARGET GTest::gtest)
    add_library(GTest::gtest ALIAS gtest)
  endif()
  if(TARGET gtest_main AND NOT TARGET GTest::gtest_main)
    add_library(GTest::gtest_main ALIAS gtest_main)
  endif()
endfunction()

if(NOT MANUMESH_GOOGLETEST_PROVIDER MATCHES "^(auto|source|prebuilt|system|fetch)$")
  message(FATAL_ERROR
    "MANUMESH_GOOGLETEST_PROVIDER 必须是 auto、source、prebuilt、system 或 fetch 之一。"
  )
endif()

set(MANUMESH_SKIP_PREBUILT_GTEST_FOR_MINGW_AUTO OFF)
if(MINGW AND MANUMESH_MINGW_AUTO_GOOGLETEST_FROM_SOURCE AND
   MANUMESH_GOOGLETEST_PROVIDER STREQUAL "auto")
  set(MANUMESH_SKIP_PREBUILT_GTEST_FOR_MINGW_AUTO ON)
endif()

if(MANUMESH_GOOGLETEST_PROVIDER STREQUAL "source" OR
   MANUMESH_SKIP_PREBUILT_GTEST_FOR_MINGW_AUTO)
  manumesh_add_source_gtest("${MANUMESH_GOOGLETEST_SOURCE_DIR}")
  if(NOT TARGET GTest::gtest_main AND MANUMESH_GOOGLETEST_PROVIDER STREQUAL "source")
    message(FATAL_ERROR
      "在以下路径未找到 GoogleTest 源码："
      "MANUMESH_GOOGLETEST_SOURCE_DIR='${MANUMESH_GOOGLETEST_SOURCE_DIR}'。"
    )
  endif()
endif()

if((MANUMESH_GOOGLETEST_PROVIDER STREQUAL "auto" AND
    NOT MANUMESH_SKIP_PREBUILT_GTEST_FOR_MINGW_AUTO) OR
   MANUMESH_GOOGLETEST_PROVIDER STREQUAL "prebuilt")
  manumesh_import_prebuilt_gtest("${MANUMESH_GOOGLETEST_PREBUILT_DIR}")
  if(NOT TARGET GTest::gtest_main AND
     MANUMESH_GOOGLETEST_PROVIDER STREQUAL "prebuilt")
    message(FATAL_ERROR
      "未找到预构建 GoogleTest。预期二进制包包含 include/gtest/gtest.h、lib/gtest、lib/gtest_main，"
      "以及可选的 bin 运行时文件，位置为 MANUMESH_GOOGLETEST_PREBUILT_DIR='${MANUMESH_GOOGLETEST_PREBUILT_DIR}'；"
      "也可以设置 MANUMESH_GTEST_INCLUDE_DIR、MANUMESH_GTEST_LIBRARY 和 MANUMESH_GTEST_MAIN_LIBRARY。"
    )
  endif()
endif()

if(MANUMESH_GOOGLETEST_PROVIDER STREQUAL "auto" OR
   MANUMESH_GOOGLETEST_PROVIDER STREQUAL "system")
  if(NOT TARGET GTest::gtest_main)
    find_package(GTest QUIET)
    if(TARGET GTest::Main AND NOT TARGET GTest::gtest_main)
      add_library(GTest::gtest_main INTERFACE IMPORTED)
      target_link_libraries(GTest::gtest_main INTERFACE GTest::Main)
    endif()
  endif()
endif()

if(NOT TARGET GTest::gtest_main AND
   (MANUMESH_GOOGLETEST_PROVIDER STREQUAL "auto" OR
    MANUMESH_GOOGLETEST_PROVIDER STREQUAL "fetch"))
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    ${MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS}
  )
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

if(NOT TARGET GTest::gtest_main)
  message(FATAL_ERROR
      "未找到 GoogleTest。请使用 thirdParty/googletest/prebuilt 下的预构建二进制包、"
      "thirdParty/googletest/source 下的 vendored 源码树，安装可由 find_package(GTest) 发现的系统 GTest，"
      "或配置 -DMANUMESH_GOOGLETEST_PROVIDER=fetch。MinGW 下默认 auto 提供方式会跳过预构建 GoogleTest DLL，"
      "并从 vendored 源码为当前编译器构建 GoogleTest；只有二进制包与所选 MinGW 运行时匹配时，才应使用"
      "-DMANUMESH_GOOGLETEST_PROVIDER=prebuilt。"
  )
endif()

set(MANUMESH_UNIT_TEST_SOURCES
  support/AnalyticFixtures.cpp
  support/TestSupport.cpp
  unit/common/geometry_predicates_tests.cpp
  unit/common/mesh_distance_index_tests.cpp
  unit/common/spatial_index_tests.cpp
  unit/core/core_tests.cpp
  unit/mesh_edit/mesh_edit_tests.cpp
  unit/api/c_api_diagnostics_tests.cpp
  unit/api/c_api_legacy_init_tests.cpp
  unit/api/c_api_mesh_tests.cpp
  unit/api/c_api_simplify_abi_tests.cpp
  unit/feature_detection/FeatureDetectionTestSupport.cpp
  unit/feature_detection/feature_detection_analytic_tests.cpp
  unit/feature_detection/feature_detection_api_tests.cpp
  unit/feature_detection/feature_comparison_tests.cpp
  unit/feature_detection/feature_detection_cleanup_tests.cpp
  unit/feature_detection/feature_detection_discrete_tests.cpp
  unit/feature_detection/feature_detection_fixture_tests.cpp
  unit/feature_detection/feature_detection_loop_tests.cpp
    unit/feature_detection/feature_detection_perf_tests.cpp
    unit/feature_detection/feature_detection_pipeline_upgrade_tests.cpp
    unit/feature_detection/feature_detection_primitive_fit_tests.cpp
  unit/feature_detection/feature_detection_robustness_tests.cpp
  unit/feature_detection/feature_detection_smooth_curvature_tests.cpp
  unit/io/mesh_io_tests.cpp
  unit/perf/pipeline_perf_guard_tests.cpp
  unit/simplification/qem_parameter_budget_tests.cpp
  unit/simplification/qem_parameter_feature_tests.cpp
  unit/simplification/qem_parameter_industrial_tests.cpp
  unit/simplification/simplification_analytic_tests.cpp
  unit/simplification/simplification_api_tests.cpp
  unit/simplification/simplification_boundary_topology_tests.cpp
  unit/simplification/simplification_core_tests.cpp
  unit/simplification/simplification_feature_protection_tests.cpp
  unit/simplification/simplification_legality_tests.cpp
  unit/simplification/simplification_normal_tensor_tests.cpp
  unit/simplification/simplification_placement_tests.cpp
  unit/simplification/simplification_quality_refinement_tests.cpp
  unit/simplification/simplification_texture_tests.cpp
)

# 此源文件在首次包含 CApi.h 前禁用公开的 size-aware 别名，以便测试旧版导出符号。
set_source_files_properties(
  unit/api/c_api_legacy_init_tests.cpp
  PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

# 需要 tests/data/external 大网格（或运行数十秒）的用例放在标记为 `external` 的独立可执行文件中，
# 因此 `ctest -LE "performance|external"` 仍可保持快速测试集。
set(MANUMESH_EXTERNAL_TEST_SOURCES
  support/TestSupport.cpp
  unit/api/c_api_external_tests.cpp
  unit/feature_detection/FeatureDetectionTestSupport.cpp
  unit/feature_detection/feature_detection_external_tests.cpp
  unit/simplification/qem_parameter_external_tests.cpp
  unit/simplification/simplification_external_tests.cpp
)

# 两个测试可执行文件都链接内部 STATIC 聚合库（manumesh_internal），而不是 DLL。
# 该聚合库复用库对象文件，因此内部源文件不会重复编译，白盒 detail 头文件也不会产生第二份符号定义（无 ODR 风险）。
# 黑盒 DLL 边界覆盖由 CLI 冒烟测试和 SDK consumer 示例承担；它们链接常规的共享 ManuMesh::manumesh。
function(manumesh_configure_test_target target)
  target_include_directories(${target}
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/support"
      "${PROJECT_SOURCE_DIR}/src"
      "${PROJECT_SOURCE_DIR}/src/feature_detection"
      "${PROJECT_SOURCE_DIR}/src/simplification"
  )
  target_link_libraries(${target}
    PRIVATE
      manumesh_internal
      GTest::gtest_main
  )
  target_compile_definitions(${target}
    PRIVATE
      MANUMESH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
      MANUMESH_TEST_EXTERNAL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/external"
  )
  manumesh_apply_warnings(${target})
  manumesh_copy_mingw_runtime_dependencies(${target})
  manumesh_copy_gtest_runtime_dependencies(${target})
endfunction()

add_executable(manumesh_tests ${MANUMESH_UNIT_TEST_SOURCES})
add_executable(manumesh_external_tests ${MANUMESH_EXTERNAL_TEST_SOURCES})
if(MINGW)
  set_source_files_properties(
    unit/feature_detection/feature_detection_primitive_fit_tests.cpp
    PROPERTIES COMPILE_OPTIONS "-Wa,-mbig-obj"
  )
endif()
manumesh_configure_test_target(manumesh_tests)
manumesh_configure_test_target(manumesh_external_tests)

include(GoogleTest)
# PRE_TEST 发现：新的 TEST() 用例会在 CTest 运行时被发现，无需重新配置 CMake；
# 可执行文件也不会在构建时运行，从而避免 Windows + MinGW 下的不稳定行为。
gtest_discover_tests(manumesh_tests
  DISCOVERY_MODE PRE_TEST
  DISCOVERY_TIMEOUT 120
  PROPERTIES LABELS unit
)
gtest_discover_tests(manumesh_external_tests
  DISCOVERY_MODE PRE_TEST
  DISCOVERY_TIMEOUT 120
  PROPERTIES LABELS external
)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME include_boundaries
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/support/check_include_boundaries.py"
      "${PROJECT_SOURCE_DIR}"
  )
  add_test(NAME include_boundary_checker_tests
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/support/check_include_boundaries_tests.py"
  )
  set_tests_properties(include_boundaries include_boundary_checker_tests
    PROPERTIES LABELS architecture
  )
else()
message(STATUS "未找到 Python3；已禁用 include 边界 CTest。")
endif()

set(MANUMESH_UNIT_TEST_DEPENDS manumesh_tests manumesh_external_tests)
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
    "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -LE "performance|external"
    --output-on-failure
  DEPENDS ${MANUMESH_UNIT_TEST_DEPENDS}
  COMMENT "正在运行快速回归测试集（不含性能/外部测试）"
  VERBATIM
)

add_custom_target(external-tests
  COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_BINARY_DIR}"
    "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -L external --output-on-failure
  DEPENDS manumesh_external_tests
  COMMENT "正在运行外部大型网格回归测试"
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

  gtest_discover_tests(manumesh_performance_tests
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 120
    PROPERTIES LABELS performance RUN_SERIAL TRUE
  )

  add_custom_target(performance-tests
    COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_BINARY_DIR}"
      "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -L performance --output-on-failure
    DEPENDS manumesh_performance_tests
  COMMENT "正在运行大型网格性能测试"
    VERBATIM
  )
endif()
