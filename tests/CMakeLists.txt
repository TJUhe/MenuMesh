include(FetchContent)

option(MANUMESH_BUILD_PERFORMANCE_TESTS "构建大型网格性能测试" OFF)
option(MANUMESH_REQUIRE_ARCHITECTURE_CHECKS
  "要求 Python 架构边界检查存在；启用时缺少 Python 会使配置失败"
  OFF
)

set(MANUMESH_GOOGLETEST_PROVIDER "source" CACHE STRING
  "测试的 GoogleTest 提供方式：默认 source；可选 auto、system 或 fetch"
)
set_property(CACHE MANUMESH_GOOGLETEST_PROVIDER PROPERTY STRINGS
  auto
  source
  system
  fetch
)
set(MANUMESH_GOOGLETEST_SOURCE_DIR
  "${PROJECT_SOURCE_DIR}/thirdParty/googletest/source/googletest"
  CACHE PATH "包含 CMakeLists.txt 的 GoogleTest 源码树路径"
)

# 清除旧构建目录中已删除的预编译 provider 缓存项。
foreach(manumesh_removed_gtest_cache_entry IN ITEMS
    MANUMESH_GOOGLETEST_PREBUILT_DIR
    MANUMESH_GTEST_INCLUDE_DIR
    MANUMESH_GTEST_LIBRARY
    MANUMESH_GTEST_MAIN_LIBRARY
    MANUMESH_GTEST_RUNTIME_DIR)
  unset(${manumesh_removed_gtest_cache_entry} CACHE)
endforeach()
unset(manumesh_removed_gtest_cache_entry)

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

if(NOT MANUMESH_GOOGLETEST_PROVIDER MATCHES "^(auto|source|system|fetch)$")
  message(FATAL_ERROR
    "MANUMESH_GOOGLETEST_PROVIDER 必须是 auto、source、system 或 fetch 之一。"
  )
endif()

if(MANUMESH_GOOGLETEST_PROVIDER STREQUAL "auto" OR
   MANUMESH_GOOGLETEST_PROVIDER STREQUAL "source")
  manumesh_add_source_gtest("${MANUMESH_GOOGLETEST_SOURCE_DIR}")
  if(NOT TARGET GTest::gtest_main AND MANUMESH_GOOGLETEST_PROVIDER STREQUAL "source")
    message(FATAL_ERROR
      "在以下路径未找到 GoogleTest 源码："
      "MANUMESH_GOOGLETEST_SOURCE_DIR='${MANUMESH_GOOGLETEST_SOURCE_DIR}'。"
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
    URL_HASH SHA256=F179EC217F9B3B3F3C6E8B02D3E7EDA997B49E4CE26D6B235C9053BEC9C0BF9F
    ${MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS}
  )
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

if(NOT TARGET GTest::gtest_main)
  message(FATAL_ERROR
      "未找到 GoogleTest。请确认 thirdParty/googletest/source 下的 vendored 源码树可用，"
      "安装可由 find_package(GTest) 发现的系统 GTest，"
      "或配置 -DMANUMESH_GOOGLETEST_PROVIDER=fetch。"
  )
endif()

