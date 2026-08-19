set(MANUMESH_PUBLIC_HEADERS
  "${PROJECT_SOURCE_DIR}/include/Export.h"
  "${PROJECT_SOURCE_DIR}/include/api/CApi.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/analysis/MeshAnalysis.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureAnalysisViews.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureComparison.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureDetector.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureOptions.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureTypes.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/Metrics.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/MeshCoreQem.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/PlainSimplifier.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/QEMSimplifier.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/SimplificationTypes.h"
  "${PROJECT_SOURCE_DIR}/include/core/Handles.h"
  "${PROJECT_SOURCE_DIR}/include/core/Filesystem.h"
  "${PROJECT_SOURCE_DIR}/include/core/ExecutionOptions.h"
  "${PROJECT_SOURCE_DIR}/include/core/MathUtils.h"
  "${PROJECT_SOURCE_DIR}/include/core/Mesh.h"
  "${PROJECT_SOURCE_DIR}/include/core/MeshGenerators.h"
  "${PROJECT_SOURCE_DIR}/include/core/MeshTopology.h"
  "${PROJECT_SOURCE_DIR}/include/core/PlainMesh.h"
  "${PROJECT_SOURCE_DIR}/include/core/Optional.h"
  "${PROJECT_SOURCE_DIR}/include/core/Status.h"
  "${PROJECT_SOURCE_DIR}/include/io/MeshIo.h"
  "${PROJECT_SOURCE_DIR}/include/io/PartitionedMeshDataset.h"
)

set(MANUMESH_API_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/api/CApiConverters.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/api/CApi.cpp"
)

set(MANUMESH_COMMON_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/common/GeometryPredicates.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/MeshDistanceIndex.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/MeshQueries.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/ParallelExecution.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/SpatialIndex.cpp"
)

set(MANUMESH_MESH_EDIT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/mesh_edit/DynamicTopology.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/mesh_edit/MeshCompaction.cpp"
)

set(MANUMESH_ANALYSIS_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/analysis/MeshAnalysis.cpp"
)

set(MANUMESH_GEOMETRY_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/core/Mesh.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/MeshGenerators.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/MeshTopology.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/PlainMesh.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/Status.cpp"
)

set(MANUMESH_IO_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/io/MeshIo.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/io/PartitionedMeshDataset.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/io/PartitionedMeshValidation.cpp"
)

set(MANUMESH_DEBUG_UTIL_SOURCES)
set(MANUMESH_DEBUG_UTIL_PRIVATE_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/debugUtil/debugUtil.h"
)
if(MANUMESH_ENABLE_DEBUG_UTIL)
  list(APPEND MANUMESH_DEBUG_UTIL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/debugUtil/debugUtil.cpp"
  )
endif()

set(MANUMESH_FEATURE_DETECTION_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureAnalysisValidation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureBenchmark.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureCircularRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureComparison.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureCycleRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureDetector.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureEvidence.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureGraph.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureGraphCompatibility.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureGraphCleanup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureGraphConsolidation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureLoopBuilder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureLoopRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeaturePrimitiveRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureTraceRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/NormalTensor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureNormalFilter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/PrimitiveFit.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureSegmentation.cpp"
)

set_source_files_properties(
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/PrimitiveFit.cpp"
  PROPERTIES COMPILE_OPTIONS "/bigobj"
)

set(MANUMESH_SIMPLIFICATION_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CandidateQueue.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CollapseAttempt.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CollapseLegality.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CollapseTopology.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/FeatureConstraintGraph.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/FeatureConstraints.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/FeatureGuidance.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/Metrics.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/Placement.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/QEMSimplifier.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/Quadrics.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/QualityRefinement.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SimplificationPolicies.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SimplificationRun.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SimplificationValidation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SpatialFaceIndex.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/TextureProtection.cpp"
)

set(MANUMESH_MESHCORE_FEATURE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/meshcore/features/FeatureClassification.cpp"
)
set(MANUMESH_MESHCORE_SIMPLIFICATION_SOURCES
  "${PROJECT_SOURCE_DIR}/src/meshcore/simplification/MeshCoreQem.cpp"
)
set(MANUMESH_MESHCORE_PRIVATE_HEADERS
  "${PROJECT_SOURCE_DIR}/src/meshcore/features/FeatureClassification.h"
  "${PROJECT_SOURCE_DIR}/src/meshcore/simplification/MeshCoreQem.h"
)

