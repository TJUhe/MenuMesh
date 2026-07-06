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
option(LQ_ENABLE_INSTALL "Enable SDK install rules" OFF)
option(LQ_INSTALL_CMAKE_CONFIG "Install optional CMake config files for CMake consumers" OFF)

set(LQ_EIGEN_PROVIDER "auto" CACHE STRING
  "Eigen provider: auto, system, vendored, or fetch"
)
set_property(CACHE LQ_EIGEN_PROVIDER PROPERTY STRINGS auto system vendored fetch)
set(LQ_EIGEN_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/eigen/include"
  CACHE PATH "Path to a vendored Eigen source/include tree"
)

set(LQ_GOOGLETEST_PROVIDER "auto" CACHE STRING
  "GoogleTest provider for tests: auto, prebuilt, system, or fetch"
)
set_property(CACHE LQ_GOOGLETEST_PROVIDER PROPERTY STRINGS auto prebuilt system fetch)
set(LQ_DEFAULT_GOOGLETEST_PREBUILT_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/googletest/prebuilt/mingw-x64-shared"
)
if(MSVC)
  set(LQ_DEFAULT_GOOGLETEST_PREBUILT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/googletest/prebuilt/msvc-x64-static"
  )
endif()
set(LQ_GOOGLETEST_PREBUILT_DIR
  "${LQ_DEFAULT_GOOGLETEST_PREBUILT_DIR}"
  CACHE PATH "Root of a prebuilt GoogleTest bundle with include, lib, and bin folders"
)
set(LQ_GTEST_INCLUDE_DIR "" CACHE PATH "Path to prebuilt GoogleTest headers")
set(LQ_GTEST_LIBRARY "" CACHE FILEPATH "Path to prebuilt gtest library")
set(LQ_GTEST_MAIN_LIBRARY "" CACHE FILEPATH "Path to prebuilt gtest_main library")
set(LQ_GTEST_RUNTIME_DIR "" CACHE PATH
  "Optional path to prebuilt GoogleTest runtime DLL/shared library folder"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(NOT LQ_EIGEN_PROVIDER MATCHES "^(auto|system|vendored|fetch)$")
  message(FATAL_ERROR
    "LQ_EIGEN_PROVIDER must be one of: auto, system, vendored, fetch."
  )
endif()

if(LQ_EIGEN_PROVIDER STREQUAL "auto" OR LQ_EIGEN_PROVIDER STREQUAL "system")
  find_package(Eigen3 3.3 QUIET NO_MODULE)
endif()

if(NOT TARGET Eigen3::Eigen AND
   (LQ_EIGEN_PROVIDER STREQUAL "auto" OR LQ_EIGEN_PROVIDER STREQUAL "vendored"))
  if(EXISTS "${LQ_EIGEN_SOURCE_DIR}/Eigen/Core")
    add_library(Eigen3::Eigen INTERFACE IMPORTED)
    target_include_directories(Eigen3::Eigen INTERFACE "${LQ_EIGEN_SOURCE_DIR}")
  elseif(LQ_EIGEN_PROVIDER STREQUAL "vendored")
    message(FATAL_ERROR
      "Vendored Eigen was not found at "
      "LQ_EIGEN_SOURCE_DIR='${LQ_EIGEN_SOURCE_DIR}'."
    )
  endif()
endif()

if(NOT TARGET Eigen3::Eigen AND
   (LQ_EIGEN_PROVIDER STREQUAL "auto" OR LQ_EIGEN_PROVIDER STREQUAL "fetch"))
  FetchContent_Declare(
    eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    ${LQ_FETCHCONTENT_TIMESTAMP_ARGS}
  )
  set(LQ_SAVED_BUILD_TESTING "${BUILD_TESTING}")
  set(BUILD_TESTING OFF CACHE BOOL "Disable dependency tests while fetching Eigen" FORCE)
  FetchContent_MakeAvailable(eigen)
  set(BUILD_TESTING "${LQ_SAVED_BUILD_TESTING}" CACHE BOOL "Enable project tests" FORCE)
endif()

set(LQ_PUBLIC_HEADERS
  include/line_quadrics_qem/Export.h
  include/line_quadrics_qem/api/CApi.h
  include/line_quadrics_qem/algorithms/simplification/Metrics.h
  include/line_quadrics_qem/algorithms/simplification/QEMSimplifier.h
  include/line_quadrics_qem/core/Handles.h
  include/line_quadrics_qem/core/Mesh.h
  include/line_quadrics_qem/core/MeshGenerators.h
  include/line_quadrics_qem/core/MeshTopology.h
  include/line_quadrics_qem/core/PlainMesh.h
  include/line_quadrics_qem/core/Status.h
  include/line_quadrics_qem/features/FeatureDetection.h
)

set(LQ_LIBRARY_SOURCES
  src/api/CApi.cpp
  src/core/Mesh.cpp
  src/core/MeshGenerators.cpp
  src/core/MeshTopology.cpp
  src/core/PlainMesh.cpp
  src/core/Status.cpp
  src/features/FeatureDetection.cpp
  src/simplification/CandidateQueue.cpp
  src/simplification/CollapseLegality.cpp
  src/simplification/DynamicTopology.cpp
  src/simplification/FeatureConstraints.cpp
  src/simplification/GeometryPredicates.cpp
  src/simplification/MeshEdges.cpp
  src/simplification/Metrics.cpp
  src/simplification/QEMSimplifier.cpp
  src/simplification/Quadrics.cpp
  src/simplification/ResultBuilder.cpp
  src/simplification/SimplificationRun.cpp
  src/simplification/SimplificationValidation.cpp
  src/simplification/SpatialFaceIndex.cpp
)

set(LQ_PRIVATE_HEADERS
  src/simplification/detail/CandidateQueue.h
  src/simplification/detail/CollapseLegality.h
  src/simplification/detail/DynamicTopology.h
  src/simplification/detail/FeatureConstraints.h
  src/simplification/detail/GeometryPredicates.h
  src/simplification/detail/MeshEdges.h
  src/simplification/detail/Quadrics.h
  src/simplification/detail/ResultBuilder.h
  src/simplification/detail/SimplificationConstants.h
  src/simplification/detail/SimplificationRun.h
  src/simplification/detail/SimplificationTypes.h
  src/simplification/detail/SimplificationValidation.h
  src/simplification/detail/SpatialFaceIndex.h
)

if(LQ_BUILD_SHARED_LIBRARY)
  add_library(line_quadrics_qem SHARED
    ${LQ_LIBRARY_SOURCES}
    ${LQ_PUBLIC_HEADERS}
    ${LQ_PRIVATE_HEADERS}
  )
else()
  add_library(line_quadrics_qem STATIC
    ${LQ_LIBRARY_SOURCES}
    ${LQ_PUBLIC_HEADERS}
    ${LQ_PRIVATE_HEADERS}
  )
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
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/simplification/detail"
  PREFIX "Simplification Private Headers"
  FILES ${LQ_PRIVATE_HEADERS}
)

