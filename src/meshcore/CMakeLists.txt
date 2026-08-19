set(MANUMESH_MESHCORE_CORE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/core/Mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/MeshTopology.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/Status.cpp"
)
set(MANUMESH_MESHCORE_CORE_HEADERS
  "${PROJECT_SOURCE_DIR}/include/Export.h"
  "${PROJECT_SOURCE_DIR}/include/core/ExecutionOptions.h"
  "${PROJECT_SOURCE_DIR}/include/core/Handles.h"
  "${PROJECT_SOURCE_DIR}/include/core/MathUtils.h"
  "${PROJECT_SOURCE_DIR}/include/core/Mesh.h"
  "${PROJECT_SOURCE_DIR}/include/core/MeshTopology.h"
  "${PROJECT_SOURCE_DIR}/include/core/Status.h"
  "${PROJECT_SOURCE_DIR}/include/core/Tolerances.h"
)

set(MANUMESH_MESHCORE_PUBLIC_HEADERS
  ${MANUMESH_MESHCORE_CORE_HEADERS}
  "${PROJECT_SOURCE_DIR}/include/algorithms/analysis/MeshAnalysis.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureAnalysisViews.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureDetector.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureOptions.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureTypes.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/MeshCoreQem.h"
  "${PROJECT_SOURCE_DIR}/include/io/MeshIo.h"
)

set(MANUMESH_MESHCORE_COMMON_SOURCES
  "${PROJECT_SOURCE_DIR}/src/common/GeometryPredicates.cpp"
  "${PROJECT_SOURCE_DIR}/src/common/MeshDistanceIndex.cpp"
  "${PROJECT_SOURCE_DIR}/src/common/MeshQueries.cpp"
  "${PROJECT_SOURCE_DIR}/src/common/ParallelExecution.cpp"
  "${PROJECT_SOURCE_DIR}/src/common/SpatialIndex.cpp"
)
set(MANUMESH_MESHCORE_COMMON_HEADERS
  "${PROJECT_SOURCE_DIR}/src/common/detail/GeometryPredicates.h"
  "${PROJECT_SOURCE_DIR}/src/common/detail/MathConstants.h"
  "${PROJECT_SOURCE_DIR}/src/common/detail/MeshDistanceIndex.h"
  "${PROJECT_SOURCE_DIR}/src/common/detail/MeshQueries.h"
  "${PROJECT_SOURCE_DIR}/src/common/detail/ParallelExecution.h"
  "${PROJECT_SOURCE_DIR}/src/common/detail/SpatialIndex.h"
)

set(MANUMESH_MESHCORE_MESH_EDIT_SOURCES
  "${PROJECT_SOURCE_DIR}/src/mesh_edit/DynamicTopology.cpp"
  "${PROJECT_SOURCE_DIR}/src/mesh_edit/MeshCompaction.cpp"
)
set(MANUMESH_MESHCORE_MESH_EDIT_HEADERS
  "${PROJECT_SOURCE_DIR}/src/mesh_edit/detail/DynamicTopology.h"
  "${PROJECT_SOURCE_DIR}/src/mesh_edit/detail/MeshCompaction.h"
  "${PROJECT_SOURCE_DIR}/src/mesh_edit/detail/MeshEditTypes.h"
)

set(MANUMESH_MESHCORE_IO_SOURCES
  "${PROJECT_SOURCE_DIR}/src/io/MeshIo.cpp"
)
set(MANUMESH_MESHCORE_IO_HEADERS
  "${PROJECT_SOURCE_DIR}/include/io/MeshIo.h"
)

set(MANUMESH_MESHCORE_ANALYSIS_SOURCES
  "${PROJECT_SOURCE_DIR}/src/analysis/MeshAnalysis.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analysis/MeshStatistics.cpp"
)
set(MANUMESH_MESHCORE_ANALYSIS_HEADERS
  "${PROJECT_SOURCE_DIR}/include/algorithms/analysis/MeshAnalysis.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/analysis/MeshStatistics.h"
)

