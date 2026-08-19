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

if(CMAKE_GENERATOR MATCHES "^Visual Studio ")
  # Keep generated Visual Studio solutions navigable without changing the
  # canonical CMake target names used by builds, SDK exports, and scripts.
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
endif()

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
option(MANUMESH_MONOLITHIC_CORE
  "将核心源文件合并为单个 manumesh_core 目标，适合精简 IDE 工作区"
  OFF
)
option(MANUMESH_ENABLE_DEVELOPER_TOOLS
  "启用格式、文档和 SDK 验证等开发辅助目标"
  ON
)
option(MANUMESH_ENABLE_DEBUG_UTIL "启用仅 Debug 可用的内部 HTML 线框调试工具" OFF)
option(MANUMESH_ENABLE_ADDRESS_SANITIZER "启用 MSVC AddressSanitizer 内存错误检查" OFF)
option(MANUMESH_ENABLE_ONETBB
  "启用 oneTBB 内部并行执行后端；关闭时保持确定性的串行回退"
  OFF
)

# Keep canonical target aliases for scripts and SDK consumers, while allowing
# the compact Visual Studio workspace to show neutral project names.
set(MANUMESH_CORE_TARGET manumesh_core)
set(MANUMESH_CLI_TARGET manumesh)
if(MANUMESH_MONOLITHIC_CORE AND CMAKE_GENERATOR MATCHES "^Visual Studio ")
  set(MANUMESH_CORE_TARGET Core)
  set(MANUMESH_CLI_TARGET CLI)
endif()
set(MANUMESH_ONETBB_PROVIDER "vendored" CACHE STRING
  "oneTBB 提供方式：默认 vendored；可选 auto、system 或 fetch"
)
set_property(CACHE MANUMESH_ONETBB_PROVIDER PROPERTY STRINGS auto system vendored fetch)
set(MANUMESH_ONETBB_SOURCE_DIR
  "${PROJECT_SOURCE_DIR}/thirdParty/oneTBB/source"
  CACHE PATH "Vendored oneTBB 源码树路径"
)

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
set(MANUMESH_ONETBB_NOTICES_INSTALLED FALSE)

function(manumesh_configure_onetbb_build)
  # oneTBB is an internal backend. Build just its shared runtime and install
  # metadata needed when producing the static ManuMesh SDK.
  set(TBB_BUILD ON CACHE BOOL "Build the oneTBB runtime" FORCE)
  set(TBB_TEST OFF CACHE BOOL "Disable oneTBB tests" FORCE)
  set(TBB_EXAMPLES OFF CACHE BOOL "Disable oneTBB examples" FORCE)
  set(TBB_BENCH OFF CACHE BOOL "Disable oneTBB benchmarks" FORCE)
  set(TBB_FUZZ_TESTING OFF CACHE BOOL "Disable oneTBB fuzz tests" FORCE)
  set(TBB_STRICT OFF CACHE BOOL "Do not treat oneTBB warnings as errors" FORCE)
  set(TBBMALLOC_BUILD OFF CACHE BOOL "Do not build oneTBB allocator" FORCE)
  set(TBB4PY_BUILD OFF CACHE BOOL "Do not build oneTBB Python bindings" FORCE)
  set(TBB_INSTALL ON CACHE BOOL "Install oneTBB for static SDK consumers" FORCE)
endfunction()

function(manumesh_add_vendored_onetbb source_dir binary_dir)
  # oneTBB 2021.12 declares CMake 3.5 support. CMake 4 warns about that
  # legacy policy range, so raise only the third-party subdirectory baseline.
  if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
  elseif(CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS 3.10)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
  endif()

  # Keep the shared-runtime choice local to the third-party subdirectory. The
  # parent may still intentionally build ManuMesh itself as a static archive.
  set(BUILD_SHARED_LIBS ON)
  set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DOCDIR}/oneTBB")
  manumesh_configure_onetbb_build()
  # The static SDK export refers to TBB::tbb.  Keep oneTBB's install rules in
  # the parent install graph so the runtime DLL, import library, headers, and
  # TBB CMake package are delivered together with ManuMesh.
  add_subdirectory("${source_dir}" "${binary_dir}")
  if(MANUMESH_ENABLE_INSTALL)
    install(FILES
      "${source_dir}/LICENSE.txt"
      "${source_dir}/third-party-programs.txt"
      DESTINATION "${CMAKE_INSTALL_DOCDIR}"
    )
  endif()