set(MANUMESH_UNIT_TEST_SOURCES
  "${PROJECT_SOURCE_DIR}/apps/CliArguments.cpp"
  "${PROJECT_SOURCE_DIR}/apps/CliCsv.cpp"
  "${PROJECT_SOURCE_DIR}/apps/CliOptionBinding.cpp"
  support/AnalyticFixtures.cpp
  support/TestSupport.cpp
  unit/apps/cli_option_binding_tests.cpp
  unit/common/geometry_predicates_tests.cpp
  unit/common/mesh_distance_index_tests.cpp
  unit/common/parallel_execution_tests.cpp
  unit/common/spatial_index_tests.cpp
  unit/core/core_tests.cpp
  unit/mesh_edit/mesh_edit_tests.cpp
  unit/api/c_api_diagnostics_tests.cpp
  unit/api/c_api_execution_tests.cpp
  unit/api/c_api_feature_edges_tests.cpp
  unit/api/c_api_legacy_init_tests.cpp
  unit/api/c_api_mesh_tests.cpp
  unit/api/c_api_simplify_abi_tests.cpp
  unit/feature_detection/FeatureDetectionTestSupport.cpp
  unit/feature_detection/feature_analysis_views_tests.cpp
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
  unit/feature_detection/feature_detection_parallel_tests.cpp
  unit/feature_detection/feature_detection_robustness_tests.cpp
  unit/io/mesh_io_tests.cpp
  unit/io/partitioned_mesh_dataset_tests.cpp
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
  unit/simplification/simplification_parallel_tests.cpp
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
  unit/io/partitioned_mesh_dataset_external_tests.cpp
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
      "${PROJECT_SOURCE_DIR}/apps"
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
  manumesh_copy_runtime_dependencies(${target})
  manumesh_apply_warnings(${target})
endfunction()

add_executable(manumesh_tests ${MANUMESH_UNIT_TEST_SOURCES})
add_executable(manumesh_external_tests ${MANUMESH_EXTERNAL_TEST_SOURCES})
set_source_files_properties(
  unit/feature_detection/feature_detection_primitive_fit_tests.cpp
  PROPERTIES COMPILE_OPTIONS "/bigobj"
)
manumesh_configure_test_target(manumesh_tests)
manumesh_configure_test_target(manumesh_external_tests)

# The optional large-fixture test consumes the exact index generated by the
# manifest CTest below. Keep the JSON parser and hash validation in the CMake
# support layer rather than adding a JSON dependency to the test binary.
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/thingi10k_manifest.index" MANUMESH_THINGI10K_INDEX_PATH)
target_compile_definitions(manumesh_external_tests
  PRIVATE MANUMESH_THINGI10K_MANIFEST_INDEX="${MANUMESH_THINGI10K_INDEX_PATH}"
)

add_executable(manumesh_ownership_lifetime_stress
  memory/ownership_lifetime_stress.cpp
)
target_compile_features(manumesh_ownership_lifetime_stress PRIVATE cxx_std_14)
target_include_directories(manumesh_ownership_lifetime_stress
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(manumesh_ownership_lifetime_stress
  PRIVATE manumesh_internal
)
manumesh_copy_runtime_dependencies(manumesh_ownership_lifetime_stress)
set_target_properties(manumesh_ownership_lifetime_stress PROPERTIES
  FOLDER "testing/memory"
)
manumesh_apply_warnings(manumesh_ownership_lifetime_stress)
add_dependencies(manumesh_tests manumesh_ownership_lifetime_stress)
add_test(NAME ownership_lifetime_stress COMMAND manumesh_ownership_lifetime_stress)
set_tests_properties(ownership_lifetime_stress PROPERTIES
  LABELS "memory;unit"
  TIMEOUT 300
)

# Compile pure C and C++ consumers with non-default MSVC calling-convention
# and packing flags. Link the public target so the checks cross the DLL/import
# library boundary in shared builds.
function(manumesh_extract_c_api_symbols content pattern output_variable)
  string(REGEX MATCHALL "${pattern}" manumesh_c_api_symbol_matches "${content}")
  set(manumesh_c_api_symbols)
  foreach(manumesh_c_api_symbol_match IN LISTS manumesh_c_api_symbol_matches)
    string(REGEX MATCH "manumesh_[A-Za-z0-9_]+"
      manumesh_c_api_symbol
      "${manumesh_c_api_symbol_match}"
    )
    list(APPEND manumesh_c_api_symbols "${manumesh_c_api_symbol}")
  endforeach()
  list(REMOVE_DUPLICATES manumesh_c_api_symbols)
  list(SORT manumesh_c_api_symbols)
  set(${output_variable} "${manumesh_c_api_symbols}" PARENT_SCOPE)
endfunction()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/api/CApi.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/support/c_api_abi_exports.inc"
)
file(READ "${PROJECT_SOURCE_DIR}/include/api/CApi.h" manumesh_c_api_header)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/support/c_api_abi_exports.inc"
  manumesh_c_api_test_export_list
)
manumesh_extract_c_api_symbols(
  "${manumesh_c_api_header}"
  "MANUMESH_API[^;]*manumesh_[A-Za-z0-9_]+[ \t\r\n]*\\("
  manumesh_c_api_header_symbols
)
manumesh_extract_c_api_symbols(
  "${manumesh_c_api_test_export_list}"
  "MANUMESH_C_API_SYMBOL\\([ \t\r\n]*manumesh_[A-Za-z0-9_]+"
  manumesh_c_api_test_symbols
)
if(NOT manumesh_c_api_header_symbols OR
   NOT manumesh_c_api_test_symbols OR
   NOT "${manumesh_c_api_header_symbols}" STREQUAL "${manumesh_c_api_test_symbols}")
  message(FATAL_ERROR
    "C API ABI consumer coverage is stale. Header symbols: "
    "${manumesh_c_api_header_symbols}; test symbols: ${manumesh_c_api_test_symbols}"
  )
