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
  "${PROJECT_SOURCE_DIR}/include/io/MeshIo.h"
)

set(MANUMESH_API_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/api/CApiConverters.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/api/CApi.cpp"
)

set(MANUMESH_COMMON_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/common/GeometryPredicates.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/MeshDistanceIndex.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/MeshQueries.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/SpatialIndex.cpp"
)

set(MANUMESH_MESH_EDIT_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/mesh_edit/DynamicTopology.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/mesh_edit/MeshCompaction.cpp"
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
)

set(MANUMESH_SIMPLIFICATION_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CandidateQueue.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CollapseAttempt.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CollapseLegality.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/CollapseTopology.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/FeatureConstraints.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/FeatureGuidance.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/Metrics.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/QEMSimplifier.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/Quadrics.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/QualityRefinement.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SimplificationPolicies.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SimplificationRun.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SimplificationValidation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SpatialFaceIndex.cpp"
)

set(MANUMESH_LIBRARY_SOURCES
  ${MANUMESH_API_SOURCES}
  ${MANUMESH_COMMON_SOURCES}
  ${MANUMESH_MESH_EDIT_SOURCES}
  ${MANUMESH_GEOMETRY_SOURCES}
  ${MANUMESH_IO_SOURCES}
  ${MANUMESH_DEBUG_UTIL_SOURCES}
  ${MANUMESH_FEATURE_DETECTION_SOURCES}
  ${MANUMESH_SIMPLIFICATION_SOURCES}
)

set(MANUMESH_COMMON_PRIVATE_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/GeometryPredicates.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/MathConstants.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/MeshDistanceIndex.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/detail/MeshQueries.h"
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
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/CollapseTopology.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/FeatureConstraints.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/FeatureGuidance.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/Quadrics.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/QualityRefinement.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationPolicies.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationRun.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationTypes.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SimplificationValidation.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail/SpatialFaceIndex.h"
)

set(MANUMESH_PRIVATE_HEADERS
  ${MANUMESH_COMMON_PRIVATE_HEADERS}
  ${MANUMESH_MESH_EDIT_PRIVATE_HEADERS}
  ${MANUMESH_DEBUG_UTIL_PRIVATE_HEADERS}
  ${MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS}
  ${MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS}
)

add_library(manumesh_common_objects OBJECT
  ${MANUMESH_COMMON_SOURCES}
  ${MANUMESH_COMMON_PRIVATE_HEADERS}
)
add_library(manumesh_mesh_edit_objects OBJECT
  ${MANUMESH_MESH_EDIT_SOURCES}
  ${MANUMESH_MESH_EDIT_PRIVATE_HEADERS}
)
add_library(manumesh_geometry_objects OBJECT
  ${MANUMESH_GEOMETRY_SOURCES}
)
add_library(manumesh_io_objects OBJECT
  ${MANUMESH_IO_SOURCES}
)
if(MANUMESH_ENABLE_DEBUG_UTIL)
  add_library(manumesh_debug_util_objects OBJECT
    ${MANUMESH_DEBUG_UTIL_SOURCES}
    ${MANUMESH_DEBUG_UTIL_PRIVATE_HEADERS}
  )
endif()
add_library(manumesh_feature_detection_objects OBJECT
  ${MANUMESH_FEATURE_DETECTION_SOURCES}
  ${MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS}
)
add_library(manumesh_simplification_objects OBJECT
  ${MANUMESH_SIMPLIFICATION_SOURCES}
  ${MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS}
)
add_library(manumesh_c_api_objects OBJECT
  ${MANUMESH_API_SOURCES}
)

target_link_libraries(manumesh_common_objects
  PRIVATE manumesh_geometry_objects
)
target_link_libraries(manumesh_mesh_edit_objects
  PRIVATE
    manumesh_common_objects
    manumesh_geometry_objects
)
target_link_libraries(manumesh_io_objects
  PRIVATE manumesh_geometry_objects
)
target_link_libraries(manumesh_feature_detection_objects
  PRIVATE
    manumesh_common_objects
    manumesh_geometry_objects
)
target_link_libraries(manumesh_simplification_objects
  PRIVATE
    manumesh_common_objects
    manumesh_geometry_objects
    manumesh_feature_detection_objects
    manumesh_mesh_edit_objects
)
target_link_libraries(manumesh_c_api_objects
  PRIVATE
    manumesh_geometry_objects
    manumesh_io_objects
    manumesh_simplification_objects
)
if(MANUMESH_ENABLE_DEBUG_UTIL)
  target_link_libraries(manumesh_debug_util_objects
    PRIVATE
      manumesh_geometry_objects
      manumesh_feature_detection_objects
  )
endif()

set(MANUMESH_INTERNAL_OBJECT_TARGETS
  manumesh_common_objects
  manumesh_mesh_edit_objects
  manumesh_geometry_objects
  manumesh_io_objects
  manumesh_feature_detection_objects
  manumesh_simplification_objects
  manumesh_c_api_objects
)
if(MANUMESH_ENABLE_DEBUG_UTIL)
  list(APPEND MANUMESH_INTERNAL_OBJECT_TARGETS manumesh_debug_util_objects)