endfunction()

function(manumesh_add_fetched_onetbb)
  # FetchContent_MakeAvailable executes add_subdirectory in this function's
  # scope, so BUILD_SHARED_LIBS remains local to oneTBB as well.
  set(BUILD_SHARED_LIBS ON)
  set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DOCDIR}/oneTBB")
  manumesh_configure_onetbb_build()
  FetchContent_MakeAvailable(onetbb)
  if(MANUMESH_ENABLE_INSTALL)
    FetchContent_GetProperties(onetbb)
    install(FILES
      "${onetbb_SOURCE_DIR}/LICENSE.txt"
      "${onetbb_SOURCE_DIR}/third-party-programs.txt"
      DESTINATION "${CMAKE_INSTALL_DOCDIR}"
    )
  endif()
endfunction()

if(MANUMESH_ENABLE_ONETBB)
  if(NOT MANUMESH_ONETBB_PROVIDER MATCHES "^(auto|system|vendored|fetch)$")
    message(FATAL_ERROR
      "MANUMESH_ONETBB_PROVIDER 必须是 auto、system、vendored 或 fetch 之一。"
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
        "-DMANUMESH_ONETBB_PROVIDER=vendored 或 fetch。"
      )
    endif()
  endif()

  if(NOT TARGET TBB::tbb AND
     (MANUMESH_ONETBB_PROVIDER STREQUAL "auto" OR
      MANUMESH_ONETBB_PROVIDER STREQUAL "vendored"))
    if(EXISTS "${MANUMESH_ONETBB_SOURCE_DIR}/CMakeLists.txt")
      manumesh_add_vendored_onetbb(
        "${MANUMESH_ONETBB_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/manumesh-onetbb-build"
      )
      set(MANUMESH_RESOLVED_ONETBB_PROVIDER "vendored")
    elseif(MANUMESH_ONETBB_PROVIDER STREQUAL "vendored")
      message(FATAL_ERROR
        "在以下路径未找到 Vendored oneTBB 源码："
        "MANUMESH_ONETBB_SOURCE_DIR='${MANUMESH_ONETBB_SOURCE_DIR}'。"
      )
    endif()
  endif()

  if(NOT TARGET TBB::tbb AND
     (MANUMESH_ONETBB_PROVIDER STREQUAL "auto" OR
      MANUMESH_ONETBB_PROVIDER STREQUAL "fetch"))
    # Keep oneTBB as its supported shared runtime. Static oneTBB is explicitly
    # discouraged upstream; the install rules below package the runtime DLL
    # and its CMake target for both shared and static ManuMesh SDKs.
    FetchContent_Declare(
      onetbb
      URL https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.12.0.tar.gz
      URL_HASH SHA256=C7BB7AA69C254D91B8F0041A71C5BCC3936ACB64408A1719AEC0B2B7639DD84F
      ${MANUMESH_FETCHCONTENT_TIMESTAMP_ARGS}
    )
    manumesh_add_fetched_onetbb()
    set(MANUMESH_RESOLVED_ONETBB_PROVIDER "fetch")
  endif()

  if(NOT TARGET TBB::tbb)
    message(FATAL_ERROR "oneTBB 已启用，但未能创建 TBB::tbb 目标。")
  endif()

  if(MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "disabled" AND
     (MANUMESH_ONETBB_PROVIDER STREQUAL "vendored" OR
      MANUMESH_ONETBB_PROVIDER STREQUAL "fetch"))
    message(FATAL_ERROR
      "已存在外部 TBB::tbb 目标，无法满足显式 provider="
      "${MANUMESH_ONETBB_PROVIDER}。请移除该目标，或改用 auto/system。"
    )
  endif()

  if(MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "disabled")
    # A superproject may have created TBB::tbb before ManuMesh was added.
    # Keep the provider state truthful and do not pretend its package is part
    # of the ManuMesh SDK installation.
    set(MANUMESH_RESOLVED_ONETBB_PROVIDER "external")
  endif()

  get_target_property(MANUMESH_ONETBB_TARGET_TYPE TBB::tbb TYPE)
  if(MANUMESH_ONETBB_TARGET_TYPE STREQUAL "SHARED_LIBRARY")
    set(MANUMESH_ONETBB_RUNTIME_DLL TRUE)
  endif()
  if(NOT MANUMESH_BUILD_SHARED_LIBRARY)
    set(MANUMESH_ONETBB_STATIC_SDK_DEPENDENCY TRUE)
  endif()
  if(NOT MANUMESH_BUILD_SHARED_LIBRARY AND
     (MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "system" OR
      MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "external"))
    message(FATAL_ERROR
      "静态 ManuMesh SDK 需要可随 SDK 安装的 oneTBB；当前 provider="
      "${MANUMESH_RESOLVED_ONETBB_PROVIDER}。请使用 vendored 或 fetch。"
    )
  endif()
  if(MANUMESH_ENABLE_INSTALL AND
     (MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "vendored" OR
      MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "fetch"))
    set(MANUMESH_ONETBB_NOTICES_INSTALLED TRUE)
  endif()
  message(STATUS
    "ManuMesh oneTBB backend: ${MANUMESH_RESOLVED_ONETBB_PROVIDER} "
    "(${MANUMESH_ONETBB_TARGET_TYPE})"
  )
