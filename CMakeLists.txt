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
option(MANUMESH_ENABLE_INSTALL "Enable SDK install rules" OFF)
option(MANUMESH_ENABLE_DEBUG_UTIL "Enable Debug-only internal HTML wireframe debug utility" OFF)

set(MANUMESH_EIGEN_PROVIDER "auto" CACHE STRING
  "Eigen provider: auto, system, vendored, or fetch"
)
set_property(CACHE MANUMESH_EIGEN_PROVIDER PROPERTY STRINGS auto system vendored fetch)
set(MANUMESH_EIGEN_SOURCE_DIR
  "${PROJECT_SOURCE_DIR}/thirdParty/eigen/include"
  CACHE PATH "Path to a vendored Eigen source/include tree"
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

if(MANUMESH_EIGEN_PROVIDER STREQUAL "auto" OR
   MANUMESH_EIGEN_PROVIDER STREQUAL "system")
  find_package(Eigen3 3.3 QUIET NO_MODULE)
endif()

if(NOT TARGET Eigen3::Eigen AND
   (MANUMESH_EIGEN_PROVIDER STREQUAL "auto" OR
    MANUMESH_EIGEN_PROVIDER STREQUAL "vendored"))
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
   (MANUMESH_EIGEN_PROVIDER STREQUAL "auto" OR
    MANUMESH_EIGEN_PROVIDER STREQUAL "fetch"))
  FetchContent_Declare(
    eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
    ${MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS}
  )
  set(MANUMESH_SAVED_BUILD_TESTING "${BUILD_TESTING}")
  set(BUILD_TESTING OFF CACHE BOOL "Disable dependency tests while fetching Eigen" FORCE)
  FetchContent_MakeAvailable(eigen)
  set(BUILD_TESTING "${MANUMESH_SAVED_BUILD_TESTING}" CACHE BOOL
    "Enable project tests"
    FORCE
  )
endif()

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

function(manumesh_copy_mingw_runtime_dependencies target)
  if(WIN32 AND MINGW)
    if(NOT TARGET manumesh_mingw_runtime_dependencies)
      get_filename_component(MANUMESH_MINGW_COMPILER_DIR "${CMAKE_CXX_COMPILER}"
        DIRECTORY
      )
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

add_subdirectory(src)

if(MANUMESH_BUILD_CLI)
  add_subdirectory(apps/manumesh)
endif()

if(MANUMESH_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(BUILD_TESTING AND MANUMESH_BUILD_TESTS)
  add_subdirectory(tests)
endif()

add_subdirectory(adm)
