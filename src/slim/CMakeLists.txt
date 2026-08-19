set(MANUMESH_SLIM_CORE_SOURCES
  "${PROJECT_SOURCE_DIR}/src/core/Mesh.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/MeshTopology.cpp"
  "${PROJECT_SOURCE_DIR}/src/core/Status.cpp"
)

add_library(${MANUMESH_CORE_TARGET} STATIC ${MANUMESH_SLIM_CORE_SOURCES})
target_compile_features(${MANUMESH_CORE_TARGET} PUBLIC cxx_std_14)
target_include_directories(${MANUMESH_CORE_TARGET}
  PUBLIC "${PROJECT_SOURCE_DIR}/include"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(${MANUMESH_CORE_TARGET} PUBLIC Eigen3::Eigen)
target_compile_definitions(${MANUMESH_CORE_TARGET} PRIVATE MANUMESH_VERSION="${PROJECT_VERSION}")
manumesh_apply_warnings(${MANUMESH_CORE_TARGET})

add_library(${MANUMESH_IO_OBJECT_TARGET} STATIC
  "${PROJECT_SOURCE_DIR}/src/io/MeshIo.cpp"
)
target_compile_features(${MANUMESH_IO_OBJECT_TARGET} PUBLIC cxx_std_14)
target_include_directories(${MANUMESH_IO_OBJECT_TARGET}
  PUBLIC "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(${MANUMESH_IO_OBJECT_TARGET} PUBLIC ${MANUMESH_CORE_TARGET})
target_compile_definitions(${MANUMESH_IO_OBJECT_TARGET} PRIVATE MANUMESH_VERSION="${PROJECT_VERSION}")
manumesh_apply_warnings(${MANUMESH_IO_OBJECT_TARGET})

add_library(${MANUMESH_ANALYSIS_OBJECT_TARGET} STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/analysis/SlimMeshAnalysis.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/analysis/SlimMeshAnalysis.h"
)
target_compile_features(${MANUMESH_ANALYSIS_OBJECT_TARGET} PUBLIC cxx_std_14)
target_include_directories(${MANUMESH_ANALYSIS_OBJECT_TARGET}
  PUBLIC "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(${MANUMESH_ANALYSIS_OBJECT_TARGET} PUBLIC ${MANUMESH_CORE_TARGET})
manumesh_apply_warnings(${MANUMESH_ANALYSIS_OBJECT_TARGET})

add_library(${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET} STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/features/SlimFeatureDetection.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/features/SlimFeatureDetection.h"
)
target_compile_features(${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET} PUBLIC cxx_std_14)
target_include_directories(${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET}
  PUBLIC "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET} PUBLIC ${MANUMESH_CORE_TARGET})
manumesh_apply_warnings(${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET})

add_library(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET} STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SlimQem.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/simplification/SlimQem.h"
)
target_compile_features(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET} PUBLIC cxx_std_14)
target_include_directories(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET}
  PUBLIC "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET}
  PUBLIC
    ${MANUMESH_CORE_TARGET}
    ${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET}
)
manumesh_apply_warnings(${MANUMESH_SIMPLIFICATION_OBJECT_TARGET})

source_group(TREE "${PROJECT_SOURCE_DIR}/src/core" PREFIX "Core" FILES ${MANUMESH_SLIM_CORE_SOURCES})