endif()

function(manumesh_apply_warnings target)
  if(MSVC)
    # Embed Debug information in objects. Visual Studio otherwise records a
    # compiler-PDB path relative to each target; object libraries are later
    # linked from several directories and trigger noisy LNK4099 diagnostics.
    set_property(TARGET "${target}" PROPERTY MSVC_DEBUG_INFORMATION_FORMAT "Embedded")
  endif()
  target_compile_options(${target} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /utf-8 /FS /external:anglebrackets /external:W0>
    $<$<COMPILE_LANGUAGE:C>:/W4 /utf-8 /FS>
  )
endfunction()

function(manumesh_copy_runtime_dependencies target)
  if(WIN32 AND MANUMESH_BUILD_SHARED_LIBRARY)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:${MANUMESH_CORE_TARGET}>"
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

function(manumesh_set_ide_target target label folder)
  if(TARGET "${target}")
    set_target_properties("${target}" PROPERTIES
      FOLDER "${folder}"
      PROJECT_LABEL "${label}"
    )
  endif()
endfunction()

function(manumesh_configure_visual_studio_layout)
  if(NOT CMAKE_GENERATOR MATCHES "^Visual Studio ")
    return()
  endif()

  if(TARGET "${MANUMESH_CLI_TARGET}")
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "${MANUMESH_CLI_TARGET}")
  endif()

  manumesh_set_ide_target("${MANUMESH_CLI_TARGET}" "CLI" "Applications")

  manumesh_set_ide_target("${MANUMESH_CORE_TARGET}" "Core" "Core")
  manumesh_set_ide_target(manumesh_cpp_api "C++ API" "Core/API")
  manumesh_set_ide_target(manumesh_c_api "C API" "Core/API")
  manumesh_set_ide_target(manumesh_common_objects "Common Objects" "Core/Implementation")
  manumesh_set_ide_target(manumesh_mesh_edit_objects "Mesh Edit Objects" "Core/Implementation")
  manumesh_set_ide_target(manumesh_analysis_objects "Analysis Objects" "Core/Implementation")
  manumesh_set_ide_target(manumesh_geometry_objects "Geometry Objects" "Core/Implementation")
  manumesh_set_ide_target(manumesh_io_objects "IO Objects" "Core/Implementation")
  manumesh_set_ide_target(manumesh_debug_util_objects "Debug Utility Objects" "Core/Implementation")
  manumesh_set_ide_target(manumesh_feature_detection_objects "Feature Detection Objects" "Core/Implementation")
  manumesh_set_ide_target(manumesh_simplification_objects "Simplification Objects" "Core/Implementation")
  manumesh_set_ide_target(manumesh_c_api_objects "C API Objects" "Core/Implementation")
  manumesh_set_ide_target(manumesh_internal "Internal Test Library" "Validation/Internal")

  manumesh_set_ide_target(manumesh_basic_simplify "Basic Simplification" "Examples")
  manumesh_set_ide_target(manumesh_c_api_basic "C API Basics" "Examples")
  manumesh_set_ide_target(manumesh_feature_workflow_demo "Feature Workflow" "Examples")

  manumesh_set_ide_target(manumesh_tests "Unit Tests" "Validation/Unit")
  manumesh_set_ide_target(manumesh_external_tests "External Tests" "Validation/External")
  manumesh_set_ide_target(manumesh_ownership_lifetime_stress "Ownership Lifetime Stress" "Validation/Memory")
  manumesh_set_ide_target(manumesh_c_api_abi_stress "C API ABI Stress" "Validation/ABI")
  manumesh_set_ide_target(manumesh_cpp_c_api_abi_stress "C++ C API ABI Stress" "Validation/ABI")
  manumesh_set_ide_target(manumesh_options_header_compile_check "Public Header Check" "Architecture/Include Boundaries")
  manumesh_set_ide_target(manumesh_feature_detection_link_check "Feature Detection Link Check" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_simplification_link_check "Simplification Link Check" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_io_link_check "IO Link Check" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_performance_tests "Performance Tests" "Validation/Performance")
  manumesh_set_ide_target(unit-tests "Run Unit Tests" "Validation/Commands")
  manumesh_set_ide_target(external-tests "Run External Tests" "Validation/Commands")
  manumesh_set_ide_target(performance-tests "Run Performance Tests" "Validation/Commands")

  manumesh_set_ide_target(manumesh_geometry_boundary_module "Geometry Boundary" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_common_boundary_module "Common Boundary" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_mesh_edit_boundary_module "Mesh Edit Boundary" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_analysis_boundary_module "Analysis Boundary" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_io_boundary_module "IO Boundary" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_debug_util_boundary_module "Debug Utility Boundary" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_feature_detection_boundary_module "Feature Detection Boundary" "Architecture/Module Boundaries")
  manumesh_set_ide_target(manumesh_simplification_boundary_module "Simplification Boundary" "Architecture/Module Boundaries")

  manumesh_set_ide_target(gtest "GoogleTest" "Dependencies")
  manumesh_set_ide_target(gtest_main "GoogleTest Main" "Dependencies")

  manumesh_set_ide_target(check-src-doxygen "Check Documentation Comments" "Tools")
  manumesh_set_ide_target(format "Format Sources" "Tools")
  manumesh_set_ide_target(check-format "Check Formatting" "Tools")
  manumesh_set_ide_target(docs-api "API Documentation" "Tools/Documentation")
  manumesh_set_ide_target(docs-internal "Internal Documentation" "Tools/Documentation")
  manumesh_set_ide_target(sdk-install-local "Install Local SDK" "Packaging")
  manumesh_set_ide_target(sdk-runtime-test "Test SDK Runtime" "Packaging")
  manumesh_set_ide_target(sdk-c-api-consumer-test "Test C API Consumer" "Packaging")
  manumesh_set_ide_target(sdk-consumer-test "Test SDK Consumers" "Packaging")
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

if(MANUMESH_ENABLE_DEVELOPER_TOOLS)
  add_subdirectory(adm)
endif()

manumesh_configure_visual_studio_layout()