set(MANUMESH_LIBRARY_SOURCES
  ${MANUMESH_API_SOURCES}
  ${MANUMESH_COMMON_SOURCES}
  ${MANUMESH_MESH_EDIT_SOURCES}
  ${MANUMESH_ANALYSIS_SOURCES}
  ${MANUMESH_GEOMETRY_SOURCES}
  ${MANUMESH_IO_SOURCES}
  ${MANUMESH_DEBUG_UTIL_SOURCES}
  ${MANUMESH_FEATURE_DETECTION_SOURCES}
  ${MANUMESH_SIMPLIFICATION_SOURCES}
  ${MANUMESH_MESHCORE_FEATURE_SOURCES}
  ${MANUMESH_MESHCORE_SIMPLIFICATION_SOURCES}
)

set(MANUMESH_COMMON_PRIVATE_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/GeometryPredicates.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/MathConstants.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/MeshDistanceIndex.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/MeshQueries.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/ParallelExecution.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/SpatialIndex.h"
)

set(MANUMESH_MESH_EDIT_PRIVATE_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/mesh_edit/detail/DynamicTopology.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/mesh_edit/detail/MeshCompaction.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/mesh_edit/detail/MeshEditTypes.h"
)

set(MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureCircularRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureCycleRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureDebugInstrumentation.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureDetectionTypes.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureEvidence.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureGraph.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureGraphCompatibility.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureGraphCleanup.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureGraphConsolidation.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureLoopBuilder.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureLoopRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeaturePrimitiveRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureNormalFilter.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureSegmentation.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureTraceRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/PrimitiveFit.h"
)

set(MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/CandidateQueue.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/CollapseAttempt.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/CollapseLegality.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/CollapseTopology.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/FeatureConstraints.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/FeatureConstraintGraph.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/FeatureGuidance.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/Placement.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/Quadrics.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/QualityRefinement.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationPolicies.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationRun.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationTypes.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationValidation.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SpatialFaceIndex.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/TextureProtection.h"
)

set(MANUMESH_PRIVATE_HEADERS
  ${MANUMESH_COMMON_PRIVATE_HEADERS}
  ${MANUMESH_MESH_EDIT_PRIVATE_HEADERS}
  ${MANUMESH_DEBUG_UTIL_PRIVATE_HEADERS}
  ${MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS}
  ${MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS}
  ${MANUMESH_MESHCORE_PRIVATE_HEADERS}
)

set(MANUMESH_COMMON_MODULE_DEPENDENCIES geometry)
set(MANUMESH_MESH_EDIT_MODULE_DEPENDENCIES common geometry)
set(MANUMESH_ANALYSIS_MODULE_DEPENDENCIES common geometry)
set(MANUMESH_IO_MODULE_DEPENDENCIES geometry)
set(MANUMESH_DEBUG_UTIL_MODULE_DEPENDENCIES geometry)
set(MANUMESH_FEATURE_DETECTION_MODULE_DEPENDENCIES common geometry)
if(MANUMESH_ENABLE_DEBUG_UTIL)
  list(APPEND MANUMESH_FEATURE_DETECTION_MODULE_DEPENDENCIES debug_util)
endif()
set(MANUMESH_SIMPLIFICATION_MODULE_DEPENDENCIES
  analysis
  common
  geometry
  feature_detection
  mesh_edit
)
set(MANUMESH_C_API_MODULE_DEPENDENCIES
  analysis
  feature_detection
  geometry
  io
  simplification
)

