cmake_minimum_required(VERSION 3.18)

project(ManuMesh
  VERSION 0.2.0
  DESCRIPTION "增材制造网格几何库"
  LANGUAGES C CXX
)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

set(MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
  list(APPEND MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()

option(MANUMESH_BUILD_SHARED_LIBRARY "构建 manumesh 共享库" ON)
option(MANUMESH_BUILD_CLI "构建 manumesh 命令行工具" ON)
option(MANUMESH_BUILD_EXAMPLES "构建使用该库的示例程序" ON)
option(MANUMESH_BUILD_TESTS "构建基于 GoogleTest 的测试" ON)
option(MANUMESH_ENABLE_INSTALL "启用 SDK 安装规则" OFF)
option(MANUMESH_ENABLE_DEBUG_UTIL "启用仅 Debug 可用的内部 HTML 线框调试工具" OFF)

if(MANUMESH_ENABLE_INSTALL AND MSVC)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  include(InstallRequiredSystemLibraries)
endif()

set(MANUMESH_EIGEN_PROVIDER "auto" CACHE STRING
  "Eigen 提供方式：auto、system、vendored 或 fetch"
)
set_property(CACHE MANUMESH_EIGEN_PROVIDER PROPERTY STRINGS auto system vendored fetch)
set(MANUMESH_EIGEN_SOURCE_DIR
  "${PROJECT_SOURCE_DIR}/thirdParty/eigen/include"
  CACHE PATH "Vendored Eigen 源码/include 树路径"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(NOT MANUMESH_EIGEN_PROVIDER MATCHES "^(auto|system|vendored|fetch)$")
  message(FATAL_ERROR
    "MANUMESH_EIGEN_PROVIDER 必须是 auto、system、vendored 或 fetch 之一。"
  )
endif()

set(MANUMESH_RESOLVED_EIGEN_PROVIDER "")

if(MANUMESH_EIGEN_PROVIDER STREQUAL "auto" OR
   MANUMESH_EIGEN_PROVIDER STREQUAL "system")
  find_package(Eigen3 3.3 QUIET NO_MODULE)
  if(TARGET Eigen3::Eigen)
    set(MANUMESH_RESOLVED_EIGEN_PROVIDER "system")
  elseif(MANUMESH_EIGEN_PROVIDER STREQUAL "system")
    message(FATAL_ERROR
      "未找到系统 Eigen3。请安装 Eigen3，或使用 "
      "-DMANUMESH_EIGEN_PROVIDER=vendored/fetch。"
    )
  endif()
endif()

if(NOT TARGET Eigen3::Eigen AND
   (MANUMESH_EIGEN_PROVIDER STREQUAL "auto" OR
    MANUMESH_EIGEN_PROVIDER STREQUAL "vendored"))
  if(EXISTS "${MANUMESH_EIGEN_SOURCE_DIR}/Eigen/Core")
    add_library(Eigen3::Eigen INTERFACE IMPORTED)
    target_include_directories(Eigen3::Eigen INTERFACE "${MANUMESH_EIGEN_SOURCE_DIR}")
    set(MANUMESH_RESOLVED_EIGEN_PROVIDER "vendored")
  elseif(MANUMESH_EIGEN_PROVIDER STREQUAL "vendored")
    message(FATAL_ERROR
      "在以下路径未找到 Vendored Eigen："
      "MANUMESH_EIGEN_SOURCE_DIR='${MANUMESH_EIGEN_SOURCE_DIR}'。"
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
  set(BUILD_TESTING OFF CACHE BOOL "获取 Eigen 时禁用依赖项测试" FORCE)
  FetchContent_MakeAvailable(eigen)
  set(BUILD_TESTING "${MANUMESH_SAVED_BUILD_TESTING}" CACHE BOOL
    "启用项目测试"
    FORCE
  )
  set(MANUMESH_RESOLVED_EIGEN_PROVIDER "fetch")
endif()

if(NOT MANUMESH_RESOLVED_EIGEN_PROVIDER AND TARGET Eigen3::Eigen)
  set(MANUMESH_RESOLVED_EIGEN_PROVIDER "external")
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

function(manumesh_collect_mingw_runtime_dependencies output_var)
  if(WIN32 AND MINGW)
    get_filename_component(MANUMESH_MINGW_COMPILER_DIR "${CMAKE_CXX_COMPILER}"
      DIRECTORY
    )
    set(MANUMESH_MINGW_RUNTIME_CANDIDATES
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
    list(APPEND MANUMESH_MINGW_RUNTIME_CANDIDATES ${MANUMESH_MINGW_GCC_RUNTIME_FILES})
    list(REMOVE_DUPLICATES MANUMESH_MINGW_RUNTIME_CANDIDATES)

    set(MANUMESH_MINGW_RUNTIME_FILES)
    foreach(mingw_runtime_file IN LISTS MANUMESH_MINGW_RUNTIME_CANDIDATES)
      if(EXISTS "${mingw_runtime_file}")
        list(APPEND MANUMESH_MINGW_RUNTIME_FILES "${mingw_runtime_file}")
      endif()
    endforeach()
    set(${output_var} "${MANUMESH_MINGW_RUNTIME_FILES}" PARENT_SCOPE)
  else()
    set(${output_var} "" PARENT_SCOPE)
  endif()
endfunction()

function(manumesh_copy_mingw_runtime_dependencies target)
  if(WIN32 AND MINGW)
    if(NOT TARGET manumesh_mingw_runtime_dependencies)
      manumesh_collect_mingw_runtime_dependencies(MANUMESH_MINGW_RUNTIME_FILES)

      set(MANUMESH_MINGW_RUNTIME_COPY_COMMANDS
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:manumesh_core>"
      )
      foreach(mingw_runtime_file IN LISTS MANUMESH_MINGW_RUNTIME_FILES)
        list(APPEND MANUMESH_MINGW_RUNTIME_COPY_COMMANDS
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${mingw_runtime_file}"
            "$<TARGET_FILE_DIR:manumesh_core>"
        )
      endforeach()

      add_custom_target(manumesh_mingw_runtime_dependencies
        ${MANUMESH_MINGW_RUNTIME_COPY_COMMANDS}
    COMMENT "正在复制 MinGW 运行时依赖"
        VERBATIM
      )
    endif()

    add_dependencies(${target} manumesh_mingw_runtime_dependencies)
  endif()
endfunction()

if(MANUMESH_ENABLE_INSTALL AND WIN32 AND MINGW)
  manumesh_collect_mingw_runtime_dependencies(MANUMESH_INSTALL_MINGW_RUNTIME_FILES)
  if(MANUMESH_INSTALL_MINGW_RUNTIME_FILES)
    install(FILES ${MANUMESH_INSTALL_MINGW_RUNTIME_FILES}
      DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
  endif()
endif()

function(manumesh_copy_runtime_dependencies target)
  if(WIN32 AND MANUMESH_BUILD_SHARED_LIBRARY)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:manumesh_core>"
        "$<TARGET_FILE_DIR:${target}>"
    COMMENT "正在将 manumesh 运行时库复制到 ${target} 旁边"
      VERBATIM
    )
  endif()
  manumesh_copy_mingw_runtime_dependencies(${target})
endfunction()

add_subdirectory(src)

if(MANUMESH_BUILD_CLI)
  add_subdirectory(apps)
endif()

if(MANUMESH_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(BUILD_TESTING AND MANUMESH_BUILD_TESTS)
  add_subdirectory(tests)
endif()

add_subdirectory(adm)