target_compile_features(line_quadrics_qem PUBLIC cxx_std_17)
target_include_directories(line_quadrics_qem
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/simplification"
)

if(TARGET Eigen3::Eigen)
  target_link_libraries(line_quadrics_qem PRIVATE Eigen3::Eigen)
  get_target_property(LQ_RESOLVED_EIGEN_INCLUDE_DIRS
    Eigen3::Eigen INTERFACE_INCLUDE_DIRECTORIES
  )
  if(LQ_RESOLVED_EIGEN_INCLUDE_DIRS)
    foreach(eigen_include_dir IN LISTS LQ_RESOLVED_EIGEN_INCLUDE_DIRS)
      target_include_directories(line_quadrics_qem
        PUBLIC "$<BUILD_INTERFACE:${eigen_include_dir}>"
      )
    endforeach()
  endif()
elseif(DEFINED eigen_SOURCE_DIR)
  target_include_directories(line_quadrics_qem PUBLIC "${eigen_SOURCE_DIR}")
else()
  message(FATAL_ERROR "Eigen3 was not found and FetchContent did not provide eigen_SOURCE_DIR.")
endif()

if(EXISTS "${LQ_EIGEN_SOURCE_DIR}/Eigen/Core")
  target_include_directories(line_quadrics_qem
    PUBLIC
      "$<BUILD_INTERFACE:${LQ_EIGEN_SOURCE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_DATADIR}/line_quadrics_qem/thirdParty/eigen/include>"
  )
