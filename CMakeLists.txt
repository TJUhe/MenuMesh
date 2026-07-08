cmake_minimum_required(VERSION 3.18)

project(ManuMesh
  VERSION 0.1.0
  DESCRIPTION "Additive-manufacturing mesh geometry library"
  LANGUAGES C CXX
)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

set(MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  list(APPEND MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

option(MANUMESH_BUILD_SHARED_LIBRARY "Build manumesh as a shared library" ON)
option(MANUMESH_BUILD_CLI "Build the manumesh command line tool" ON)
option(MANUMESH_BUILD_EXAMPLES "Build example programs that consume the library" ON)
option(MANUMESH_BUILD_TESTS "Build GoogleTest based tests" ON)
option(MANUMESH_BUILD_PERFORMANCE_TESTS "Build large mesh performance tests" OFF)
option(MANUMESH_BUILD_DOCS "Enable the Doxygen docs-api target when Doxygen is available" ON)
option(MANUMESH_ENABLE_INSTALL "Enable SDK install rules" OFF)
option(MANUMESH_INSTALL_CMAKE_CONFIG "Install optional CMake config files for CMake consumers" OFF)

set(MANUMESH_EIGEN_PROVIDER "auto" CACHE STRING
  "Eigen provider: auto, system, vendored, or fetch"
)
set_property(CACHE MANUMESH_EIGEN_PROVIDER PROPERTY STRINGS auto system vendored fetch)
set(MANUMESH_EIGEN_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/eigen/include"
  CACHE PATH "Path to a vendored Eigen source/include tree"
)

set(MANUMESH_GOOGLETEST_PROVIDER "auto" CACHE STRING
  "GoogleTest provider for tests: auto, source, prebuilt, system, or fetch"
)
set_property(CACHE MANUMESH_GOOGLETEST_PROVIDER PROPERTY STRINGS
  auto source prebuilt system fetch
)
option(MANUMESH_MINGW_AUTO_GOOGLETEST_FROM_SOURCE
  "When MANUMESH_GOOGLETEST_PROVIDER=auto under MinGW, do not use prebuilt GoogleTest DLLs; build GoogleTest for the active compiler instead."
  ON
)
set(MANUMESH_GOOGLETEST_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/googletest/source/googletest"
  CACHE PATH "Path to a GoogleTest source tree with CMakeLists.txt"
)
set(MANUMESH_DEFAULT_GOOGLETEST_PREBUILT_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/googletest/prebuilt/mingw-x64-shared"
)
if(MSVC)
  set(MANUMESH_DEFAULT_GOOGLETEST_PREBUILT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/googletest/prebuilt/msvc-x64-static"
  )
endif()
set(MANUMESH_GOOGLETEST_PREBUILT_DIR
  "${MANUMESH_DEFAULT_GOOGLETEST_PREBUILT_DIR}"
  CACHE PATH "Root of a prebuilt GoogleTest bundle with include, lib, and bin folders"
)
set(MANUMESH_GTEST_INCLUDE_DIR "" CACHE PATH "Path to prebuilt GoogleTest headers")
set(MANUMESH_GTEST_LIBRARY "" CACHE FILEPATH "Path to prebuilt gtest library")
set(MANUMESH_GTEST_MAIN_LIBRARY "" CACHE FILEPATH "Path to prebuilt gtest_main library")
set(MANUMESH_GTEST_RUNTIME_DIR "" CACHE PATH
  "Optional path to prebuilt GoogleTest runtime DLL/shared library folder"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(NOT MANUMESH_EIGEN_PROVIDER MATCHES "^(auto|system|vendored|fetch)$")
  message(FATAL_ERROR
    "MANUMESH_EIGEN_PROVIDER must be one of: auto, system, vendored, fetch."
  )
endif()

if(MANUMESH_EIGEN_PROVIDER STREQUAL "auto" OR MANUMESH_EIGEN_PROVIDER STREQUAL "system")
  find_package(Eigen3 3.3 QUIET NO_MODULE)
endif()

if(NOT TARGET Eigen3::Eigen AND
   (MANUMESH_EIGEN_PROVIDER STREQUAL "auto" OR MANUMESH_EIGEN_PROVIDER STREQUAL "vendored"))
  if(EXISTS "${MANUMESH_EIGEN_SOURCE_DIR}/Eigen/Core")
    add_library(Eigen3::Eigen INTERFACE IMPORTED)
    target_include_directories(Eigen3::Eigen INTERFACE "${MANUMESH_EIGEN_SOURCE_DIR}")
  elseif(MANUMESH_EIGEN_PROVIDER STREQUAL "vendored")
    message(FATAL_ERROR
      "Vendored Eigen was not found at "
      "MANUMESH_EIGEN_SOURCE_DIR='${MANUMESH_EIGEN_SOURCE_DIR}'."
    )
  endif()
endif()

if(NOT TARGET Eigen3::Eigen AND
   (MANUMESH_EIGEN_PROVIDER STREQUAL "auto" OR MANUMESH_EIGEN_PROVIDER STREQUAL "fetch"))
  FetchContent_Declare(
    eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    ${MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS}
  )
  set(MANUMESH_SAVED_BUILD_TESTING "${BUILD_TESTING}")
  set(BUILD_TESTING OFF CACHE BOOL "Disable dependency tests while fetching Eigen" FORCE)
  FetchContent_MakeAvailable(eigen)
  set(BUILD_TESTING "${MANUMESH_SAVED_BUILD_TESTING}" CACHE BOOL "Enable project tests" FORCE)
endif()

set(MANUMESH_PUBLIC_HEADERS
  include/Export.h
  include/api/CApi.h
  include/algorithms/feature_detection/FeatureDetector.h
  include/algorithms/feature_detection/FeatureTypes.h
  include/algorithms/simplification/Metrics.h
  include/algorithms/simplification/PlainSimplifier.h
  include/algorithms/simplification/QEMSimplifier.h
  include/algorithms/simplification/SimplificationTypes.h
  include/core/Handles.h
  include/core/Mesh.h
  include/core/MeshGenerators.h
  include/core/MeshTopology.h
  include/core/PlainMesh.h
  include/core/Status.h
)

set(MANUMESH_LIBRARY_SOURCES
  src/api/CApi.cpp
  src/common/MeshQueries.cpp
  src/core/Mesh.cpp
  src/core/MeshGenerators.cpp
  src/core/MeshTopology.cpp
  src/core/PlainMesh.cpp
  src/core/Status.cpp
  src/feature_detection/FeatureCircularRecovery.cpp
  src/feature_detection/FeatureCycleRecovery.cpp
  src/feature_detection/FeatureDetector.cpp
  src/feature_detection/FeatureEvidence.cpp
  src/feature_detection/FeatureGraph.cpp
  src/feature_detection/FeatureLoopBuilder.cpp
  src/feature_detection/FeatureLoopRecovery.cpp
  src/feature_detection/FeaturePrimitiveRecovery.cpp
  src/feature_detection/FeatureTraceRecovery.cpp
  src/feature_detection/NormalTensor.cpp
  src/feature_detection/PrimitiveFit.cpp
  src/simplification/CandidateQueue.cpp
  src/simplification/CollapseAttempt.cpp
  src/simplification/CollapseLegality.cpp
  src/simplification/DynamicTopology.cpp
  src/simplification/FeatureConstraints.cpp
  src/simplification/GeometryPredicates.cpp
  src/simplification/Metrics.cpp
  src/simplification/QEMSimplifier.cpp
  src/simplification/Quadrics.cpp
  src/simplification/ResultBuilder.cpp
  src/simplification/SimplificationPolicies.cpp
  src/simplification/SimplificationRun.cpp
  src/simplification/SimplificationValidation.cpp
  src/simplification/SpatialFaceIndex.cpp
)

set(MANUMESH_COMMON_PRIVATE_HEADERS
  src/common/detail/MathConstants.h
  src/common/detail/MeshQueries.h
)

set(MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS
  src/feature_detection/detail/FeatureCircularRecovery.h
  src/feature_detection/detail/FeatureCycleRecovery.h
  src/feature_detection/detail/FeatureDetectionTypes.h
  src/feature_detection/detail/FeatureEvidence.h
  src/feature_detection/detail/FeatureGraph.h
  src/feature_detection/detail/FeatureLoopBuilder.h
  src/feature_detection/detail/FeatureLoopRecovery.h
  src/feature_detection/detail/FeaturePrimitiveRecovery.h
  src/feature_detection/detail/FeatureTraceRecovery.h
  src/feature_detection/detail/PrimitiveFit.h
)

set(MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS
  src/simplification/detail/CandidateQueue.h
  src/simplification/detail/CollapseAttempt.h
  src/simplification/detail/CollapseLegality.h
  src/simplification/detail/DynamicTopology.h
  src/simplification/detail/FeatureConstraints.h
  src/simplification/detail/GeometryPredicates.h
  src/simplification/detail/Quadrics.h
  src/simplification/detail/ResultBuilder.h
  src/simplification/detail/SimplificationPolicies.h
  src/simplification/detail/SimplificationRun.h
  src/simplification/detail/SimplificationTypes.h
  src/simplification/detail/SimplificationValidation.h
  src/simplification/detail/SpatialFaceIndex.h
)

set(MANUMESH_PRIVATE_HEADERS
  ${MANUMESH_COMMON_PRIVATE_HEADERS}
  ${MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS}
  ${MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS}
)

if(MANUMESH_BUILD_SHARED_LIBRARY)
  add_library(manumesh_core SHARED
    ${MANUMESH_LIBRARY_SOURCES}
    ${MANUMESH_PUBLIC_HEADERS}
    ${MANUMESH_PRIVATE_HEADERS}
  )
else()
  add_library(manumesh_core STATIC
    ${MANUMESH_LIBRARY_SOURCES}
    ${MANUMESH_PUBLIC_HEADERS}
    ${MANUMESH_PRIVATE_HEADERS}
  )
endif()
add_library(ManuMesh::manumesh ALIAS manumesh_core)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PREFIX "Public Headers"
  FILES ${MANUMESH_PUBLIC_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
  PREFIX "Source"
  FILES ${MANUMESH_LIBRARY_SOURCES}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/common/detail"
  PREFIX "Common Private Headers"
  FILES ${MANUMESH_COMMON_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/feature_detection/detail"
  PREFIX "Feature Detection Private Headers"
  FILES ${MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/simplification/detail"
  PREFIX "Simplification Private Headers"
  FILES ${MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS}
)

target_compile_features(manumesh_core PUBLIC cxx_std_17)
target_include_directories(manumesh_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/simplification"
)

if(TARGET Eigen3::Eigen)
  target_link_libraries(manumesh_core PUBLIC Eigen3::Eigen)
elseif(DEFINED eigen_SOURCE_DIR)
  target_include_directories(manumesh_core PUBLIC "${eigen_SOURCE_DIR}")
else()
  message(FATAL_ERROR "Eigen3 was not found and FetchContent did not provide eigen_SOURCE_DIR.")
endif()

if(EXISTS "${MANUMESH_EIGEN_SOURCE_DIR}/Eigen/Core")
  target_include_directories(manumesh_core
    PUBLIC
      "$<BUILD_INTERFACE:${MANUMESH_EIGEN_SOURCE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_DATADIR}/manumesh/thirdParty/eigen/include>"
  )
endif()

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

target_compile_definitions(manumesh_core
  PRIVATE MANUMESH_VERSION="${PROJECT_VERSION}"
)

if(MANUMESH_BUILD_SHARED_LIBRARY)
  target_compile_definitions(manumesh_core PRIVATE MANUMESH_BUILDING_DLL)
  if(WIN32)
    target_compile_definitions(manumesh_core INTERFACE MANUMESH_USING_DLL)
  endif()
endif()

set_target_properties(manumesh_core PROPERTIES
  OUTPUT_NAME manumesh
  EXPORT_NAME manumesh
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

function(manumesh_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /external:anglebrackets /external:W0>
      $<$<COMPILE_LANGUAGE:C>:/W4 /utf-8>
    )
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

manumesh_apply_warnings(manumesh_core)

function(manumesh_copy_mingw_runtime_dependencies target)
  if(WIN32 AND MINGW)
    if(NOT TARGET manumesh_mingw_runtime_dependencies)
      get_filename_component(MANUMESH_MINGW_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
      set(MANUMESH_MINGW_RUNTIME_FILES
        "${MANUMESH_MINGW_COMPILER_DIR}/libatomic-1.dll"
        "${MANUMESH_MINGW_COMPILER_DIR}/libgcc_s_dw2-1.dll"
        "${MANUMESH_MINGW_COMPILER_DIR}/libgcc_s_seh-1.dll"
        "${MANUMESH_MINGW_COMPILER_DIR}/libgcc_s_sjlj-1.dll"
        "${MANUMESH_MINGW_COMPILER_DIR}/libgomp-1.dll"
        "${MANUMESH_MINGW_COMPILER_DIR}/libquadmath-0.dll"
        "${MANUMESH_MINGW_COMPILER_DIR}/libssp-0.dll"
        "${MANUMESH_MINGW_COMPILER_DIR}/libstdc++-6.dll"
        "${MANUMESH_MINGW_COMPILER_DIR}/libwinpthread-1.dll"
      )
      file(GLOB MANUMESH_MINGW_GCC_RUNTIME_FILES CONFIGURE_DEPENDS
        "${MANUMESH_MINGW_COMPILER_DIR}/libgcc_s_*.dll"
      )
      list(APPEND MANUMESH_MINGW_RUNTIME_FILES ${MANUMESH_MINGW_GCC_RUNTIME_FILES})
      list(REMOVE_DUPLICATES MANUMESH_MINGW_RUNTIME_FILES)

      set(MANUMESH_MINGW_RUNTIME_COPY_COMMANDS
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:manumesh_core>"
      )
      foreach(mingw_runtime_file IN LISTS MANUMESH_MINGW_RUNTIME_FILES)
        if(EXISTS "${mingw_runtime_file}")
          list(APPEND MANUMESH_MINGW_RUNTIME_COPY_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${mingw_runtime_file}"
              "$<TARGET_FILE_DIR:manumesh_core>"
          )
        endif()
      endforeach()

      add_custom_target(manumesh_mingw_runtime_dependencies
        ${MANUMESH_MINGW_RUNTIME_COPY_COMMANDS}
        COMMENT "Copying MinGW runtime dependencies"
        VERBATIM
      )
    endif()

    add_dependencies(${target} manumesh_mingw_runtime_dependencies)
  endif()
endfunction()

function(manumesh_copy_runtime_dependencies target)
  if(WIN32 AND MANUMESH_BUILD_SHARED_LIBRARY)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:manumesh_core>"
        "$<TARGET_FILE_DIR:${target}>"
      COMMENT "Copying manumesh runtime library next to ${target}"
      VERBATIM
    )
  endif()
  manumesh_copy_mingw_runtime_dependencies(${target})
endfunction()

function(manumesh_copy_gtest_runtime_dependencies target)
  if(MANUMESH_RESOLVED_GTEST_RUNTIME_DIR AND EXISTS "${MANUMESH_RESOLVED_GTEST_RUNTIME_DIR}")
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

if(MANUMESH_BUILD_CLI)
  add_executable(manumesh
    apps/manumesh/CliArguments.cpp
    apps/manumesh/CliArguments.h
    apps/manumesh/CliCommands.h
    apps/manumesh/ManuMeshCommands.cpp
    apps/manumesh/main.cpp
    apps/manumesh/ManuMeshCli.cpp
    apps/manumesh/ManuMeshCli.h
  )
  target_link_libraries(manumesh PRIVATE ManuMesh::manumesh)
  manumesh_apply_warnings(manumesh)
  manumesh_copy_runtime_dependencies(manumesh)

  if(BUILD_TESTING)
    add_test(NAME manumesh_cli_help COMMAND manumesh --help)
    add_test(NAME manumesh_cli_rejects_unknown_option
      COMMAND manumesh simplify --not-a-real-option
    )
    add_test(NAME manumesh_cli_rejects_missing_option_value
      COMMAND manumesh generate --type
    )
    add_test(NAME manumesh_cli_rejects_malformed_number
      COMMAND manumesh generate --type plane --n 12x --out bad.stl
    )
    set_tests_properties(manumesh_cli_rejects_unknown_option
      manumesh_cli_rejects_missing_option_value
      manumesh_cli_rejects_malformed_number
      PROPERTIES WILL_FAIL TRUE
    )
  endif()
endif()

if(MANUMESH_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(BUILD_TESTING AND MANUMESH_BUILD_TESTS)
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
    if(NOT TARGET GTest::gtest_main AND
       MANUMESH_GOOGLETEST_PROVIDER STREQUAL "source")
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
    if(NOT TARGET GTest::gtest_main AND MANUMESH_GOOGLETEST_PROVIDER STREQUAL "prebuilt")
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

  add_subdirectory(tests)
endif()

find_program(CLANG_FORMAT_EXE NAMES clang-format)
file(GLOB_RECURSE MANUMESH_FORMAT_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/apps/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
)

if(CLANG_FORMAT_EXE)
  add_custom_target(format
    COMMAND "${CLANG_FORMAT_EXE}" -i ${MANUMESH_FORMAT_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Formatting C++ sources with clang-format"
    VERBATIM
  )
  add_custom_target(check-format
    COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror ${MANUMESH_FORMAT_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Checking C++ source formatting"
    VERBATIM
  )
else()
  add_custom_target(format
    COMMAND "${CMAKE_COMMAND}" -E echo "clang-format was not found; install it to enable the format target."
    VERBATIM
  )
  add_custom_target(check-format
    COMMAND "${CMAKE_COMMAND}" -E echo "clang-format was not found; install it to enable the check-format target."
    VERBATIM
  )
endif()

if(MANUMESH_BUILD_DOCS)
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    set(MANUMESH_DOXYGEN_INPUT_FILES
      "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/algorithm_essence.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/architecture.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/common_foundation.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/adding_new_algorithm.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/industrial_library.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/guide/sdk_integration.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/industrial_validation.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/large_model_validation.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/guide/vscode_setup.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/guide/vscode_mingw_ninja_parameter_debugging.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/guide/paper_guided_usage.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/feature_curve_constraints.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/feature_curve_experiment.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/external_model_validation.md"
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/manumesh/CliArguments.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/manumesh/CliCommands.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/manumesh/CliArguments.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/manumesh/ManuMeshCommands.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/manumesh/ManuMeshCli.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/manumesh/ManuMeshCli.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/apps/manumesh/main.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/basic_simplify.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/c_api_basic.c"
    )
    string(JOIN " " MANUMESH_DOXYGEN_INPUT ${MANUMESH_DOXYGEN_INPUT_FILES})
    set(MANUMESH_DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs/api")
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
      "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
      @ONLY
    )
    add_custom_target(docs-api
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${MANUMESH_DOXYGEN_OUTPUT_DIR}"
      COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM
    )
  else()
    add_custom_target(docs-api
      COMMAND "${CMAKE_COMMAND}" -E echo "Doxygen was not found; install it to enable API documentation generation."
      VERBATIM
    )
  endif()
endif()

if(MANUMESH_ENABLE_INSTALL)
  set(MANUMESH_LOCAL_SDK_PREFIX
    "${CMAKE_BINARY_DIR}/sdk"
    CACHE PATH "Local installed SDK root used by sdk-install-local and sdk-consumer-test"
  )
  set(MANUMESH_SDK_CONSUMER_BUILD_DIR
    "${CMAKE_BINARY_DIR}/sdk-consumer-build"
    CACHE PATH "Build directory for the installed-SDK consumer examples"
  )

  install(TARGETS manumesh_core
    EXPORT ManuMeshTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )

  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
  )

  if(EXISTS "${MANUMESH_EIGEN_SOURCE_DIR}/Eigen/Core")
    install(DIRECTORY "${MANUMESH_EIGEN_SOURCE_DIR}/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/manumesh/thirdParty/eigen/include"
      FILES_MATCHING PATTERN "*"
    )
  endif()

  if(MANUMESH_BUILD_CLI)
    install(TARGETS manumesh
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
  endif()

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/adm/templates/ManuMesh.props.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ManuMesh.props"
    @ONLY
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ManuMesh.props"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/manumesh/msvc"
  )

  if(MANUMESH_INSTALL_CMAKE_CONFIG)
    include(CMakePackageConfigHelpers)

    install(EXPORT ManuMeshTargets
      NAMESPACE ManuMesh::
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ManuMesh"
    )

    configure_package_config_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/adm/templates/ManuMeshConfig.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/ManuMeshConfig.cmake"
      INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ManuMesh"
    )
    write_basic_package_version_file(
      "${CMAKE_CURRENT_BINARY_DIR}/ManuMeshConfigVersion.cmake"
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY SameMajorVersion
    )

    install(FILES
      "${CMAKE_CURRENT_BINARY_DIR}/ManuMeshConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/ManuMeshConfigVersion.cmake"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ManuMesh"
    )
  endif()

  install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/architecture.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/industrial_library.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/guide/sdk_integration.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/large_model_validation.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
  )

  install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/basic_simplify.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/c_api_basic.c"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/manumesh/samples"
  )

  set(MANUMESH_SDK_INSTALL_DEPENDS manumesh_core)
  if(TARGET manumesh)
    list(APPEND MANUMESH_SDK_INSTALL_DEPENDS manumesh)
  endif()

  add_custom_target(sdk-install-local
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
      --config "$<CONFIG>"
      --prefix "${MANUMESH_LOCAL_SDK_PREFIX}"
    DEPENDS ${MANUMESH_SDK_INSTALL_DEPENDS}
    COMMENT "Installing local SDK layout to ${MANUMESH_LOCAL_SDK_PREFIX}"
    VERBATIM
  )

  set(MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS
    "-S" "${CMAKE_CURRENT_SOURCE_DIR}/examples/sdk_consumer"
    "-B" "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
    "-G" "${CMAKE_GENERATOR}"
    "-DMANUMESH_SDK_ROOT=${MANUMESH_LOCAL_SDK_PREFIX}"
    "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
  )
  if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS "-A" "${CMAKE_GENERATOR_PLATFORM}")
  endif()
  if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS "-T" "${CMAKE_GENERATOR_TOOLSET}")
  endif()
  if(NOT CMAKE_CONFIGURATION_TYPES)
    list(APPEND MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
  endif()

  add_custom_target(sdk-consumer-test
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
    COMMAND "${CMAKE_COMMAND}" ${MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS}
    COMMAND "${CMAKE_COMMAND}" --build "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
      --config "$<CONFIG>"
      --parallel
    COMMAND "${CMAKE_COMMAND}" -E chdir "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
      "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>"
      --output-on-failure
    DEPENDS sdk-install-local
    COMMENT "Building and running examples against the installed SDK"
    VERBATIM
  )

  if(BUILD_TESTING)
    add_test(NAME sdk_consumer_examples
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
        --config "$<CONFIG>"
        --target sdk-consumer-test
    )
  endif()
endif()
