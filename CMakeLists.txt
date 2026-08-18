cmake_minimum_required(VERSION 3.20)

# ManuMesh deliberately uses the release DLL CRT for every configuration.
# Debug also uses /MD, never /MDd, so the DLL and all in-tree consumers share
# one runtime ABI.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING
  "MSVC runtime for every ManuMesh target; Debug and Release both use /MD."
  FORCE
)
set_property(CACHE CMAKE_MSVC_RUNTIME_LIBRARY PROPERTY STRINGS MultiThreadedDLL)

project(ManuMesh
  VERSION 0.2.0
  DESCRIPTION "增材制造网格几何库"
  LANGUAGES C CXX
)

if(CMAKE_GENERATOR MATCHES "^Visual Studio " AND
   NOT CMAKE_GENERATOR STREQUAL "Visual Studio 16 2019")
  message(FATAL_ERROR
    "ManuMesh supports only the Visual Studio 16 2019 generator; detected "
    "'${CMAKE_GENERATOR}'. Use a vs2019-* preset."
  )
endif()

if(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR
   NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR
    "ManuMesh supports only Microsoft Visual C++ from Visual Studio 2019 "
    "(MSVC v142). Configure with the 'Visual Studio 16 2019' generator or "
    "from a VS2019 v142 developer environment."
  )
endif()
if(MSVC_VERSION LESS 1920 OR MSVC_VERSION GREATER_EQUAL 1930)
  message(FATAL_ERROR
    "ManuMesh requires MSVC v142 from Visual Studio 2019 "
    "(_MSC_VER 1920-1929); detected MSVC_VERSION=${MSVC_VERSION}."
  )
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT CMAKE_C_COMPILER_ARCHITECTURE_ID MATCHES "^[Xx]64$" OR
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^[Xx]64$")
  message(FATAL_ERROR
    "ManuMesh supports only the x64 target architecture; detected "
    "C='${CMAKE_C_COMPILER_ARCHITECTURE_ID}', "
    "CXX='${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}', "
    "pointer-size=${CMAKE_SIZEOF_VOID_P}."
  )
endif()

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
option(MANUMESH_ENABLE_ADDRESS_SANITIZER "启用 MSVC AddressSanitizer 内存错误检查" OFF)
option(MANUMESH_ENABLE_ONETBB
  "启用 oneTBB 内部并行执行后端；关闭时保持确定性的串行回退"
  OFF
)
set(MANUMESH_ONETBB_PROVIDER "auto" CACHE STRING
  "oneTBB 提供方式：auto、system 或 fetch"
)
set_property(CACHE MANUMESH_ONETBB_PROVIDER PROPERTY STRINGS auto system fetch)

if(MANUMESH_ENABLE_ADDRESS_SANITIZER)
  if(NOT MSVC OR MSVC_VERSION LESS 1928)
    message(FATAL_ERROR
      "MANUMESH_ENABLE_ADDRESS_SANITIZER requires VS2019 16.9 or newer "
      "within the supported MSVC v142 toolset."
    )
  endif()
  foreach(language C CXX)
    string(REPLACE "/RTC1" "" CMAKE_${language}_FLAGS_DEBUG
      "${CMAKE_${language}_FLAGS_DEBUG}"
    )
    string(REPLACE "/RTCsu" "" CMAKE_${language}_FLAGS_DEBUG
      "${CMAKE_${language}_FLAGS_DEBUG}"
    )
  endforeach()
  add_compile_options(/fsanitize=address)
  # /fsanitize=address is a cl.exe compile option. Instrumented objects carry
  # the ASan runtime directives; link.exe only needs incremental linking off.
  add_link_options(/INCREMENTAL:NO)
endif()

if(MANUMESH_ENABLE_INSTALL)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  include(InstallRequiredSystemLibraries)
endif()

set(MANUMESH_EIGEN_PROVIDER "vendored" CACHE STRING
  "Eigen 提供方式：默认 vendored；可选 auto、system 或 fetch"
)
set_property(CACHE MANUMESH_EIGEN_PROVIDER PROPERTY STRINGS auto system vendored fetch)
set(MANUMESH_EIGEN_SOURCE_DIR
  "${PROJECT_SOURCE_DIR}/thirdParty/eigen/include"
  CACHE PATH "Vendored Eigen 源码/include 树路径"
)

