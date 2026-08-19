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

# This is an internal switch used only by the small default Visual Studio
# preset. It deliberately is not a public build option: the compact solution
# is a presentation choice, not a different library configuration.
if(DEFINED MANUMESH_COMPACT_VISUAL_STUDIO_WORKSPACE)
  set(MANUMESH_COMPACT_VISUAL_STUDIO_WORKSPACE
    "${MANUMESH_COMPACT_VISUAL_STUDIO_WORKSPACE}"
    CACHE INTERNAL "Internal Visual Studio solution layout selector" FORCE
  )
else()
  set(MANUMESH_COMPACT_VISUAL_STUDIO_WORKSPACE OFF
    CACHE INTERNAL "Internal Visual Studio solution layout selector"
  )
endif()
set(MANUMESH_COMPACT_VS_TARGET_NAMES FALSE)
if(MANUMESH_COMPACT_VISUAL_STUDIO_WORKSPACE AND
   CMAKE_GENERATOR MATCHES "^Visual Studio ")
  set(MANUMESH_COMPACT_VS_TARGET_NAMES TRUE)
endif()

if(CMAKE_GENERATOR MATCHES "^Visual Studio ")
  # The compact workspace uses one meaningful Core folder. Full validation
  # solutions keep CMake's generated-target folder for its auxiliary targets.
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  if(MANUMESH_COMPACT_VS_TARGET_NAMES)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "")
  else()
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
  endif()
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
include(GNUInstallDirs)

option(MANUMESH_BUILD_SHARED_LIBRARY "构建 manumesh 共享库" ON)
option(MANUMESH_BUILD_CLI "构建 manumesh 命令行工具" ON)
option(MANUMESH_BUILD_EXAMPLES "构建使用该库的示例程序" ON)
option(MANUMESH_BUILD_TESTS "构建基于 GoogleTest 的测试" ON)
option(MANUMESH_ENABLE_INSTALL "启用 SDK 安装规则" OFF)
option(MANUMESH_MONOLITHIC_CORE
  "将核心源文件合并为单个 manumesh_core 目标；关闭时按功能模块生成核心目标"
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

# Keep canonical target aliases for in-tree consumers and SDK exports. The
# compact Visual Studio workspace uses concise physical target names because
# CMake's Visual Studio generator writes target names, rather than
# PROJECT_LABEL, into the solution tree.
set(MANUMESH_CORE_TARGET manumesh_core)
set(MANUMESH_CLI_TARGET manumesh)
set(MANUMESH_COMMON_OBJECT_TARGET manumesh_common_objects)
set(MANUMESH_MESH_EDIT_OBJECT_TARGET manumesh_mesh_edit_objects)
set(MANUMESH_ANALYSIS_OBJECT_TARGET manumesh_analysis_objects)
set(MANUMESH_GEOMETRY_OBJECT_TARGET manumesh_geometry_objects)
set(MANUMESH_IO_OBJECT_TARGET manumesh_io_objects)
set(MANUMESH_DEBUG_UTIL_OBJECT_TARGET manumesh_debug_util_objects)
set(MANUMESH_FEATURE_DETECTION_OBJECT_TARGET manumesh_feature_detection_objects)
set(MANUMESH_SIMPLIFICATION_OBJECT_TARGET manumesh_simplification_objects)
set(MANUMESH_C_API_OBJECT_TARGET manumesh_c_api_objects)
if(MANUMESH_COMPACT_VS_TARGET_NAMES)
  set(MANUMESH_CORE_TARGET Core)
  set(MANUMESH_CLI_TARGET CLI)
  set(MANUMESH_COMMON_OBJECT_TARGET Common)
  set(MANUMESH_MESH_EDIT_OBJECT_TARGET MeshEdit)
  set(MANUMESH_ANALYSIS_OBJECT_TARGET Analysis)
  set(MANUMESH_GEOMETRY_OBJECT_TARGET Geometry)
  set(MANUMESH_IO_OBJECT_TARGET IO)
  set(MANUMESH_DEBUG_UTIL_OBJECT_TARGET DebugUtil)
  set(MANUMESH_FEATURE_DETECTION_OBJECT_TARGET FeatureDetection)
  set(MANUMESH_SIMPLIFICATION_OBJECT_TARGET Simplification)
  set(MANUMESH_C_API_OBJECT_TARGET CAPI)
endif()
set(MANUMESH_ONETBB_PROVIDER "vendored" CACHE STRING
  "oneTBB 提供方式：默认 vendored；可选 auto、system 或 vendored"
)
set_property(CACHE MANUMESH_ONETBB_PROVIDER PROPERTY STRINGS auto system vendored)
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
  "Eigen 提供方式：默认 vendored；可选 auto、system 或 vendored"
)
set_property(CACHE MANUMESH_EIGEN_PROVIDER PROPERTY STRINGS auto system vendored)
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