# Keep the production detector and its public diagnostics. Benchmarking is a
# small, linkable API implementation rather than a test or command-line tool.
set(MANUMESH_MESHCORE_FEATURE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureAnalysisValidation.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureBenchmark.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureCircularRecovery.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureCycleRecovery.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureDetector.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureEvidence.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureGraph.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureGraphCleanup.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureGraphCompatibility.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureGraphConsolidation.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureLoopBuilder.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureLoopRecovery.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureNormalFilter.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeaturePrimitiveRecovery.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureSegmentation.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/FeatureTraceRecovery.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/NormalTensor.cpp"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/PrimitiveFit.cpp"
)
set(MANUMESH_MESHCORE_FEATURE_CLASSIFICATION_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/features/FeatureClassification.cpp"
)
set(MANUMESH_MESHCORE_FEATURE_PRIVATE_HEADERS
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureCircularRecovery.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureCycleRecovery.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureDebugInstrumentation.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureDetectionCache.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureDetectionTypes.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureEvidence.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureGraph.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureGraphCleanup.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureGraphCompatibility.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureGraphConsolidation.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureInputValidation.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureLoopBuilder.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureLoopRecovery.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureNormalFilter.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeaturePrimitiveRecovery.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureSegmentation.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/FeatureTraceRecovery.h"
  "${PROJECT_SOURCE_DIR}/src/feature_detection/detail/PrimitiveFit.h"
)
set(MANUMESH_MESHCORE_FEATURE_CLASSIFICATION_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/features/FeatureClassification.h"
)

set(MANUMESH_MESHCORE_SIMPLIFICATION_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/MeshCoreQem.cpp"
)
set(MANUMESH_MESHCORE_SIMPLIFICATION_HEADERS
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/MeshCoreQem.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/MeshCoreQem.h"
)

function(manumesh_configure_meshcore_target target)
  target_compile_features(${target} PUBLIC cxx_std_14)
  target_include_directories(${target}
    PUBLIC "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
  )
  target_compile_definitions(${target}
    PRIVATE
      MANUMESH_BUILDING_DLL
      MANUMESH_VERSION="${PROJECT_VERSION}"
  )
  manumesh_apply_warnings(${target})
endfunction()

add_library(${MANUMESH_COMMON_OBJECT_TARGET} OBJECT
  ${MANUMESH_MESHCORE_COMMON_SOURCES}
  ${MANUMESH_MESHCORE_COMMON_HEADERS}
)
manumesh_configure_meshcore_target(${MANUMESH_COMMON_OBJECT_TARGET})
target_link_libraries(${MANUMESH_COMMON_OBJECT_TARGET} PRIVATE Eigen3::Eigen)

add_library(${MANUMESH_MESH_EDIT_OBJECT_TARGET} OBJECT
  ${MANUMESH_MESHCORE_MESH_EDIT_SOURCES}
  ${MANUMESH_MESHCORE_MESH_EDIT_HEADERS}
)
manumesh_configure_meshcore_target(${MANUMESH_MESH_EDIT_OBJECT_TARGET})
target_link_libraries(${MANUMESH_MESH_EDIT_OBJECT_TARGET} PRIVATE Eigen3::Eigen)

add_library(${MANUMESH_IO_OBJECT_TARGET} OBJECT
  ${MANUMESH_MESHCORE_IO_SOURCES}
  ${MANUMESH_MESHCORE_IO_HEADERS}
)
manumesh_configure_meshcore_target(${MANUMESH_IO_OBJECT_TARGET})
target_link_libraries(${MANUMESH_IO_OBJECT_TARGET} PRIVATE Eigen3::Eigen)

add_library(${MANUMESH_ANALYSIS_OBJECT_TARGET} OBJECT
  ${MANUMESH_MESHCORE_ANALYSIS_SOURCES}
  ${MANUMESH_MESHCORE_ANALYSIS_HEADERS}
)
manumesh_configure_meshcore_target(${MANUMESH_ANALYSIS_OBJECT_TARGET})
target_link_libraries(${MANUMESH_ANALYSIS_OBJECT_TARGET} PRIVATE Eigen3::Eigen)

