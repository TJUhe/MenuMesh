option(MANUMESH_BUILD_DOCS
  "Enable the Doxygen docs-api and docs-internal targets when Doxygen is available"
  ON
)
option(MANUMESH_DOXYGEN_ENABLE_GRAPHS
  "Enable Graphviz-backed diagrams in generated Doxygen documentation when dot is available"
  ON
)
option(MANUMESH_INSTALL_CMAKE_CONFIG
  "Install optional CMake config files for CMake consumers"
  ON
)


set(MANUMESH_VENDORED_DOXYGEN_EXECUTABLE
  "${PROJECT_SOURCE_DIR}/thirdParty/doxygen/prebuilt/windows-x64/bin/doxygen.exe"
)
set(MANUMESH_VENDORED_GRAPHVIZ_ROOT
  "${PROJECT_SOURCE_DIR}/thirdParty/graphviz/prebuilt/windows-x64"
)
set(MANUMESH_VENDORED_GRAPHVIZ_DOT_EXE
  "${MANUMESH_VENDORED_GRAPHVIZ_ROOT}/bin/dot.exe"
)
add_custom_target(check-src-doxygen
  COMMAND "${CMAKE_COMMAND}"
    -DMANUMESH_SOURCE_DIR=${PROJECT_SOURCE_DIR}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckSrcDoxygen.cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking src Doxygen comment conventions"
  VERBATIM
)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
file(GLOB_RECURSE MANUMESH_FORMAT_FILES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/*.h"
  "${PROJECT_SOURCE_DIR}/apps/*.cpp"
  "${PROJECT_SOURCE_DIR}/examples/*.c"
  "${PROJECT_SOURCE_DIR}/examples/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/*.h"
  "${PROJECT_SOURCE_DIR}/tests/*.cpp"
  "${PROJECT_SOURCE_DIR}/tests/*.h"
)

if(CLANG_FORMAT_EXE)
  add_custom_target(format
    COMMAND "${CLANG_FORMAT_EXE}" -i ${MANUMESH_FORMAT_FILES}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Formatting C++ sources with clang-format"
    VERBATIM
  )
  add_custom_target(check-format
    COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror ${MANUMESH_FORMAT_FILES}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Checking C++ source formatting"
    VERBATIM
  )
else()
  add_custom_target(format
    COMMAND "${CMAKE_COMMAND}" -E echo
      "clang-format was not found; install it to enable the format target."
    VERBATIM
  )
  add_custom_target(check-format
    COMMAND "${CMAKE_COMMAND}" -E echo
      "clang-format was not found; install it to enable the check-format target."
    VERBATIM
  )
endif()

if(MANUMESH_BUILD_DOCS)
  if(EXISTS "${MANUMESH_VENDORED_DOXYGEN_EXECUTABLE}")
    set(DOXYGEN_EXECUTABLE "${MANUMESH_VENDORED_DOXYGEN_EXECUTABLE}"
      CACHE FILEPATH "Vendored Doxygen executable" FORCE
    )
  endif()
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    if(EXISTS "${MANUMESH_VENDORED_GRAPHVIZ_DOT_EXE}")
      set(MANUMESH_DOXYGEN_DOT_EXE "${MANUMESH_VENDORED_GRAPHVIZ_DOT_EXE}"
        CACHE FILEPATH "Vendored Graphviz dot executable" FORCE
      )
    else()
      find_program(MANUMESH_DOXYGEN_DOT_EXE NAMES dot)
    endif()
    set(MANUMESH_DOXYGEN_HAVE_DOT "NO")
    set(MANUMESH_DOXYGEN_DOT_PATH "")
    if(MANUMESH_DOXYGEN_ENABLE_GRAPHS AND MANUMESH_DOXYGEN_DOT_EXE)
      set(MANUMESH_DOXYGEN_HAVE_DOT "YES")
      get_filename_component(MANUMESH_DOXYGEN_DOT_PATH
        "${MANUMESH_DOXYGEN_DOT_EXE}" DIRECTORY
      )
    endif()
    set(MANUMESH_DOXYGEN_INPUT_FILES
      "${PROJECT_SOURCE_DIR}/documentation/doxygen"
      "${PROJECT_SOURCE_DIR}/include"
      "${PROJECT_SOURCE_DIR}/src"
      "${PROJECT_SOURCE_DIR}/apps"
      "${PROJECT_SOURCE_DIR}/examples"
    )
    string(JOIN " " MANUMESH_DOXYGEN_INPUT ${MANUMESH_DOXYGEN_INPUT_FILES})
    set(MANUMESH_DOXYGEN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/docs/doxygen")
    set(MANUMESH_DOXYGEN_PROJECT_NAME "ManuMesh")
    set(MANUMESH_DOXYGEN_PROJECT_BRIEF
      "C++17 triangle-mesh analysis, feature detection, and simplification kernel"
    )
    set(MANUMESH_DOXYGEN_EXTRACT_ALL "YES")
    set(MANUMESH_DOXYGEN_EXTRACT_PRIVATE "YES")
    set(MANUMESH_DOXYGEN_EXTRACT_STATIC "YES")
    set(MANUMESH_DOXYGEN_EXTRACT_LOCAL_CLASSES "YES")
    set(MANUMESH_DOXYGEN_EXTRACT_ANON_NSPACES "NO")
    set(MANUMESH_DOXYGEN_INTERNAL_DOCS "NO")
    set(MANUMESH_DOXYGEN_WARN_IF_UNDOCUMENTED "YES")
    set(MANUMESH_DOXYGEN_INLINE_SOURCES "NO")
    configure_file(
      "${PROJECT_SOURCE_DIR}/Doxyfile.in"
      "${PROJECT_BINARY_DIR}/Doxyfile"
      @ONLY
    )
    add_custom_target(docs-api
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${MANUMESH_DOXYGEN_OUTPUT_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=${MANUMESH_DOXYGEN_DOT_PATH};$ENV{PATH}"
        "GVBINDIR=${MANUMESH_DOXYGEN_DOT_PATH}"
        "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM
    )

    set(MANUMESH_DOXYGEN_INPUT_FILES
      "${PROJECT_SOURCE_DIR}/documentation/doxygen-internal"
      "${PROJECT_SOURCE_DIR}/documentation/doxygen/groups.dox"
      "${PROJECT_SOURCE_DIR}/include"
      "${PROJECT_SOURCE_DIR}/src"
    )
    string(JOIN " " MANUMESH_DOXYGEN_INPUT ${MANUMESH_DOXYGEN_INPUT_FILES})
    set(MANUMESH_DOXYGEN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/docs/internal")
    set(MANUMESH_DOXYGEN_PROJECT_NAME "ManuMesh Internal Source Reference")
    set(MANUMESH_DOXYGEN_PROJECT_BRIEF
      "Private implementation, source browser, and engineering call relationships"
    )
    set(MANUMESH_DOXYGEN_EXTRACT_ALL "YES")
    set(MANUMESH_DOXYGEN_EXTRACT_PRIVATE "YES")
    set(MANUMESH_DOXYGEN_EXTRACT_STATIC "YES")
    set(MANUMESH_DOXYGEN_EXTRACT_LOCAL_CLASSES "YES")
    set(MANUMESH_DOXYGEN_EXTRACT_ANON_NSPACES "YES")
    set(MANUMESH_DOXYGEN_INTERNAL_DOCS "YES")
    set(MANUMESH_DOXYGEN_WARN_IF_UNDOCUMENTED "NO")
    set(MANUMESH_DOXYGEN_INLINE_SOURCES "YES")
    configure_file(
      "${PROJECT_SOURCE_DIR}/Doxyfile.in"
      "${PROJECT_BINARY_DIR}/Doxyfile.internal"
      @ONLY
    )
    add_custom_target(docs-internal
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${MANUMESH_DOXYGEN_OUTPUT_DIR}"
      COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=${MANUMESH_DOXYGEN_DOT_PATH};$ENV{PATH}"
        "GVBINDIR=${MANUMESH_DOXYGEN_DOT_PATH}"
        "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile.internal"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Generating internal source documentation with Doxygen"
      VERBATIM
    )
  else()
    add_custom_target(docs-api
      COMMAND "${CMAKE_COMMAND}" -E echo
        "Doxygen was not found; install it to enable API documentation generation."
      VERBATIM
    )
    add_custom_target(docs-internal
      COMMAND "${CMAKE_COMMAND}" -E echo
        "Doxygen was not found; install it to enable internal source documentation generation."
      VERBATIM
    )
  endif()

  add_dependencies(docs-api check-src-doxygen)
  add_dependencies(docs-internal check-src-doxygen)
endif()

if(MANUMESH_ENABLE_INSTALL)
  set(MANUMESH_LOCAL_SDK_PREFIX
    "${CMAKE_BINARY_DIR}/sdk"
    CACHE PATH "Local installed SDK root used by sdk-install-local and sdk-consumer-test"
  )
  set(MANUMESH_SDK_CONSUMER_BUILD_DIR
    "${CMAKE_BINARY_DIR}/sdk-consumer-build"
    CACHE PATH "Build directory for the installed-SDK consumer examples"
  )

  set(MANUMESH_HAS_MSVC_PROPS FALSE)
  if(MSVC)
    configure_file(
      "${PROJECT_SOURCE_DIR}/adm/templates/ManuMesh.props.in"
      "${PROJECT_BINARY_DIR}/ManuMesh.props"
      @ONLY
    )

    install(FILES
      "${PROJECT_BINARY_DIR}/ManuMesh.props"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/manumesh/msvc"
    )
    set(MANUMESH_HAS_MSVC_PROPS TRUE)
  endif()

  if(MANUMESH_INSTALL_CMAKE_CONFIG)
    include(CMakePackageConfigHelpers)

    install(EXPORT ManuMeshTargets
      NAMESPACE ManuMesh::
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ManuMesh"
    )

    configure_package_config_file(
      "${PROJECT_SOURCE_DIR}/adm/templates/ManuMeshConfig.cmake.in"
      "${PROJECT_BINARY_DIR}/ManuMeshConfig.cmake"
      INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ManuMesh"
    )
    write_basic_package_version_file(
      "${PROJECT_BINARY_DIR}/ManuMeshConfigVersion.cmake"
      VERSION ${PROJECT_VERSION}
      COMPATIBILITY SameMajorVersion
    )

    install(FILES
      "${PROJECT_BINARY_DIR}/ManuMeshConfig.cmake"
      "${PROJECT_BINARY_DIR}/ManuMeshConfigVersion.cmake"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ManuMesh"
    )
  endif()

  install(FILES
    "${PROJECT_SOURCE_DIR}/README.md"
    "${PROJECT_SOURCE_DIR}/documentation/design/architecture.md"
    "${PROJECT_SOURCE_DIR}/documentation/design/industrial_library.md"
    "${PROJECT_SOURCE_DIR}/documentation/guide/sdk_integration.md"
    "${PROJECT_SOURCE_DIR}/documentation/design/large_model_validation.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
  )

  install(FILES
    "${PROJECT_SOURCE_DIR}/examples/basic_simplify.cpp"
    "${PROJECT_SOURCE_DIR}/examples/c_api_basic.c"
    "${PROJECT_SOURCE_DIR}/examples/feature_workflow_demo.cpp"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/manumesh/samples"
  )

  set(MANUMESH_SDK_INSTALL_DEPENDS manumesh_core)
  if(TARGET manumesh)
    list(APPEND MANUMESH_SDK_INSTALL_DEPENDS manumesh)
  endif()

  add_custom_target(sdk-install-local
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
      --config "$<CONFIG>"
      --prefix "${MANUMESH_LOCAL_SDK_PREFIX}"
    DEPENDS ${MANUMESH_SDK_INSTALL_DEPENDS}
    COMMENT "Installing local SDK layout to ${MANUMESH_LOCAL_SDK_PREFIX}"
    VERBATIM
  )

  add_custom_target(sdk-runtime-test
    COMMAND "${CMAKE_COMMAND}"
      "-DMANUMESH_SDK_ROOT=${MANUMESH_LOCAL_SDK_PREFIX}"
      "-DMANUMESH_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
      -P "${PROJECT_SOURCE_DIR}/adm/CheckInstalledRuntime.cmake"
    DEPENDS sdk-install-local
    COMMENT "Testing the installed CLI with an isolated runtime PATH"
    VERBATIM
  )

  if(MANUMESH_INSTALL_CMAKE_CONFIG)
    set(MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS
      "-S" "${PROJECT_SOURCE_DIR}/examples/sdk_consumer"
      "-B" "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
      "-G" "${CMAKE_GENERATOR}"
      "-DMANUMESH_SDK_ROOT=${MANUMESH_LOCAL_SDK_PREFIX}"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    )
    if(CMAKE_GENERATOR_PLATFORM)
      list(APPEND MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS "-A" "${CMAKE_GENERATOR_PLATFORM}")
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
      list(APPEND MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS "-T" "${CMAKE_GENERATOR_TOOLSET}")
    endif()
    if(NOT CMAKE_CONFIGURATION_TYPES)
      list(APPEND MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
      )
    endif()

    add_custom_target(sdk-consumer-test
      COMMAND "${CMAKE_COMMAND}" -E remove_directory "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
      COMMAND "${CMAKE_COMMAND}" ${MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS}
      COMMAND "${CMAKE_COMMAND}" --build "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
        --config "$<CONFIG>"
        --parallel
      COMMAND "${CMAKE_COMMAND}" -E chdir "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
        "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>"
        --output-on-failure
      DEPENDS sdk-runtime-test
      COMMENT "Building and running examples against the installed SDK"
      VERBATIM
    )

    if(BUILD_TESTING)
      add_test(NAME sdk_consumer_examples
        COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
          --config "$<CONFIG>"
          --target sdk-consumer-test
      )
    endif()
  endif()
endif()



