cmake_minimum_required(VERSION 3.18)

project(line_quadrics_qem
  VERSION 0.1.0
  DESCRIPTION "Line-quadrics guided QEM mesh simplification library"
  LANGUAGES C CXX
)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

set(LQ_FETCHCONTENT_TIMESTAMP_ARGS)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  list(APPEND LQ_FETCHCONTENT_TIMESTAMP_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

option(LQ_BUILD_SHARED_LIBRARY "Build line_quadrics_qem as a shared library" ON)
option(LQ_BUILD_CLI "Build the linequadrics command line tool" ON)
option(LQ_BUILD_EXAMPLES "Build example programs that consume the library" ON)
option(LQ_BUILD_TESTS "Build GoogleTest based tests" ON)
option(LQ_BUILD_PERFORMANCE_TESTS "Build large mesh performance tests" OFF)
option(LQ_BUILD_DOCS "Enable the Doxygen docs-api target when Doxygen is available" ON)
option(LQ_ENABLE_INSTALL "Enable install rules and CMake package export" ON)
set(LQ_GOOGLETEST_PROVIDER "auto" CACHE STRING
  "GoogleTest provider for tests: auto, package, or source"
)
set_property(CACHE LQ_GOOGLETEST_PROVIDER PROPERTY STRINGS auto package source)
set(LQ_GOOGLETEST_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/googletest"
  CACHE PATH "Path to a GoogleTest source tree when LQ_GOOGLETEST_PROVIDER is source or auto fallback"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Eigen3 3.3 QUIET NO_MODULE)

if(NOT Eigen3_FOUND)
  FetchContent_Declare(
    eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    ${LQ_FETCHCONTENT_TIMESTAMP_ARGS}
  )
  FetchContent_MakeAvailable(eigen)
endif()

set(LQ_PUBLIC_HEADERS
  include/line_quadrics_qem/CApi.h
  include/line_quadrics_qem/Export.h
  include/line_quadrics_qem/FeatureDetection.h
  include/line_quadrics_qem/Handles.h
  include/line_quadrics_qem/Mesh.h
  include/line_quadrics_qem/MeshGenerators.h
  include/line_quadrics_qem/MeshTopology.h
  include/line_quadrics_qem/Metrics.h
  include/line_quadrics_qem/QEMSimplifier.h
  include/line_quadrics_qem/Status.h
  include/line_quadrics_qem/api/CApi.h
  include/line_quadrics_qem/core/Handles.h
  include/line_quadrics_qem/core/Mesh.h
  include/line_quadrics_qem/core/MeshGenerators.h
  include/line_quadrics_qem/core/MeshTopology.h
  include/line_quadrics_qem/core/Status.h
  include/line_quadrics_qem/features/FeatureDetection.h
  include/line_quadrics_qem/simplification/Metrics.h
  include/line_quadrics_qem/simplification/QEMSimplifier.h
)

set(LQ_LIBRARY_SOURCES
  src/api/CApi.cpp
  src/core/Mesh.cpp
  src/core/MeshGenerators.cpp
  src/core/MeshTopology.cpp
  src/features/FeatureDetection.cpp
  src/simplification/Metrics.cpp
  src/simplification/QEMSimplifier.cpp
)

if(LQ_BUILD_SHARED_LIBRARY)
  add_library(line_quadrics_qem SHARED ${LQ_LIBRARY_SOURCES} ${LQ_PUBLIC_HEADERS})
else()
  add_library(line_quadrics_qem STATIC ${LQ_LIBRARY_SOURCES} ${LQ_PUBLIC_HEADERS})
endif()
add_library(line_quadrics_qem::line_quadrics_qem ALIAS line_quadrics_qem)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PREFIX "Public Headers"
  FILES ${LQ_PUBLIC_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
  PREFIX "Source"
  FILES ${LQ_LIBRARY_SOURCES}
)

target_compile_features(line_quadrics_qem PUBLIC cxx_std_17)
target_include_directories(line_quadrics_qem
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(TARGET Eigen3::Eigen)
  target_link_libraries(line_quadrics_qem PUBLIC Eigen3::Eigen)
elseif(DEFINED eigen_SOURCE_DIR)
  target_include_directories(line_quadrics_qem PUBLIC "${eigen_SOURCE_DIR}")
else()
  message(FATAL_ERROR "Eigen3 was not found and FetchContent did not provide eigen_SOURCE_DIR.")
endif()

target_compile_definitions(line_quadrics_qem
  PRIVATE LINE_QUADRICS_QEM_VERSION="${PROJECT_VERSION}"
)

if(LQ_BUILD_SHARED_LIBRARY)
  target_compile_definitions(line_quadrics_qem PRIVATE LINE_QUADRICS_QEM_BUILDING_DLL)
  if(WIN32)
    target_compile_definitions(line_quadrics_qem INTERFACE LINE_QUADRICS_QEM_USING_DLL)
  endif()
endif()

set_target_properties(line_quadrics_qem PROPERTIES
  OUTPUT_NAME line_quadrics_qem
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

function(lq_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /external:anglebrackets /external:W0>
      $<$<COMPILE_LANGUAGE:C>:/W4 /utf-8>
    )
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

lq_apply_warnings(line_quadrics_qem)

function(lq_copy_runtime_dependencies target)
  if(WIN32 AND LQ_BUILD_SHARED_LIBRARY)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:line_quadrics_qem>"
        "$<TARGET_FILE_DIR:${target}>"
      COMMENT "Copying line_quadrics_qem runtime library next to ${target}"
      VERBATIM
    )
  endif()
endfunction()

if(LQ_BUILD_CLI)
  add_executable(linequadrics apps/linequadrics/main.cpp)
  target_link_libraries(linequadrics PRIVATE line_quadrics_qem::line_quadrics_qem)
  lq_apply_warnings(linequadrics)
  lq_copy_runtime_dependencies(linequadrics)

  if(BUILD_TESTING)
    add_test(NAME linequadrics_cli_help COMMAND linequadrics --help)
  endif()
endif()

if(LQ_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(BUILD_TESTING AND LQ_BUILD_TESTS)
  if(NOT LQ_GOOGLETEST_PROVIDER MATCHES "^(auto|package|source)$")
    message(FATAL_ERROR
      "LQ_GOOGLETEST_PROVIDER must be one of: auto, package, source."
    )
  endif()

  if(LQ_GOOGLETEST_PROVIDER STREQUAL "auto" OR
     LQ_GOOGLETEST_PROVIDER STREQUAL "package")
    find_package(GTest QUIET)
    if(TARGET GTest::Main AND NOT TARGET GTest::gtest_main)
      add_library(GTest::gtest_main INTERFACE IMPORTED)
      target_link_libraries(GTest::gtest_main INTERFACE GTest::Main)
    endif()
  endif()

  if(NOT TARGET GTest::gtest_main AND
     (LQ_GOOGLETEST_PROVIDER STREQUAL "auto" OR
      LQ_GOOGLETEST_PROVIDER STREQUAL "source"))
    if(EXISTS "${LQ_GOOGLETEST_SOURCE_DIR}/CMakeLists.txt")
      set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
      set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
      add_subdirectory("${LQ_GOOGLETEST_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/thirdParty/googletest"
        EXCLUDE_FROM_ALL
      )
    elseif(LQ_GOOGLETEST_PROVIDER STREQUAL "source")
      message(FATAL_ERROR
        "GoogleTest source tree was not found at "
        "LQ_GOOGLETEST_SOURCE_DIR='${LQ_GOOGLETEST_SOURCE_DIR}'."
      )
    endif()
  endif()

  if(NOT TARGET GTest::gtest_main)
    message(FATAL_ERROR
      "GoogleTest was not found. Install a package discoverable by "
      "find_package(GTest), or configure with "
      "-DLQ_GOOGLETEST_PROVIDER=source "
      "-DLQ_GOOGLETEST_SOURCE_DIR=/path/to/googletest."
    )
  endif()

  add_subdirectory(tests)
endif()

find_program(CLANG_FORMAT_EXE NAMES clang-format)
file(GLOB_RECURSE LQ_FORMAT_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/apps/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

if(CLANG_FORMAT_EXE)
  add_custom_target(format
    COMMAND "${CLANG_FORMAT_EXE}" -i ${LQ_FORMAT_FILES}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Formatting C++ sources with clang-format"
    VERBATIM
  )
  add_custom_target(check-format
    COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror ${LQ_FORMAT_FILES}
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

if(LQ_BUILD_DOCS)
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    set(LQ_DOXYGEN_INPUT
      "${CMAKE_CURRENT_SOURCE_DIR}/README.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/design/architecture.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/design/industrial_library.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/guide/sdk_integration.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/design/industrial_validation.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/design/large_model_validation.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/guide/vscode_setup.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/guide/paper_guided_usage.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/design/feature_curve_constraints.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/design/feature_curve_experiment.md ${CMAKE_CURRENT_SOURCE_DIR}/docs/design/external_model_validation.md ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/apps/linequadrics/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/basic_simplify.cpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/c_api_basic.c"
    )
    set(LQ_DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs/api")
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
      "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
      @ONLY
    )
    add_custom_target(docs-api
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${LQ_DOXYGEN_OUTPUT_DIR}"
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

if(LQ_ENABLE_INSTALL)
  include(CMakePackageConfigHelpers)

  install(TARGETS line_quadrics_qem
    EXPORT line_quadrics_qemTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )

  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
  )

  if(LQ_BUILD_CLI)
    install(TARGETS linequadrics
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
  endif()

  install(EXPORT line_quadrics_qemTargets
    NAMESPACE line_quadrics_qem::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/line_quadrics_qem"
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/line_quadrics_qemConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/line_quadrics_qemConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/line_quadrics_qem"
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/line_quadrics_qem.props.in"
    "${CMAKE_CURRENT_BINARY_DIR}/line_quadrics_qem.props"
    @ONLY
  )
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/line_quadrics_qemConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/line_quadrics_qemConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/line_quadrics_qemConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/line_quadrics_qem"
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/line_quadrics_qem.props"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/line_quadrics_qem/msvc"
  )

  install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/architecture.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/industrial_library.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/guide/sdk_integration.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/design/large_model_validation.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
  )
endif()