endif()

function(lq_import_prebuilt_gtest bundle_root)
  if(TARGET GTest::gtest_main)
    return()
  endif()

  set(gtest_include "${LQ_GTEST_INCLUDE_DIR}")
  set(gtest_library "${LQ_GTEST_LIBRARY}")
  set(gtest_main_library "${LQ_GTEST_MAIN_LIBRARY}")
  set(gtest_runtime_dir "${LQ_GTEST_RUNTIME_DIR}")
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
  set(LQ_RESOLVED_GTEST_RUNTIME_DIR "${gtest_runtime_dir}" PARENT_SCOPE)
endfunction()

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

function(lq_copy_mingw_runtime_dependencies target)
  if(WIN32 AND MINGW)
    if(NOT TARGET lq_mingw_runtime_dependencies)
      get_filename_component(LQ_MINGW_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
      set(LQ_MINGW_RUNTIME_FILES
        "${LQ_MINGW_COMPILER_DIR}/libatomic-1.dll"
        "${LQ_MINGW_COMPILER_DIR}/libgcc_s_dw2-1.dll"
        "${LQ_MINGW_COMPILER_DIR}/libgcc_s_seh-1.dll"
        "${LQ_MINGW_COMPILER_DIR}/libgcc_s_sjlj-1.dll"
        "${LQ_MINGW_COMPILER_DIR}/libgomp-1.dll"
        "${LQ_MINGW_COMPILER_DIR}/libquadmath-0.dll"
        "${LQ_MINGW_COMPILER_DIR}/libssp-0.dll"
        "${LQ_MINGW_COMPILER_DIR}/libstdc++-6.dll"
        "${LQ_MINGW_COMPILER_DIR}/libwinpthread-1.dll"
      )
      file(GLOB LQ_MINGW_GCC_RUNTIME_FILES CONFIGURE_DEPENDS
        "${LQ_MINGW_COMPILER_DIR}/libgcc_s_*.dll"
      )
      list(APPEND LQ_MINGW_RUNTIME_FILES ${LQ_MINGW_GCC_RUNTIME_FILES})
      list(REMOVE_DUPLICATES LQ_MINGW_RUNTIME_FILES)

      set(LQ_MINGW_RUNTIME_COPY_COMMANDS
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:line_quadrics_qem>"
      )
      foreach(mingw_runtime_file IN LISTS LQ_MINGW_RUNTIME_FILES)
        if(EXISTS "${mingw_runtime_file}")
          list(APPEND LQ_MINGW_RUNTIME_COPY_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${mingw_runtime_file}"
              "$<TARGET_FILE_DIR:line_quadrics_qem>"
          )
        endif()
      endforeach()

      add_custom_target(lq_mingw_runtime_dependencies
        ${LQ_MINGW_RUNTIME_COPY_COMMANDS}
        COMMENT "Copying MinGW runtime dependencies"
        VERBATIM
      )
    endif()

    add_dependencies(${target} lq_mingw_runtime_dependencies)
  endif()
endfunction()

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
  lq_copy_mingw_runtime_dependencies(${target})
endfunction()

function(lq_copy_gtest_runtime_dependencies target)
  if(LQ_RESOLVED_GTEST_RUNTIME_DIR AND EXISTS "${LQ_RESOLVED_GTEST_RUNTIME_DIR}")
    file(GLOB LQ_GTEST_RUNTIME_FILES CONFIGURE_DEPENDS
      "${LQ_RESOLVED_GTEST_RUNTIME_DIR}/*.dll"
      "${LQ_RESOLVED_GTEST_RUNTIME_DIR}/*.so"
      "${LQ_RESOLVED_GTEST_RUNTIME_DIR}/*.dylib"
    )
    foreach(gtest_runtime_file IN LISTS LQ_GTEST_RUNTIME_FILES)
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

if(LQ_BUILD_CLI)
  add_executable(linequadrics apps/linequadrics/main.cpp)
  target_link_libraries(linequadrics PRIVATE line_quadrics_qem::line_quadrics_qem)
  lq_apply_warnings(linequadrics)
  lq_copy_runtime_dependencies(linequadrics)

  if(BUILD_TESTING)
    add_test(NAME linequadrics_cli_help COMMAND linequadrics --help)
    add_test(NAME linequadrics_cli_rejects_unknown_option
      COMMAND linequadrics simplify --not-a-real-option
    )
    add_test(NAME linequadrics_cli_rejects_missing_option_value
      COMMAND linequadrics generate --type
    )
    add_test(NAME linequadrics_cli_rejects_malformed_number
      COMMAND linequadrics generate --type plane --n 12x --out bad.stl
    )
    set_tests_properties(linequadrics_cli_rejects_unknown_option
      linequadrics_cli_rejects_missing_option_value
      linequadrics_cli_rejects_malformed_number
      PROPERTIES WILL_FAIL TRUE
    )
  endif()
endif()

if(LQ_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(BUILD_TESTING AND LQ_BUILD_TESTS)
  if(NOT LQ_GOOGLETEST_PROVIDER MATCHES "^(auto|prebuilt|system|fetch)$")
    message(FATAL_ERROR
      "LQ_GOOGLETEST_PROVIDER must be one of: auto, prebuilt, system, fetch."
    )
  endif()

  if(LQ_GOOGLETEST_PROVIDER STREQUAL "auto" OR
     LQ_GOOGLETEST_PROVIDER STREQUAL "prebuilt")
    lq_import_prebuilt_gtest("${LQ_GOOGLETEST_PREBUILT_DIR}")
    if(NOT TARGET GTest::gtest_main AND LQ_GOOGLETEST_PROVIDER STREQUAL "prebuilt")
      message(FATAL_ERROR
        "Prebuilt GoogleTest was not found. Expected a binary bundle with "
        "include/gtest/gtest.h, lib/gtest, lib/gtest_main, and optional bin "
        "runtime files at LQ_GOOGLETEST_PREBUILT_DIR='${LQ_GOOGLETEST_PREBUILT_DIR}', "
        "or set LQ_GTEST_INCLUDE_DIR, LQ_GTEST_LIBRARY, and LQ_GTEST_MAIN_LIBRARY."
      )
    endif()
  endif()

  if(LQ_GOOGLETEST_PROVIDER STREQUAL "auto" OR
     LQ_GOOGLETEST_PROVIDER STREQUAL "system")
    if(NOT TARGET GTest::gtest_main)
      find_package(GTest QUIET)
      if(TARGET GTest::Main AND NOT TARGET GTest::gtest_main)
        add_library(GTest::gtest_main INTERFACE IMPORTED)
        target_link_libraries(GTest::gtest_main INTERFACE GTest::Main)
      endif()
    endif()
  endif()

  if(NOT TARGET GTest::gtest_main AND
     (LQ_GOOGLETEST_PROVIDER STREQUAL "auto" OR
      LQ_GOOGLETEST_PROVIDER STREQUAL "fetch"))
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
      ${LQ_FETCHCONTENT_TIMESTAMP_ARGS}
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()

  if(NOT TARGET GTest::gtest_main)
    message(FATAL_ERROR
      "GoogleTest was not found. Use the prebuilt binary bundle under "
      "thirdParty/googletest/prebuilt, install a system GTest discoverable by "
      "find_package(GTest), or configure with -DLQ_GOOGLETEST_PROVIDER=fetch."
    )
  endif()

  add_subdirectory(tests)
endif()

find_program(CLANG_FORMAT_EXE NAMES clang-format)
file(GLOB_RECURSE LQ_FORMAT_FILES CONFIGURE_DEPENDS
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
  set(LQ_LOCAL_SDK_PREFIX
    "${CMAKE_BINARY_DIR}/sdk"
    CACHE PATH "Local installed SDK root used by sdk-install-local and sdk-consumer-test"
  )
  set(LQ_SDK_CONSUMER_BUILD_DIR
    "${CMAKE_BINARY_DIR}/sdk-consumer-build"
    CACHE PATH "Build directory for the installed-SDK consumer examples"
  )

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

  if(EXISTS "${LQ_EIGEN_SOURCE_DIR}/Eigen/Core")
    install(DIRECTORY "${LQ_EIGEN_SOURCE_DIR}/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/line_quadrics_qem/thirdParty/eigen/include"
      FILES_MATCHING PATTERN "*"
    )
  endif()

  if(LQ_BUILD_CLI)
    install(TARGETS linequadrics
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
  endif()

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/adm/templates/line_quadrics_qem.props.in"
    "${CMAKE_CURRENT_BINARY_DIR}/line_quadrics_qem.props"
    @ONLY
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/line_quadrics_qem.props"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/line_quadrics_qem/msvc"
  )

  if(LQ_INSTALL_CMAKE_CONFIG)
    include(CMakePackageConfigHelpers)

    install(EXPORT line_quadrics_qemTargets
      NAMESPACE line_quadrics_qem::
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/line_quadrics_qem"
    )

    configure_package_config_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/adm/templates/line_quadrics_qemConfig.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/line_quadrics_qemConfig.cmake"
      INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/line_quadrics_qem"
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
    DESTINATION "${CMAKE_INSTALL_DATADIR}/line_quadrics_qem/samples"
  )

  set(LQ_SDK_INSTALL_DEPENDS line_quadrics_qem)
  if(TARGET linequadrics)
    list(APPEND LQ_SDK_INSTALL_DEPENDS linequadrics)
  endif()

  add_custom_target(sdk-install-local
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
      --config "$<CONFIG>"
      --prefix "${LQ_LOCAL_SDK_PREFIX}"
    DEPENDS ${LQ_SDK_INSTALL_DEPENDS}
    COMMENT "Installing local SDK layout to ${LQ_LOCAL_SDK_PREFIX}"
    VERBATIM
  )

  set(LQ_SDK_CONSUMER_CONFIGURE_ARGS
    "-S" "${CMAKE_CURRENT_SOURCE_DIR}/examples/sdk_consumer"
    "-B" "${LQ_SDK_CONSUMER_BUILD_DIR}"
    "-G" "${CMAKE_GENERATOR}"
    "-DLQ_SDK_ROOT=${LQ_LOCAL_SDK_PREFIX}"
    "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
  )
  if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND LQ_SDK_CONSUMER_CONFIGURE_ARGS "-A" "${CMAKE_GENERATOR_PLATFORM}")
  endif()
  if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND LQ_SDK_CONSUMER_CONFIGURE_ARGS "-T" "${CMAKE_GENERATOR_TOOLSET}")
  endif()
  if(NOT CMAKE_CONFIGURATION_TYPES)
    list(APPEND LQ_SDK_CONSUMER_CONFIGURE_ARGS "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
  endif()

  add_custom_target(sdk-consumer-test
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${LQ_SDK_CONSUMER_BUILD_DIR}"
    COMMAND "${CMAKE_COMMAND}" ${LQ_SDK_CONSUMER_CONFIGURE_ARGS}
    COMMAND "${CMAKE_COMMAND}" --build "${LQ_SDK_CONSUMER_BUILD_DIR}"
      --config "$<CONFIG>"
      --parallel
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${LQ_SDK_CONSUMER_BUILD_DIR}"
      -C "$<CONFIG>"
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