endif()

set(MANUMESH_LIBRARY_OBJECTS
  $<TARGET_OBJECTS:manumesh_common_objects>
  $<TARGET_OBJECTS:manumesh_mesh_edit_objects>
  $<TARGET_OBJECTS:manumesh_geometry_objects>
  $<TARGET_OBJECTS:manumesh_io_objects>
  $<TARGET_OBJECTS:manumesh_feature_detection_objects>
  $<TARGET_OBJECTS:manumesh_simplification_objects>
  $<TARGET_OBJECTS:manumesh_c_api_objects>
)
if(MANUMESH_ENABLE_DEBUG_UTIL)
  list(APPEND MANUMESH_LIBRARY_OBJECTS
    $<TARGET_OBJECTS:manumesh_debug_util_objects>
  )
endif()

if(MANUMESH_BUILD_SHARED_LIBRARY)
  add_library(manumesh_core SHARED
    ${MANUMESH_LIBRARY_OBJECTS}
    ${MANUMESH_PUBLIC_HEADERS}
  )
else()
  add_library(manumesh_core STATIC
    ${MANUMESH_LIBRARY_OBJECTS}
    ${MANUMESH_PUBLIC_HEADERS}
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
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/mesh_edit/detail"
  PREFIX "Mesh Edit Private Headers"
  FILES ${MANUMESH_MESH_EDIT_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/debugUtil"
  PREFIX "Debug Util"
  FILES ${MANUMESH_DEBUG_UTIL_SOURCES} ${MANUMESH_DEBUG_UTIL_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/feature_detection/detail"
  PREFIX "Feature Detection Private Headers"
  FILES ${MANUMESH_FEATURE_DETECTION_PRIVATE_HEADERS}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/simplification/detail"
  PREFIX "Simplification Private Headers"
  FILES ${MANUMESH_SIMPLIFICATION_PRIVATE_HEADERS}
)

foreach(manumesh_internal_target IN LISTS MANUMESH_INTERNAL_OBJECT_TARGETS)
  target_compile_features(${manumesh_internal_target} PRIVATE cxx_std_17)
  target_include_directories(${manumesh_internal_target}
    PRIVATE
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
      "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_compile_definitions(${manumesh_internal_target}
    PRIVATE MANUMESH_VERSION="${PROJECT_VERSION}"
  )
  if(MSVC)
    target_compile_options(${manumesh_internal_target}
      PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/FIdebugUtil/debugUtil.h>
    )
  else()
    target_compile_options(${manumesh_internal_target}
      PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-include>
        $<$<COMPILE_LANGUAGE:CXX>:debugUtil/debugUtil.h>
    )
  endif()
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

target_include_directories(manumesh_simplification_objects
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/simplification"
)

target_compile_features(manumesh_core PUBLIC cxx_std_17)
target_include_directories(manumesh_core
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(TARGET Eigen3::Eigen)
  target_link_libraries(manumesh_core PUBLIC Eigen3::Eigen)
  foreach(manumesh_internal_target IN LISTS MANUMESH_INTERNAL_OBJECT_TARGETS)
    target_link_libraries(${manumesh_internal_target} PRIVATE Eigen3::Eigen)
  endforeach()
elseif(DEFINED eigen_SOURCE_DIR)
  target_include_directories(manumesh_core PUBLIC "${eigen_SOURCE_DIR}")
  foreach(manumesh_internal_target IN LISTS MANUMESH_INTERNAL_OBJECT_TARGETS)
    target_include_directories(${manumesh_internal_target} PRIVATE "${eigen_SOURCE_DIR}")
  endforeach()
else()
  message(FATAL_ERROR
    "Eigen3 was not found and FetchContent did not provide eigen_SOURCE_DIR."
  )
endif()

if(MANUMESH_RESOLVED_EIGEN_PROVIDER STREQUAL "vendored")
  target_include_directories(manumesh_core
    PUBLIC
      "$<BUILD_INTERFACE:${MANUMESH_EIGEN_SOURCE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_DATADIR}/manumesh/thirdParty/eigen/include>"
  )
  foreach(manumesh_internal_target IN LISTS MANUMESH_INTERNAL_OBJECT_TARGETS)
    target_include_directories(${manumesh_internal_target}
      PRIVATE "$<BUILD_INTERFACE:${MANUMESH_EIGEN_SOURCE_DIR}>"
    )
  endforeach()
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

  if(MANUMESH_RESOLVED_EIGEN_PROVIDER STREQUAL "vendored")
    install(DIRECTORY "${MANUMESH_EIGEN_SOURCE_DIR}/"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/manumesh/thirdParty/eigen/include"
      FILES_MATCHING PATTERN "*"
    )
  endif()
endif()