function(manumesh_link_module_dependencies target module suffix visibility)
  string(TOUPPER "${module}" module_upper)
  set(dependency_variable "MANUMESH_${module_upper}_MODULE_DEPENDENCIES")
  set(dependency_targets)
  foreach(dependency IN LISTS ${dependency_variable})
    if(suffix STREQUAL "objects")
      string(TOUPPER "${dependency}" dependency_upper)
      set(dependency_target_variable
        "MANUMESH_${dependency_upper}_OBJECT_TARGET"
      )
      list(APPEND dependency_targets "${${dependency_target_variable}}")
    else()
      list(APPEND dependency_targets "manumesh_${dependency}_${suffix}")
    endif()
  endforeach()
  if(dependency_targets)
    target_link_libraries(${target} ${visibility} ${dependency_targets})
  endif()
endfunction()

if(MANUMESH_MONOLITHIC_CORE AND BUILD_TESTING AND MANUMESH_BUILD_TESTS)
  message(FATAL_ERROR
    "MANUMESH_MONOLITHIC_CORE 仅适用于不包含架构边界测试的核心工作区；"
    "请关闭 BUILD_TESTING 和 MANUMESH_BUILD_TESTS，或使用模块化核心。"
  )
endif()

set(MANUMESH_INTERNAL_OBJECT_TARGETS)
set(MANUMESH_LIBRARY_OBJECTS)
if(NOT MANUMESH_MONOLITHIC_CORE)
  add_library(${MANUMESH_COMMON_OBJECT_TARGET} OBJECT
    ${MANUMESH_COMMON_SOURCES}
    ${MANUMESH_COMMON_PRIVATE_HEADERS}
  )
  add_library(${MANUMESH_MESH_EDIT_OBJECT_TARGET} OBJECT
    ${MANUMESH_MESH_EDIT_SOURCES}
    ${MANUMESH_MESH_EDIT_PRIVATE_HEADERS}
  )
  add_library(${MANUMESH_ANALYSIS_OBJECT_TARGET} OBJECT
    ${MANUMESH_ANALYSIS_SOURCES}
  )
  add_library(${MANUMESH_GEOMETRY_OBJECT_TARGET} OBJECT
    ${MANUMESH_GEOMETRY_SOURCES}
  )
  add_library(${MANUMESH_IO_OBJECT_TARGET} OBJECT
    ${MANUMESH_IO_SOURCES}
  )
  if(MANUMESH_ENABLE_DEBUG_UTIL)
    add_library(${MANUMESH_DEBUG_UTIL_OBJECT_TARGET} OBJECT
      ${MANUMESH_DEBUG_UTIL_SOURCES}
      ${MANUMESH_DEBUG_UTIL_PRIVATE_HEADERS}
    )
  endif()
  add_library(${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET} OBJECT
    ${MANUMESH_FEATURE_DETECTION_SOURCES}
    ${MANUMESH_MESHCORE_FEATURE_SOURCES}
    ${MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS}
    "${PROJECT_SOURCE_DIR}/src/meshcore/features/FeatureClassification.h"
  )
  add_library(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET} OBJECT
    ${MANUMESH_SIMPLIFICATION_SOURCES}
    ${MANUMESH_MESHCORE_SIMPLIFICATION_SOURCES}
    ${MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS}
    "${PROJECT_SOURCE_DIR}/src/meshcore/simplification/MeshCoreQem.h"
  )
  add_library(${MANUMESH_C_API_OBJECT_TARGET} OBJECT
    ${MANUMESH_API_SOURCES}
  )
  target_compile_definitions(${MANUMESH_C_API_OBJECT_TARGET}
    PRIVATE MANUMESH_C_API_IMPLEMENTATION
  )

  foreach(module IN ITEMS common mesh_edit analysis io feature_detection simplification c_api)
    string(TOUPPER "${module}" module_upper)
    set(module_target_variable "MANUMESH_${module_upper}_OBJECT_TARGET")
    manumesh_link_module_dependencies(
      ${${module_target_variable}}
      ${module}
      objects
      PRIVATE
    )
  endforeach()
  if(MANUMESH_ENABLE_DEBUG_UTIL)
    manumesh_link_module_dependencies(
      ${MANUMESH_DEBUG_UTIL_OBJECT_TARGET}
      debug_util
      objects
      PRIVATE
    )
  endif()

  set(MANUMESH_INTERNAL_OBJECT_TARGETS
    ${MANUMESH_COMMON_OBJECT_TARGET}
    ${MANUMESH_MESH_EDIT_OBJECT_TARGET}
    ${MANUMESH_ANALYSIS_OBJECT_TARGET}
    ${MANUMESH_GEOMETRY_OBJECT_TARGET}
    ${MANUMESH_IO_OBJECT_TARGET}
    ${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET}
    ${MANUMESH_SIMPLIFICATION_OBJECT_TARGET}
    ${MANUMESH_C_API_OBJECT_TARGET}
  )
  if(MANUMESH_ENABLE_DEBUG_UTIL)
    list(APPEND MANUMESH_INTERNAL_OBJECT_TARGETS
      ${MANUMESH_DEBUG_UTIL_OBJECT_TARGET}
    )
  endif()

  set(MANUMESH_LIBRARY_OBJECTS
    $<TARGET_OBJECTS:${MANUMESH_COMMON_OBJECT_TARGET}>
    $<TARGET_OBJECTS:${MANUMESH_MESH_EDIT_OBJECT_TARGET}>
    $<TARGET_OBJECTS:${MANUMESH_ANALYSIS_OBJECT_TARGET}>
    $<TARGET_OBJECTS:${MANUMESH_GEOMETRY_OBJECT_TARGET}>
    $<TARGET_OBJECTS:${MANUMESH_IO_OBJECT_TARGET}>
    $<TARGET_OBJECTS:${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET}>
    $<TARGET_OBJECTS:${MANUMESH_SIMPLIFICATION_OBJECT_TARGET}>
    $<TARGET_OBJECTS:${MANUMESH_C_API_OBJECT_TARGET}>
  )
  if(MANUMESH_ENABLE_DEBUG_UTIL)
    list(APPEND MANUMESH_LIBRARY_OBJECTS
      $<TARGET_OBJECTS:${MANUMESH_DEBUG_UTIL_OBJECT_TARGET}>
    )
  endif()
