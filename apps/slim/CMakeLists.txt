add_executable(${MANUMESH_CLI_TARGET}
  main.cpp
  SlimCli.cpp
  SlimCli.h
)
set_target_properties(${MANUMESH_CLI_TARGET} PROPERTIES OUTPUT_NAME mesh-slim)
target_compile_features(${MANUMESH_CLI_TARGET} PRIVATE cxx_std_14)
target_include_directories(${MANUMESH_CLI_TARGET} PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(${MANUMESH_CLI_TARGET} PRIVATE MANUMESH_VERSION="${PROJECT_VERSION}")
target_link_libraries(${MANUMESH_CLI_TARGET}
  PRIVATE
    ${MANUMESH_IO_OBJECT_TARGET}
    ${MANUMESH_ANALYSIS_OBJECT_TARGET}
    ${MANUMESH_FEATURE_DETECTION_OBJECT_TARGET}
    ${MANUMESH_SIMPLIFICATION_OBJECT_TARGET}
)
if(WIN32)
  target_link_libraries(${MANUMESH_CLI_TARGET} PRIVATE shell32)
endif()
manumesh_apply_warnings(${MANUMESH_CLI_TARGET})
