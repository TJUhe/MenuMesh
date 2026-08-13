option(MANUMESH_BUILD_DOCS
  "Doxygen 可用时启用 docs-api 和 docs-internal 目标"
  ON
)
option(MANUMESH_DOXYGEN_ENABLE_GRAPHS
  "dot 可用时在生成的 Doxygen 文档中启用 Graphviz 图表"
  ON
)
option(MANUMESH_INSTALL_CMAKE_CONFIG
  "为 CMake consumer 安装可选的 CMake 配置文件"
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
  COMMENT "正在检查 src Doxygen 注释约定"
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
    COMMENT "正在使用 clang-format 格式化 C++ 源码"
    VERBATIM
  )
  add_custom_target(check-format
    COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror ${MANUMESH_FORMAT_FILES}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "正在检查 C++ 源码格式"
    VERBATIM
  )
else()
  add_custom_target(format
    COMMAND "${CMAKE_COMMAND}" -E echo
      "未找到 clang-format；请安装后启用 format 目标。"
    VERBATIM
  )
  add_custom_target(check-format
    COMMAND "${CMAKE_COMMAND}" -E echo
      "未找到 clang-format；请安装后启用 check-format 目标。"
    VERBATIM
  )
endif()

if(MANUMESH_BUILD_DOCS)
  if(EXISTS "${MANUMESH_VENDORED_DOXYGEN_EXECUTABLE}")
    set(DOXYGEN_EXECUTABLE "${MANUMESH_VENDORED_DOXYGEN_EXECUTABLE}"
      CACHE FILEPATH "Vendored Doxygen 可执行文件" FORCE
    )
  endif()
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    if(EXISTS "${MANUMESH_VENDORED_GRAPHVIZ_DOT_EXE}")
      set(MANUMESH_DOXYGEN_DOT_EXE "${MANUMESH_VENDORED_GRAPHVIZ_DOT_EXE}"
        CACHE FILEPATH "Vendored Graphviz dot 可执行文件" FORCE
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
      "C++17 三角网格分析、特征检测与简化内核"
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
      COMMENT "正在使用 Doxygen 生成 API 文档"
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
      "私有实现、源码浏览和工程调用关系"
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
      COMMENT "正在使用 Doxygen 生成内部源码文档"
      VERBATIM
    )
  else()
    add_custom_target(docs-api
      COMMAND "${CMAKE_COMMAND}" -E echo
        "未找到 Doxygen；请安装后启用 API 文档生成。"
      VERBATIM
    )
    add_custom_target(docs-internal
      COMMAND "${CMAKE_COMMAND}" -E echo
        "未找到 Doxygen；请安装后启用内部源码文档生成。"
      VERBATIM
    )
  endif()

  add_dependencies(docs-api check-src-doxygen)
  add_dependencies(docs-internal check-src-doxygen)
endif()

if(MANUMESH_ENABLE_INSTALL)
  set(MANUMESH_LOCAL_SDK_PREFIX
    "${CMAKE_BINARY_DIR}/sdk"
    CACHE PATH "供 sdk-install-local 和 sdk-consumer-test 使用的本地已安装 SDK 根目录"
  )
  set(MANUMESH_SDK_CONSUMER_BUILD_DIR
    "${CMAKE_BINARY_DIR}/sdk-consumer-build"
    CACHE PATH "已安装 SDK consumer 示例的构建目录"
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
    COMMENT "正在将本地 SDK 布局安装到 ${MANUMESH_LOCAL_SDK_PREFIX}"
    VERBATIM
  )

  add_custom_target(sdk-runtime-test
    COMMAND "${CMAKE_COMMAND}"
      "-DMANUMESH_SDK_ROOT=${MANUMESH_LOCAL_SDK_PREFIX}"
      "-DMANUMESH_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
      -P "${PROJECT_SOURCE_DIR}/adm/CheckInstalledRuntime.cmake"
    DEPENDS sdk-install-local
    COMMENT "正在使用隔离运行时 PATH 测试已安装 CLI"
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
    if(MSVC AND CMAKE_MSVC_RUNTIME_LIBRARY)
      list(APPEND MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
      )
    endif()
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
      COMMENT "正在针对已安装 SDK 构建并运行示例"
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