set(CMAKE_CXX_STANDARD 14)
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
    URL_HASH SHA256=8586084F71F9BDE545EE7FA6D00288B264A2B7AC3607B974E54D13E7162C1C72
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

set(MANUMESH_RESOLVED_ONETBB_PROVIDER "disabled")
set(MANUMESH_ONETBB_STATIC_SDK_DEPENDENCY FALSE)
set(MANUMESH_ONETBB_RUNTIME_DLL FALSE)

if(MANUMESH_ENABLE_ONETBB)
  if(NOT MANUMESH_ONETBB_PROVIDER MATCHES "^(auto|system|fetch)$")
    message(FATAL_ERROR
      "MANUMESH_ONETBB_PROVIDER 必须是 auto、system 或 fetch 之一。"
    )
  endif()

  if(MANUMESH_ONETBB_PROVIDER STREQUAL "auto" OR
     MANUMESH_ONETBB_PROVIDER STREQUAL "system")
    find_package(TBB 2021.12 CONFIG QUIET)
    if(TARGET TBB::tbb)
      set(MANUMESH_RESOLVED_ONETBB_PROVIDER "system")
    elseif(MANUMESH_ONETBB_PROVIDER STREQUAL "system")
      message(FATAL_ERROR
        "未找到系统 oneTBB (TBB::tbb)。请安装 oneTBB 2021.12 或使用 "
        "-DMANUMESH_ONETBB_PROVIDER=fetch。"
      )
    endif()
  endif()

  if(NOT TARGET TBB::tbb AND
     (MANUMESH_ONETBB_PROVIDER STREQUAL "auto" OR
      MANUMESH_ONETBB_PROVIDER STREQUAL "fetch"))
    # Keep oneTBB as its supported shared runtime. Static oneTBB is explicitly
    # discouraged upstream; the install rules below package the runtime DLL
    # and its CMake target for both shared and static ManuMesh SDKs.
    set(TBB_TEST OFF CACHE BOOL "Disable oneTBB tests" FORCE)
    set(TBB_STRICT OFF CACHE BOOL "Do not treat oneTBB warnings as errors" FORCE)
    set(TBBMALLOC_BUILD OFF CACHE BOOL "Do not build oneTBB allocator" FORCE)
    set(TBB4PY_BUILD OFF CACHE BOOL "Do not build oneTBB Python bindings" FORCE)
    set(TBB_INSTALL ON CACHE BOOL "Install fetched oneTBB for static SDK consumers" FORCE)
    FetchContent_Declare(
      onetbb
      URL https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.12.0.tar.gz
      URL_HASH SHA256=C7BB7AA69C254D91B8F0041A71C5BCC3936ACB64408A1719AEC0B2B7639DD84F
      ${MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS}
    )
    FetchContent_MakeAvailable(onetbb)
    set(MANUMESH_RESOLVED_ONETBB_PROVIDER "fetch")
  endif()

  if(NOT TARGET TBB::tbb)
    message(FATAL_ERROR "oneTBB 已启用，但未能创建 TBB::tbb 目标。")
  endif()

  get_target_property(MANUMESH_ONETBB_TARGET_TYPE TBB::tbb TYPE)
  if(MANUMESH_ONETBB_TARGET_TYPE STREQUAL "SHARED_LIBRARY")
    set(MANUMESH_ONETBB_RUNTIME_DLL TRUE)
  endif()
  if(NOT MANUMESH_BUILD_SHARED_LIBRARY)
    set(MANUMESH_ONETBB_STATIC_SDK_DEPENDENCY TRUE)
  endif()
  message(STATUS
    "ManuMesh oneTBB backend: ${MANUMESH_RESOLVED_ONETBB_PROVIDER} "
    "(${MANUMESH_ONETBB_TARGET_TYPE})"
  )
endif()

function(manumesh_apply_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /FS /external:anglebrackets /external:W0>
    $<$<COMPILE_LANGUAGE:C>:/W4 /utf-8 /FS>
  )
endfunction()

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
  if(WIN32 AND MANUMESH_ONETBB_RUNTIME_DLL)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:TBB::tbb>"
        "$<TARGET_FILE_DIR:${target}>"
      COMMENT "正在将 oneTBB 运行时复制到 ${target} 旁边"
      VERBATIM
    )
  endif()
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
