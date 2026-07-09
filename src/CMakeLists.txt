set(MANUMESH_PUBLIC_HEADERS
  "${PROJECT_SOURCE_DIR}/include/Export.h"
  "${PROJECT_SOURCE_DIR}/include/api/CApi.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureDetector.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/feature_detection/FeatureTypes.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/Metrics.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/PlainSimplifier.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/QEMSimplifier.h"
  "${PROJECT_SOURCE_DIR}/include/algorithms/simplification/SimplificationTypes.h"
  "${PROJECT_SOURCE_DIR}/include/core/Handles.h"
  "${PROJECT_SOURCE_DIR}/include/core/Mesh.h"
  "${PROJECT_SOURCE_DIR}/include/core/MeshGenerators.h"
  "${PROJECT_SOURCE_DIR}/include/core/MeshTopology.h"
  "${PROJECT_SOURCE_DIR}/include/core/PlainMesh.h"
  "${PROJECT_SOURCE_DIR}/include/core/Status.h"
)

set(MANUMESH_LIBRARY_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/api/CApi.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/MeshQueries.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/Mesh.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/MeshGenerators.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/MeshTopology.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/PlainMesh.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/core/Status.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureCircularRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureCycleRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureDetector.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureEvidence.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureGraph.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureGraphCleanup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureLoopBuilder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureLoopRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeaturePrimitiveRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/FeatureTraceRecovery.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/NormalTensor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/PrimitiveFit.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CandidateQueue.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CollapseAttempt.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CollapseLegality.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/DynamicTopology.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/FeatureConstraints.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/GeometryPredicates.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/Metrics.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/QEMSimplifier.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/Quadrics.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/ResultBuilder.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SimplificationPolicies.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SimplificationRun.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SimplificationValidation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SpatialFaceIndex.cpp"
)

set(MANUMESH_COMMON_PRIVATE_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/MathConstants.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/MeshQueries.h"
)

set(MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureCircularRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureCycleRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureDetectionTypes.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureEvidence.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureGraph.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureGraphCleanup.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureLoopBuilder.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureLoopRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeaturePrimitiveRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/FeatureTraceRecovery.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail/PrimitiveFit.h"
)

set(MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/CandidateQueue.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/CollapseAttempt.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/CollapseLegality.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/DynamicTopology.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/FeatureConstraints.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/GeometryPredicates.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/Quadrics.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/ResultBuilder.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationPolicies.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationRun.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationTypes.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationValidation.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SpatialFaceIndex.h"
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
source_group(TREE "${PROJECT_SOURCE_DIR}/include"
  PREFIX "Public Headers"
  FILES ${MANUMESH_PUBLIC_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
  PREFIX "Source"
  FILES ${MANUMESH_LIBRARY_SOURCES}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/common/detail"
  PREFIX "Common Private Headers"
  FILES ${MANUMESH_COMMON_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail"
  PREFIX "Feature Detection Private Headers"
  FILES ${MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail"
  PREFIX "Simplification Private Headers"
  FILES ${MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS}
)

target_compile_features(manumesh_core PUBLIC cxx_std_17)
target_include_directories(manumesh_core
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/simplification"
)

if(TARGET Eigen3::Eigen)
  target_link_libraries(manumesh_core PUBLIC Eigen3::Eigen)
elseif(DEFINED eigen_SOURCE_DIR)
  target_include_directories(manumesh_core PUBLIC "${eigen_SOURCE_DIR}")
else()
  message(FATAL_ERROR
    "Eigen3 was not found and FetchContent did not provide eigen_SOURCE_DIR."
  )
endif()

if(EXISTS "${MANUMESH_EIGEN_SOURCE_DIR}/Eigen/Core")
  target_include_directories(manumesh_core
    PUBLIC
      "$<BUILD_INTERFACE:${MANUMESH_EIGEN_SOURCE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_DATADIR}/manumesh/thirdParty/eigen/include>"
  )
endif()

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

manumesh_apply_warnings(manumesh_core)

if(MANUMESH_ENABLE_INSTALL)
  install(TARGETS manumesh_core
    EXPORT ManuMeshTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  )

  install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
  )

  if(EXISTS "${MANUMESH_EIGEN_SOURCE_DIR}/Eigen/Core")
    install(DIRECTORY "${MANUMESH_EIGEN_SOURCE_DIR}/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/manumesh/thirdParty/eigen/include"
      FILES_MATCHING PATTERN "*"
    )
  endif()
endif()