if(NOT MANUMESH_EIGEN_PROVIDER MATCHES "^(auto|system|vendored)$")
  message(FATAL_ERROR
    "MANUMESH_EIGEN_PROVIDER 必须是 auto、system 或 vendored 之一。"
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
      "-DMANUMESH_EIGEN_PROVIDER=vendored。"
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

if(NOT MANUMESH_RESOLVED_EIGEN_PROVIDER AND TARGET Eigen3::Eigen)
  set(MANUMESH_RESOLVED_EIGEN_PROVIDER "external")
endif()

if(NOT MANUMESH_RESOLVED_EIGEN_PROVIDER)
  message(FATAL_ERROR
    "未找到 Eigen3。请安装系统 Eigen3，或确认 Vendored Eigen 位于 "
    "MANUMESH_EIGEN_SOURCE_DIR='${MANUMESH_EIGEN_SOURCE_DIR}'。"
  )
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

if(MANUMESH_ENABLE_ONETBB)
  if(NOT MANUMESH_ONETBB_PROVIDER MATCHES "^(auto|system|vendored)$")
    message(FATAL_ERROR
      "MANUMESH_ONETBB_PROVIDER 必须是 auto、system 或 vendored 之一。"
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
        "-DMANUMESH_ONETBB_PROVIDER=vendored。"
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

  if(NOT TARGET TBB::tbb)
    message(FATAL_ERROR
      "oneTBB 已启用，但未能创建 TBB::tbb 目标。请安装系统 oneTBB 2021.12，"
      "或确认 Vendored oneTBB 位于 "
      "MANUMESH_ONETBB_SOURCE_DIR='${MANUMESH_ONETBB_SOURCE_DIR}'。"
    )
  endif()

  if(MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "disabled" AND
     MANUMESH_ONETBB_PROVIDER STREQUAL "vendored")
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
      "${MANUMESH_RESOLVED_ONETBB_PROVIDER}。请使用 vendored。"
    )
  endif()
  if(MANUMESH_ENABLE_INSTALL AND
     MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "vendored")
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
  # All in-tree executables and the ManuMesh DLL use the common runtime
  # output directory, so the core DLL is already beside each executable.
  if(WIN32 AND MANUMESH_ONETBB_RUNTIME_DLL)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:TBB::tbb>"
        "$<TARGET_FILE_DIR:${target}>"
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
  # Object libraries are the useful module boundaries to browse and debug.
  # In the compact preset they share one Core folder; full configurations keep
  # the finer grained hierarchy that is useful while working on validation.
  set(manumesh_module_folder "Core")
  if(NOT MANUMESH_COMPACT_VS_TARGET_NAMES)
    set(manumesh_module_folder "Core/Implementation")
  endif()
  manumesh_set_ide_target("${MANUMESH_GEOMETRY_OBJECT_TARGET}" "Geometry" "${manumesh_module_folder}")
  manumesh_set_ide_target("${MANUMESH_COMMON_OBJECT_TARGET}" "Common" "${manumesh_module_folder}")
  manumesh_set_ide_target("${MANUMESH_MESH_EDIT_OBJECT_TARGET}" "Mesh Edit" "${manumesh_module_folder}")
  manumesh_set_ide_target("${MANUMESH_ANALYSIS_OBJECT_TARGET}" "Analysis" "${manumesh_module_folder}")
  manumesh_set_ide_target("${MANUMESH_IO_OBJECT_TARGET}" "IO" "${manumesh_module_folder}")
  manumesh_set_ide_target("${MANUMESH_DEBUG_UTIL_OBJECT_TARGET}" "Debug Util" "${manumesh_module_folder}")
  manumesh_set_ide_target("${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET}" "Feature Detection" "${manumesh_module_folder}")
  manumesh_set_ide_target("${MANUMESH_SIMPLIFICATION_OBJECT_TARGET}" "Simplification" "${manumesh_module_folder}")
  manumesh_set_ide_target("${MANUMESH_C_API_OBJECT_TARGET}" "C API" "${manumesh_module_folder}")
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