add_library(${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET} OBJECT
  ${MANUMESH_MESHCORE_FEATURE_SOURCES}
  ${MANUMESH_MESHCORE_FEATURE_CLASSIFICATION_SOURCES}
  ${MANUMESH_MESHCORE_PUBLIC_HEADERS}
  ${MANUMESH_MESHCORE_FEATURE_PRIVATE_HEADERS}
  ${MANUMESH_MESHCORE_FEATURE_CLASSIFICATION_HEADERS}
)
manumesh_configure_meshcore_target(${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET})
target_link_libraries(${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET} PRIVATE Eigen3::Eigen)
if(MSVC)
  set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/src/feature_detection/PrimitiveFit.cpp"
    PROPERTIES COMPILE_OPTIONS "/bigobj"
  )
endif()

add_library(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET} OBJECT
  ${MANUMESH_MESHCORE_SIMPLIFICATION_SOURCES}
  ${MANUMESH_MESHCORE_SIMPLIFICATION_HEADERS}
)
manumesh_configure_meshcore_target(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET})
target_link_libraries(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET} PRIVATE Eigen3::Eigen)

# Core is the only linkable target. Functional modules remain visible as
# object-library projects so the solution follows the repository structure.
add_library(${MANUMESH_CORE_TARGET} SHARED
  ${MANUMESH_MESHCORE_CORE_SOURCES}
  ${MANUMESH_MESHCORE_PUBLIC_HEADERS}
  $<TARGET_OBJECTS:${MANUMESH_COMMON_OBJECT_TARGET}>
  $<TARGET_OBJECTS:${MANUMESH_MESH_EDIT_OBJECT_TARGET}>
  $<TARGET_OBJECTS:${MANUMESH_IO_OBJECT_TARGET}>
  $<TARGET_OBJECTS:${MANUMESH_ANALYSIS_OBJECT_TARGET}>
  $<TARGET_OBJECTS:${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET}>
  $<TARGET_OBJECTS:${MANUMESH_SIMPLIFICATION_OBJECT_TARGET}>
)
manumesh_configure_meshcore_target(${MANUMESH_CORE_TARGET})
target_link_libraries(${MANUMESH_CORE_TARGET} PUBLIC Eigen3::Eigen)
set_target_properties(${MANUMESH_CORE_TARGET} PROPERTIES OUTPUT_NAME MeshCore)

source_group(TREE "${PROJECT_SOURCE_DIR}/include" PREFIX "Public API" FILES
  ${MANUMESH_MESHCORE_PUBLIC_HEADERS}
)
source_group(TREE "${PROJECT_SOURCE_DIR}/src/core" PREFIX "Core" FILES
  ${MANUMESH_MESHCORE_CORE_SOURCES}
)
source_group(TREE "${PROJECT_SOURCE_DIR}/src/common" PREFIX "Common" FILES
  ${MANUMESH_MESHCORE_COMMON_SOURCES}
  ${MANUMESH_MESHCORE_COMMON_HEADERS}
)
source_group(TREE "${PROJECT_SOURCE_DIR}/src/mesh_edit" PREFIX "Mesh Edit" FILES
  ${MANUMESH_MESHCORE_MESH_EDIT_SOURCES}
  ${MANUMESH_MESHCORE_MESH_EDIT_HEADERS}
)
source_group(TREE "${PROJECT_SOURCE_DIR}/src/io" PREFIX "IO" FILES
  ${MANUMESH_MESHCORE_IO_SOURCES}
)
source_group(TREE "${PROJECT_SOURCE_DIR}/src/analysis" PREFIX "Analysis" FILES
  "${PROJECT_SOURCE_DIR}/src/analysis/MeshAnalysis.cpp"
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/analysis" PREFIX "Analysis" FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/analysis/MeshStatistics.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analysis/MeshStatistics.h"
)
source_group(TREE "${PROJECT_SOURCE_DIR}/src/feature_detection" PREFIX "Feature Detection" FILES
  ${MANUMESH_MESHCORE_FEATURE_SOURCES}
  ${MANUMESH_MESHCORE_FEATURE_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/features" PREFIX "Feature Detection" FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/features/FeatureClassification.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/features/FeatureClassification.h"
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/simplification" PREFIX "Simplification" FILES
  ${MANUMESH_MESHCORE_SIMPLIFICATION_SOURCES}
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/MeshCoreQem.h"
)