endif()

add_executable(manumesh_c_api_abi_stress
  support/c_api_abi_stress.c
)
add_executable(manumesh_cpp_c_api_abi_stress
  support/cpp_c_api_abi_stress.cpp
)
set_source_files_properties(
  support/c_api_abi_stress.c
  support/cpp_c_api_abi_stress.cpp
  PROPERTIES COMPILE_OPTIONS "/Gv;/Zp1"
)
set_target_properties(manumesh_c_api_abi_stress PROPERTIES
  LINKER_LANGUAGE CXX
  FOLDER "testing/abi"
)
set_target_properties(manumesh_cpp_c_api_abi_stress PROPERTIES
  FOLDER "testing/abi"
)
foreach(manumesh_c_api_abi_target IN ITEMS
    manumesh_c_api_abi_stress
    manumesh_cpp_c_api_abi_stress)
  target_link_libraries(${manumesh_c_api_abi_target}
    PRIVATE ManuMesh::c_api
  )
  manumesh_copy_runtime_dependencies(${manumesh_c_api_abi_target})
  target_compile_options(${manumesh_c_api_abi_target} PRIVATE /WX)
  if(MANUMESH_BUILD_SHARED_LIBRARY)
    target_compile_definitions(${manumesh_c_api_abi_target}
      PRIVATE MANUMESH_EXPECT_DLL_IMPORT
    )
  else()
    target_compile_definitions(${manumesh_c_api_abi_target}
      PRIVATE MANUMESH_EXPECT_STATIC_API
    )
  endif()
  manumesh_apply_warnings(${manumesh_c_api_abi_target})
endforeach()
add_dependencies(
  manumesh_tests
  manumesh_c_api_abi_stress
  manumesh_cpp_c_api_abi_stress
)
add_test(NAME c_api_abi_stress COMMAND manumesh_c_api_abi_stress)
add_test(NAME cpp_c_api_abi_stress COMMAND manumesh_cpp_c_api_abi_stress)
set_tests_properties(c_api_abi_stress PROPERTIES LABELS "abi;unit" TIMEOUT 120)
set_tests_properties(cpp_c_api_abi_stress PROPERTIES LABELS "abi;unit" TIMEOUT 120)

add_library(manumesh_options_header_compile_check OBJECT
  support/feature_options_header_compile.cpp
  support/simplification_options_header_compile.cpp
)
target_compile_features(manumesh_options_header_compile_check PRIVATE cxx_std_14)
target_include_directories(manumesh_options_header_compile_check
  PRIVATE "${PROJECT_SOURCE_DIR}/include"
)
set_target_properties(manumesh_options_header_compile_check PROPERTIES
  FOLDER "architecture/include-boundaries"
)
manumesh_apply_warnings(manumesh_options_header_compile_check)