endif()

if(MANUMESH_MONOLITHIC_CORE)
  # Optional single-target mode for consumers that explicitly want one IDE
  # project. The normal and compact Visual Studio presets keep module OBJECT
  # targets so the project tree follows the source architecture.
  set(MANUMESH_CORE_SOURCES
    ${MANUMESH_LIBRARY_SOURCES}
    ${MANUMESH_PUBLIC_HEADERS}
    ${MANUMESH_PRIVATE_HEADERS}
  )
  set_source_files_properties(${MANUMESH_API_SOURCES}
    PROPERTIES COMPILE_DEFINITIONS MANUMESH_C_API_IMPLEMENTATION
  )
else()
  set(MANUMESH_CORE_SOURCES
    ${MANUMESH_LIBRARY_OBJECTS}
    ${MANUMESH_PUBLIC_HEADERS}
  )
endif()

if(MANUMESH_BUILD_SHARED_LIBRARY)
  add_library(${MANUMESH_CORE_TARGET} SHARED
    ${MANUMESH_CORE_SOURCES}
  )
else()
  add_library(${MANUMESH_CORE_TARGET} STATIC
    ${MANUMESH_CORE_SOURCES}
  )
endif()
if(NOT MANUMESH_CORE_TARGET STREQUAL "manumesh_core")
  add_library(manumesh_core ALIAS ${MANUMESH_CORE_TARGET})
endif()

# Keep the physical library free of C++ header usage requirements. The two
# installed interface targets below link the same binary while exposing only
# the requirements needed by their respective APIs.
add_library(manumesh_cpp_api INTERFACE)
add_library(ManuMesh::manumesh ALIAS manumesh_cpp_api)
set_target_properties(manumesh_cpp_api PROPERTIES EXPORT_NAME manumesh)

