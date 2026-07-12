include(FetchContent)

option(MANUMESH_BUILD_PERFORMANCE_TESTS "Build large mesh performance tests" OFF)

set(MANUMESH_GOOGLETEST_PROVIDER "auto" CACHE STRING
  "GoogleTest provider for tests: auto, source, prebuilt, system, or fetch"
)
set_property(CACHE MANUMESH_GOOGLETEST_PROVIDER PROPERTY STRINGS
  auto
  source
  prebuilt
  system
  fetch
)
option(MANUMESH_MINGW_AUTO_GOOGLETEST_FROM_SOURCE
  "When MANUMESH_GOOGLETEST_PROVIDER=auto under MinGW, do not use prebuilt GoogleTest DLLs; build GoogleTest for the active compiler instead."
  ON
)
set(MANUMESH_GOOGLETEST_SOURCE_DIR
  "${PROJECT_SOURCE_DIR}/thirdParty/googletest/source/googletest"
  CACHE PATH "Path to a GoogleTest source tree with CMakeLists.txt"
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
  CACHE PATH "Root of a prebuilt GoogleTest bundle with include, lib, and bin folders"
)
set(MANUMESH_GTEST_INCLUDE_DIR "" CACHE PATH "Path to prebuilt GoogleTest headers")
set(MANUMESH_GTEST_LIBRARY "" CACHE FILEPATH "Path to prebuilt gtest library")
set(MANUMESH_GTEST_MAIN_LIBRARY "" CACHE FILEPATH
  "Path to prebuilt gtest_main library"
)
set(MANUMESH_GTEST_RUNTIME_DIR "" CACHE PATH
  "Optional path to prebuilt GoogleTest runtime DLL/shared library folder"
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
        COMMENT "Copying prebuilt GoogleTest runtime next to ${target}"
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
    "MANUMESH_GOOGLETEST_PROVIDER must be one of: auto, source, prebuilt, system, fetch."
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
      "GoogleTest source was not found at "
      "MANUMESH_GOOGLETEST_SOURCE_DIR='${MANUMESH_GOOGLETEST_SOURCE_DIR}'."
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
      "Prebuilt GoogleTest was not found. Expected a binary bundle with "
      "include/gtest/gtest.h, lib/gtest, lib/gtest_main, and optional bin "
      "runtime files at MANUMESH_GOOGLETEST_PREBUILT_DIR='${MANUMESH_GOOGLETEST_PREBUILT_DIR}', "
      "or set MANUMESH_GTEST_INCLUDE_DIR, MANUMESH_GTEST_LIBRARY, and MANUMESH_GTEST_MAIN_LIBRARY."
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
    "GoogleTest was not found. Use the prebuilt binary bundle under "
    "thirdParty/googletest/prebuilt, the vendored source tree under "
    "thirdParty/googletest/source, install a system GTest discoverable by "
    "find_package(GTest), or configure with -DMANUMESH_GOOGLETEST_PROVIDER=fetch. "
    "For MinGW, the default auto provider skips prebuilt GoogleTest DLLs "
    "and builds GoogleTest for the active compiler from vendored source; use "
    "-DMANUMESH_GOOGLETEST_PROVIDER=prebuilt only when that binary bundle matches "
    "the selected MinGW runtime."
  )
endif()

set(MANUMESH_UNIT_TEST_SOURCES
  support/TestSupport.cpp
  unit/common/geometry_predicates_tests.cpp
  unit/common/mesh_distance_index_tests.cpp
  unit/common/spatial_index_tests.cpp
  unit/mesh_edit/mesh_edit_tests.cpp
  unit/api/c_api_diagnostics_tests.cpp
  unit/api/c_api_mesh_tests.cpp
  unit/api/c_api_simplify_abi_tests.cpp
  unit/feature_detection/FeatureDetectionTestSupport.cpp
  unit/feature_detection/feature_detection_api_tests.cpp
  unit/feature_detection/feature_detection_discrete_tests.cpp
  unit/feature_detection/feature_detection_external_tests.cpp
  unit/feature_detection/feature_detection_fixture_tests.cpp
  unit/feature_detection/feature_detection_loop_tests.cpp
  unit/feature_detection/feature_detection_smooth_curvature_tests.cpp
  unit/simplification/qem_parameter_budget_tests.cpp
  unit/simplification/qem_parameter_feature_tests.cpp
  unit/simplification/qem_parameter_industrial_tests.cpp
  unit/simplification/simplification_api_tests.cpp
  unit/simplification/simplification_core_tests.cpp
  unit/simplification/simplification_external_tests.cpp
  unit/simplification/simplification_feature_protection_tests.cpp
  unit/simplification/simplification_legality_tests.cpp
  unit/simplification/simplification_normal_tensor_tests.cpp
  unit/simplification/simplification_quality_refinement_tests.cpp
  unit/simplification/simplification_texture_tests.cpp
)

add_executable(manumesh_tests ${MANUMESH_UNIT_TEST_SOURCES})
if(MANUMESH_BUILD_SHARED_LIBRARY)
  target_sources(manumesh_tests
    PRIVATE
      "${PROJECT_SOURCE_DIR}/src/simplification/TextureProtection.cpp"
  )
endif()
if(WIN32 AND MANUMESH_BUILD_SHARED_LIBRARY)
  target_sources(manumesh_tests
    PRIVATE
      $<TARGET_OBJECTS:manumesh_common_objects>
      $<TARGET_OBJECTS:manumesh_mesh_edit_objects>
  )
endif()
target_include_directories(manumesh_tests
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/simplification"
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
  message(STATUS "Python3 not found; include boundary CTest disabled.")
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