function(manumesh_add_module_link_check target module)
  # The root module objects are linked directly so the linker cannot skip an
  # object file and hide one of that module's undeclared dependencies.
  add_executable(${target}
    support/module_link_boundary_main.cpp
    $<TARGET_OBJECTS:manumesh_${module}_objects>
  )
  target_link_libraries(${target}
    PRIVATE manumesh_${module}_boundary_module
  )
  set_target_properties(${target} PROPERTIES
    FOLDER "architecture/module-boundaries"
  )
  manumesh_copy_runtime_dependencies(${target})
  manumesh_apply_warnings(${target})
  add_test(NAME ${module}_link_boundary COMMAND ${target})
  set_tests_properties(${module}_link_boundary PROPERTIES LABELS architecture)
endfunction()

manumesh_add_module_link_check(
  manumesh_feature_detection_link_check
  feature_detection
)
manumesh_add_module_link_check(
  manumesh_simplification_link_check
  simplification
)
manumesh_add_module_link_check(
  manumesh_io_link_check
  io
)

include(GoogleTest)
# PRE_TEST 发现：新的 TEST() 用例会在 CTest 运行时被发现，无需重新配置 CMake；
# 可执行文件也不会在 configure/build 阶段运行，适合 Visual Studio 多配置构建。
gtest_discover_tests(manumesh_tests
  DISCOVERY_MODE PRE_TEST
  DISCOVERY_TIMEOUT 120
  PROPERTIES LABELS unit
)
gtest_discover_tests(manumesh_external_tests
  DISCOVERY_MODE PRE_TEST
  DISCOVERY_TIMEOUT 120
  PROPERTIES
    LABELS external
    FIXTURES_REQUIRED thingi10k_manifest_ready
)

# Keep the downloaded large-mesh evidence tied to its source/license/hash
# manifest. The script is intentionally a separate CTest because the C++ test
# binary does not need a JSON dependency; an absent manifest is an optional
# fixture state and is reported as a passed no-op.
add_test(
  NAME thingi10k_manifest
  COMMAND "${CMAKE_COMMAND}"
    "-DMANUMESH_THINGI10K_DIR=${PROJECT_SOURCE_DIR}/output/thingi10k_large"
    "-DMANUMESH_THINGI10K_INDEX=${MANUMESH_THINGI10K_INDEX_PATH}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/support/check_thingi10k_manifest.cmake"
)
set_tests_properties(thingi10k_manifest PROPERTIES
  LABELS "external"
  TIMEOUT 180
  FIXTURES_SETUP thingi10k_manifest_ready
)

if(MANUMESH_ENABLE_ADDRESS_SANITIZER)
  # PRE_TEST discovers the individual tests when CTest runs. Append a CTest
  # script after both discovery includes so it can disable only the two wall-
  # clock guards whose thresholds intentionally target non-instrumented builds.
  set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/support/disable_asan_pipeline_perf_guards.cmake"
  )
endif()

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
  if(MANUMESH_REQUIRE_ARCHITECTURE_CHECKS)
    message(FATAL_ERROR
      "未找到 Python3，无法注册 include 边界架构检查；"
      "如需明确跳过，请设置 -DMANUMESH_REQUIRE_ARCHITECTURE_CHECKS=OFF。"
    )
  endif()
  message(STATUS
    "未找到 Python3；已跳过 include 边界 CTest。"
    "设置 -DMANUMESH_REQUIRE_ARCHITECTURE_CHECKS=ON 可将该检查设为必需。"
  )
endif()

set(MANUMESH_UNIT_TEST_DEPENDS
  manumesh_tests
  manumesh_ownership_lifetime_stress
  manumesh_c_api_abi_stress
  manumesh_cpp_c_api_abi_stress
  manumesh_options_header_compile_check
  manumesh_feature_detection_link_check
  manumesh_simplification_link_check
  manumesh_io_link_check
)
foreach(optional_target IN ITEMS
    manumesh
    manumesh_basic_simplify
    manumesh_feature_workflow_demo
    manumesh_c_api_basic
)
  if(TARGET ${optional_target})
    list(APPEND MANUMESH_UNIT_TEST_DEPENDS ${optional_target})
  endif()
endforeach()

add_custom_target(unit-tests
  COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_BINARY_DIR}"
    "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>" -LE "performance|external"
    -E "^manumesh_external_tests_NOT_BUILT$"
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
    performance/parallel_pipeline_benchmark.cpp
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