# C ABI-only consumers do not include the Eigen-backed C++ headers. Keep this
# target's usage requirements independent from the C++14 requirement of the
# full C++ SDK target while linking the same built library.
add_library(manumesh_c_api INTERFACE)
add_library(ManuMesh::c_api ALIAS manumesh_c_api)
set_target_properties(manumesh_c_api PROPERTIES EXPORT_NAME c_api)
target_include_directories(manumesh_c_api
  INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
if(MSVC)
  # Public headers are stored as UTF-8. Propagate the source encoding to
  # installed consumers as well as to in-tree targets; otherwise MSVC's
  # default system code page can turn harmless comments into parse errors.
  target_compile_options(manumesh_c_api
    INTERFACE
      $<$<COMPILE_LANGUAGE:C>:/utf-8>
      $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
  )
endif()
target_link_libraries(manumesh_c_api
  INTERFACE "$<LINK_ONLY:manumesh_core>"
)

target_compile_features(manumesh_cpp_api INTERFACE cxx_std_14)
target_include_directories(manumesh_cpp_api
  INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(manumesh_cpp_api
  INTERFACE manumesh_core
)

source_group(TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "公共头文件"
  FILES ${MANUMESH_PUBLIC_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
  PREFIX "源文件"
  FILES ${MANUMESH_LIBRARY_SOURCES}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/common/detail"
  PREFIX "公共模块私有头文件"
  FILES ${MANUMESH_COMMON_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/mesh_edit/detail"
  PREFIX "网格编辑私有头文件"
  FILES ${MANUMESH_MESH_EDIT_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/debugUtil"
  PREFIX "调试工具"
  FILES ${MANUMESH_DEBUG_UTIL_SOURCES} ${MANUMESH_DEBUG_UTIL_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail"
  PREFIX "特征检测私有头文件"
  FILES ${MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail"
  PREFIX "简化私有头文件"
  FILES ${MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS}
)
source_group(TREE "${PROJECT_SOURCE_DIR}/src/meshcore"
  PREFIX "MeshCore"
  FILES
    ${MANUMESH_MESHCORE_FEATURE_SOURCES}
    ${MANUMESH_MESHCORE_SIMPLIFICATION_SOURCES}
    ${MANUMESH_MESHCORE_PRIVATE_HEADERS}
)

foreach(manumesh_internal_target IN LISTS MANUMESH_INTERNAL_OBJECT_TARGETS)
  target_compile_features(${manumesh_internal_target} PRIVATE cxx_std_14)
  target_include_directories(${manumesh_internal_target}
    PRIVATE
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
      "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_compile_definitions(${manumesh_internal_target}
    PRIVATE MANUMESH_VERSION="${PROJECT_VERSION}"
  )
  if(MANUMESH_ENABLE_DEBUG_UTIL)
    target_compile_definitions(${manumesh_internal_target}
      PRIVATE $<$<CONFIG:Debug>:MANUMESH_ENABLE_DEBUG_UTIL>
    )
  endif()
  if(MANUMESH_BUILD_SHARED_LIBRARY)
    target_compile_definitions(${manumesh_internal_target}
      PRIVATE MANUMESH_BUILDING_DLL
    )
  endif()
  set_target_properties(${manumesh_internal_target}
    PROPERTIES POSITION_INDEPENDENT_CODE ON
  )
  manumesh_apply_warnings(${manumesh_internal_target})
endforeach()

if(MANUMESH_ENABLE_ONETBB)
  if(MANUMESH_MONOLITHIC_CORE)
    target_compile_definitions(${MANUMESH_CORE_TARGET} PRIVATE MANUMESH_HAS_ONETBB)
  else()
    target_compile_definitions(${MANUMESH_COMMON_OBJECT_TARGET} PRIVATE MANUMESH_HAS_ONETBB)
    target_link_libraries(${MANUMESH_COMMON_OBJECT_TARGET} PRIVATE TBB::tbb)
  endif()
endif()

if(MANUMESH_MONOLITHIC_CORE)
  target_include_directories(${MANUMESH_CORE_TARGET}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/simplification"
  )
else()
  target_include_directories(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/simplification"
  )
endif()

target_compile_features(${MANUMESH_CORE_TARGET} PRIVATE cxx_std_14)
target_include_directories(${MANUMESH_CORE_TARGET}
  PRIVATE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_target_properties(${MANUMESH_CORE_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MANUMESH_MONOLITHIC_CORE AND MANUMESH_ENABLE_DEBUG_UTIL)
  target_compile_definitions(${MANUMESH_CORE_TARGET}
    PRIVATE $<$<CONFIG:Debug>:MANUMESH_ENABLE_DEBUG_UTIL>
  )
endif()
if(MSVC)
  # Keep the installed C++ target consistent with the in-tree warning helper.
  target_compile_options(manumesh_cpp_api
    INTERFACE
      $<$<COMPILE_LANGUAGE:C>:/utf-8>
      $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
  )
endif()
if(TARGET Eigen3::Eigen)
  target_link_libraries(manumesh_cpp_api INTERFACE Eigen3::Eigen)
  if(MANUMESH_MONOLITHIC_CORE)
    target_link_libraries(${MANUMESH_CORE_TARGET} PRIVATE Eigen3::Eigen)
  else()
    foreach(manumesh_internal_target IN LISTS MANUMESH_INTERNAL_OBJECT_TARGETS)
      target_link_libraries(${manumesh_internal_target} PRIVATE Eigen3::Eigen)
    endforeach()
  endif()
elseif(DEFINED eigen_SOURCE_DIR)
  target_include_directories(manumesh_cpp_api INTERFACE "${eigen_SOURCE_DIR}")
  if(MANUMESH_MONOLITHIC_CORE)
    target_include_directories(${MANUMESH_CORE_TARGET} PRIVATE "${eigen_SOURCE_DIR}")
  else()
    foreach(manumesh_internal_target IN LISTS MANUMESH_INTERNAL_OBJECT_TARGETS)
      target_include_directories(${manumesh_internal_target} PRIVATE "${eigen_SOURCE_DIR}")
    endforeach()
  endif()
else()
  message(FATAL_ERROR
    "未找到 Eigen3。请确认已配置仓库内 Eigen，或提供可由 find_package(Eigen3) "
    "发现的系统 Eigen。"
  )
endif()

if(MANUMESH_RESOLVED_EIGEN_PROVIDER STREQUAL "vendored")
  target_include_directories(manumesh_cpp_api
    INTERFACE
      "$<BUILD_INTERFACE:${MANUMESH_EIGEN_SOURCE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_DATADIR}/manumesh/thirdParty/eigen/include>"
  )
  if(MANUMESH_MONOLITHIC_CORE)
    target_include_directories(${MANUMESH_CORE_TARGET}
      PRIVATE "$<BUILD_INTERFACE:${MANUMESH_EIGEN_SOURCE_DIR}>"
    )
  else()
    foreach(manumesh_internal_target IN LISTS MANUMESH_INTERNAL_OBJECT_TARGETS)
      target_include_directories(${manumesh_internal_target}
        PRIVATE "$<BUILD_INTERFACE:${MANUMESH_EIGEN_SOURCE_DIR}>"
      )
    endforeach()
  endif()
endif()

target_compile_definitions(${MANUMESH_CORE_TARGET}
  PRIVATE MANUMESH_VERSION="${PROJECT_VERSION}"
)

if(MANUMESH_ENABLE_ONETBB)
  # oneTBB remains an implementation dependency: public headers contain no
  # TBB types. For a static archive CMake exports this PRIVATE link edge as
  # LINK_ONLY on manumesh_binary, which is exactly the SDK consumer contract.
  target_link_libraries(${MANUMESH_CORE_TARGET} PRIVATE TBB::tbb)
endif()

# Non-installed module archives give architecture tests real link boundaries
# while reusing the production object files. A link-check executable adds the
# root module objects directly, so all of their unresolved symbols must be
# provided by the dependency modules declared here.
if(BUILD_TESTING AND MANUMESH_BUILD_TESTS)
  function(manumesh_add_boundary_module module)
    string(TOUPPER "${module}" module_upper)
    set(module_target_variable "MANUMESH_${module_upper}_OBJECT_TARGET")
    set(module_object_target "${${module_target_variable}}")
    add_library(manumesh_${module}_boundary_module STATIC
      $<TARGET_OBJECTS:${module_object_target}>
    )
    set_target_properties(manumesh_${module}_boundary_module PROPERTIES
      EXCLUDE_FROM_ALL TRUE
      FOLDER "architecture/module-boundaries"
    )
  endfunction()

  manumesh_add_boundary_module(geometry)
  manumesh_add_boundary_module(common)
  manumesh_add_boundary_module(mesh_edit)
  manumesh_add_boundary_module(analysis)
  manumesh_add_boundary_module(io)
  if(MANUMESH_ENABLE_DEBUG_UTIL)
    manumesh_add_boundary_module(debug_util)
  endif()
  manumesh_add_boundary_module(feature_detection)
  manumesh_add_boundary_module(simplification)

  foreach(module IN ITEMS common mesh_edit analysis io feature_detection simplification)
    manumesh_link_module_dependencies(
      manumesh_${module}_boundary_module
      ${module}
      boundary_module
      PUBLIC
    )
  endforeach()
  if(MANUMESH_ENABLE_DEBUG_UTIL)
    manumesh_link_module_dependencies(
      manumesh_debug_util_boundary_module
      debug_util
      boundary_module
      PUBLIC
    )
  endif()
  if(MANUMESH_ENABLE_ONETBB)
    # Feature/simplification architecture checks pull common objects through
    # this archive, so retain the backend link edge in their test graph.
    target_link_libraries(manumesh_common_boundary_module PUBLIC TBB::tbb)
  endif()
endif()

# 供白盒测试使用的内部 STATIC 聚合库。生产或纯 SDK 构建不创建此目标。
if(BUILD_TESTING AND MANUMESH_BUILD_TESTS)
  # 复用上面的对象库，每个源文件仍只编译一次；测试链接内部符号的唯一副本。
  add_library(manumesh_internal STATIC ${MANUMESH_LIBRARY_OBJECTS})
  add_library(ManuMesh::internal ALIAS manumesh_internal)
  target_compile_features(manumesh_internal PUBLIC cxx_std_14)
  target_include_directories(manumesh_internal
    PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  )
  if(TARGET Eigen3::Eigen)
    target_link_libraries(manumesh_internal PUBLIC Eigen3::Eigen)
  elseif(DEFINED eigen_SOURCE_DIR)
    target_include_directories(manumesh_internal PUBLIC "${eigen_SOURCE_DIR}")
  endif()
  if(MANUMESH_RESOLVED_EIGEN_PROVIDER STREQUAL "vendored")
    target_include_directories(manumesh_internal
      PUBLIC "$<BUILD_INTERFACE:${MANUMESH_EIGEN_SOURCE_DIR}>"
    )
  endif()
  if(MANUMESH_ENABLE_ONETBB)
    target_link_libraries(manumesh_internal PRIVATE TBB::tbb)
  endif()
endif()

if(MANUMESH_BUILD_SHARED_LIBRARY)
  target_compile_definitions(${MANUMESH_CORE_TARGET} PRIVATE MANUMESH_BUILDING_DLL)
  if(WIN32)
    target_compile_definitions(manumesh_cpp_api INTERFACE MANUMESH_USING_DLL)
    target_compile_definitions(manumesh_c_api INTERFACE MANUMESH_USING_DLL)
  endif()
endif()

set_target_properties(${MANUMESH_CORE_TARGET} PROPERTIES
  OUTPUT_NAME manumesh
  EXPORT_NAME manumesh_binary
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

manumesh_apply_warnings(${MANUMESH_CORE_TARGET})

if(MANUMESH_ENABLE_INSTALL)
  install(TARGETS ${MANUMESH_CORE_TARGET}
    EXPORT ManuMeshTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )
  install(TARGETS manumesh_cpp_api
    EXPORT ManuMeshTargets
  )
  install(TARGETS manumesh_c_api
    EXPORT ManuMeshTargets
  )

  install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
  )

  if(MANUMESH_RESOLVED_EIGEN_PROVIDER STREQUAL "vendored")
    install(DIRECTORY "${MANUMESH_EIGEN_SOURCE_DIR}/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/manumesh/thirdParty/eigen/include"
      FILES_MATCHING PATTERN "*"
    )
  endif()
  if(MANUMESH_ENABLE_ONETBB AND MANUMESH_ONETBB_RUNTIME_DLL AND
     (MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "system" OR
      MANUMESH_RESOLVED_ONETBB_PROVIDER STREQUAL "external"))
    install(FILES "$<TARGET_FILE:TBB::tbb>"
      DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
  endif()
endif()
