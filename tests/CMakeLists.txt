include(FetchContent)

option(MANUMESH_BUILD_PERFORMANCE_TESTS "构建大型网格性能测试" OFF)
option(MANUMESH_REQUIRE_ARCHITECTURE_CHECKS
  "要求 Python 架构边界检查存在；启用时缺少 Python 会使配置失败"
  OFF
)

set(MANUMESH_GOOGLETEST_PROVIDER "auto" CACHE STRING
  "测试的 GoogleTest 提供方式：auto、source、system 或 fetch"
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
  "${PROJECT_SOURCE_DIR}/apps/CliOptionBinding.cpp"
  support/AnalyticFixtures.cpp
  support/TestSupport.cpp
  unit/apps/cli_option_binding_tests.cpp
  unit/common/geometry_predicates_tests.cpp
  unit/common/mesh_distance_index_tests.cpp
  unit/common/spatial_index_tests.cpp
  unit/core/core_tests.cpp
  unit/mesh_edit/mesh_edit_tests.cpp
  unit/api/c_api_diagnostics_tests.cpp
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
  manumesh_options_header_compile_check
  manumesh_feature_detection_link_check
  manumesh_simplification_link_check
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
