option(MANUMESH_BUILD_DOCS
  "启用 docs-api 和 docs-internal 目标；配置时必须可找到 Doxygen"
  OFF
)
option(MANUMESH_DOXYGEN_ENABLE_GRAPHS
  "dot 可用时在生成的 Doxygen 文档中启用 Graphviz 图表"
  ON
)
set(MANUMESH_DOXYGEN_OUTPUT_LANGUAGE "English" CACHE STRING
  "Doxygen 生成页面的界面语言（English 或 Chinese）"
)
set_property(CACHE MANUMESH_DOXYGEN_OUTPUT_LANGUAGE PROPERTY STRINGS English Chinese)
if(NOT "${MANUMESH_DOXYGEN_OUTPUT_LANGUAGE}" MATCHES "^(English|Chinese)$")
  message(FATAL_ERROR
    "MANUMESH_DOXYGEN_OUTPUT_LANGUAGE 必须是 English 或 Chinese；"
    "当前值为 '${MANUMESH_DOXYGEN_OUTPUT_LANGUAGE}'。"
  )
endif()
option(MANUMESH_INSTALL_CMAKE_CONFIG
  "为 CMake consumer 安装可选的 CMake 配置文件"
  ON
)


set(MANUMESH_VENDORED_GRAPHVIZ_ROOT
  "${PROJECT_SOURCE_DIR}/thirdParty/graphviz/prebuilt/windows-x64"
)
set(MANUMESH_VENDORED_GRAPHVIZ_DOT_EXE
  "${MANUMESH_VENDORED_GRAPHVIZ_ROOT}/bin/dot.exe"
)
set(MANUMESH_VENDORED_DOXYGEN_ROOT
  "${PROJECT_SOURCE_DIR}/thirdParty/doxygen"
  CACHE PATH "Vendored Doxygen 工具包根目录"
)
set(MANUMESH_VENDORED_DOXYGEN_EXE "")
foreach(manumesh_doxygen_candidate IN ITEMS
    "${MANUMESH_VENDORED_DOXYGEN_ROOT}/bin/doxygen.exe"
    "${MANUMESH_VENDORED_DOXYGEN_ROOT}/doxygen.exe"
    "${MANUMESH_VENDORED_DOXYGEN_ROOT}/bin/doxygen"
    "${MANUMESH_VENDORED_DOXYGEN_ROOT}/doxygen")
  if(EXISTS "${manumesh_doxygen_candidate}" AND
     NOT IS_DIRECTORY "${manumesh_doxygen_candidate}")
    set(MANUMESH_VENDORED_DOXYGEN_EXE "${manumesh_doxygen_candidate}")
    break()
  endif()
endforeach()
unset(manumesh_doxygen_candidate)
add_custom_target(check-src-doxygen
  COMMAND "${CMAKE_COMMAND}"
    -DMANUMESH_SOURCE_DIR=${PROJECT_SOURCE_DIR}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckSrcDoxygen.cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "正在检查 src Doxygen 注释约定"
  VERBATIM
)

set(MANUMESH_CLANG_FORMAT_REQUIRED_MAJOR "22")
set(MANUMESH_CLANG_FORMAT_READY OFF)
set(MANUMESH_CLANG_FORMAT_DIAGNOSTIC
  "未找到 clang-format；format/check-format 需要 clang-format 22.x。"
)
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  execute_process(
    COMMAND "${CLANG_FORMAT_EXE}" --version
    RESULT_VARIABLE MANUMESH_CLANG_FORMAT_VERSION_RESULT
    OUTPUT_VARIABLE MANUMESH_CLANG_FORMAT_VERSION_OUTPUT
    ERROR_VARIABLE MANUMESH_CLANG_FORMAT_VERSION_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  if(NOT MANUMESH_CLANG_FORMAT_VERSION_RESULT EQUAL 0)
    set(MANUMESH_CLANG_FORMAT_DIAGNOSTIC
      "无法执行 '${CLANG_FORMAT_EXE} --version'：${MANUMESH_CLANG_FORMAT_VERSION_ERROR}"
    )
  elseif(MANUMESH_CLANG_FORMAT_VERSION_OUTPUT MATCHES
         "clang-format version ([0-9]+)(\\.[0-9]+)*")
    set(MANUMESH_CLANG_FORMAT_DETECTED_MAJOR "${CMAKE_MATCH_1}")
    if(MANUMESH_CLANG_FORMAT_DETECTED_MAJOR STREQUAL
       MANUMESH_CLANG_FORMAT_REQUIRED_MAJOR)
      set(MANUMESH_CLANG_FORMAT_READY ON)
    else()
      set(MANUMESH_CLANG_FORMAT_DIAGNOSTIC
        "检测到 ${MANUMESH_CLANG_FORMAT_VERSION_OUTPUT}；format/check-format 需要 clang-format 22.x。"
      )
    endif()
  else()
    set(MANUMESH_CLANG_FORMAT_DIAGNOSTIC
      "无法识别 clang-format 版本：${MANUMESH_CLANG_FORMAT_VERSION_OUTPUT}"
    )
  endif()
endif()
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

if(MANUMESH_CLANG_FORMAT_READY)
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
  message(STATUS "ManuMesh 格式目标不可用：${MANUMESH_CLANG_FORMAT_DIAGNOSTIC}")
  add_custom_target(format
    COMMAND "${CMAKE_COMMAND}" -E echo
      "${MANUMESH_CLANG_FORMAT_DIAGNOSTIC}"
    COMMAND "${CMAKE_COMMAND}" -E false
    VERBATIM
  )
  add_custom_target(check-format
    COMMAND "${CMAKE_COMMAND}" -E echo
      "${MANUMESH_CLANG_FORMAT_DIAGNOSTIC}"
    COMMAND "${CMAKE_COMMAND}" -E false
    VERBATIM
  )
endif()

function(manumesh_quote_doxygen_paths output_variable)
  set(quoted_paths "")
  foreach(path IN LISTS ARGN)
    string(APPEND quoted_paths " \"${path}\"")
  endforeach()
  string(STRIP "${quoted_paths}" quoted_paths)
  set(${output_variable} "${quoted_paths}" PARENT_SCOPE)
endfunction()

if(MANUMESH_BUILD_DOCS)
  if(MANUMESH_VENDORED_DOXYGEN_EXE)
    set(DOXYGEN_EXECUTABLE "${MANUMESH_VENDORED_DOXYGEN_EXE}"
      CACHE FILEPATH "Vendored Doxygen executable" FORCE
    )
  endif()
  find_package(Doxygen REQUIRED)
  if(EXISTS "${MANUMESH_VENDORED_GRAPHVIZ_DOT_EXE}")
    set(MANUMESH_DOXYGEN_DOT_EXE "${MANUMESH_VENDORED_GRAPHVIZ_DOT_EXE}"
      CACHE FILEPATH "Vendored Graphviz dot 可执行文件" FORCE
    )
  else()
    if(MANUMESH_DOXYGEN_DOT_EXE AND
       NOT EXISTS "${MANUMESH_DOXYGEN_DOT_EXE}")
      unset(MANUMESH_DOXYGEN_DOT_EXE CACHE)
    endif()
    find_program(MANUMESH_DOXYGEN_DOT_EXE NAMES dot)
  endif()
  set(MANUMESH_DOXYGEN_HAVE_DOT "NO")
  set(MANUMESH_DOXYGEN_DOT_PATH "")
  if(MANUMESH_DOXYGEN_ENABLE_GRAPHS AND
     MANUMESH_DOXYGEN_DOT_EXE AND
     EXISTS "${MANUMESH_DOXYGEN_DOT_EXE}" AND
     NOT IS_DIRECTORY "${MANUMESH_DOXYGEN_DOT_EXE}")
    set(MANUMESH_DOXYGEN_HAVE_DOT "YES")
    get_filename_component(MANUMESH_DOXYGEN_DOT_PATH
      "${MANUMESH_DOXYGEN_DOT_EXE}" DIRECTORY
    )
  elseif(MANUMESH_DOXYGEN_ENABLE_GRAPHS)
    message(STATUS
      "未找到可用的 Graphviz dot；Doxygen 继续生成，但不包含关系图。"
    )
  endif()
  set(MANUMESH_DOXYGEN_INPUT_FILES
    "${PROJECT_SOURCE_DIR}/documentation/doxygen"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/apps"
    "${PROJECT_SOURCE_DIR}/examples"
  )
  manumesh_quote_doxygen_paths(MANUMESH_DOXYGEN_INPUT
    ${MANUMESH_DOXYGEN_INPUT_FILES}
  )
  set(MANUMESH_DOXYGEN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/docs/doxygen")
  set(MANUMESH_DOXYGEN_PROJECT_NAME "ManuMesh")
  set(MANUMESH_DOXYGEN_PROJECT_BRIEF
    "C++14 三角网格分析、特征检测与简化内核"
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
  manumesh_quote_doxygen_paths(MANUMESH_DOXYGEN_INPUT
    ${MANUMESH_DOXYGEN_INPUT_FILES}
  )
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
      "GVBINDIR=${MANUMESH_DOXYGEN_DOT_PATH}"
      "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile.internal"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "正在使用 Doxygen 生成内部源码文档"
    VERBATIM
  )
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
  set(MANUMESH_C_API_ONLY_SDK_PREFIX
    "${CMAKE_BINARY_DIR}/sdk-c-api-only"
    CACHE PATH "Installed SDK clone used to verify the C ABI without Eigen"
  )
  set(MANUMESH_C_API_ONLY_CONSUMER_BUILD_DIR
    "${CMAKE_BINARY_DIR}/sdk-c-api-only-consumer-build"
    CACHE PATH "Build directory for the installed pure C ABI consumer"
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
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
  )

  # Keep the documentation tree intact so links from the installed README
  # continue to resolve without depending on the source checkout.
  install(DIRECTORY
    "${PROJECT_SOURCE_DIR}/documentation/"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/documentation"
    FILES_MATCHING
      PATTERN "*.md"
      PATTERN "*.dox"
  )

  install(FILES
    "${PROJECT_SOURCE_DIR}/examples/basic_simplify.cpp"
    "${PROJECT_SOURCE_DIR}/examples/c_api_basic.c"
    "${PROJECT_SOURCE_DIR}/examples/feature_workflow_demo.cpp"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/manumesh/samples"
  )

  set(MANUMESH_SDK_INSTALL_DEPENDS "${MANUMESH_CORE_TARGET}")
  if(TARGET "${MANUMESH_CLI_TARGET}")
    list(APPEND MANUMESH_SDK_INSTALL_DEPENDS "${MANUMESH_CLI_TARGET}")
  endif()

  add_custom_target(sdk-install-local
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
      --config "$<CONFIG>"
      --prefix "${MANUMESH_LOCAL_SDK_PREFIX}"
    DEPENDS ${MANUMESH_SDK_INSTALL_DEPENDS}
    COMMENT "正在将本地 SDK 布局安装到 ${MANUMESH_LOCAL_SDK_PREFIX}"
    VERBATIM
  )

  if(TARGET "${MANUMESH_CLI_TARGET}")
    add_custom_target(sdk-runtime-test
      COMMAND "${CMAKE_COMMAND}"
        "-DMANUMESH_SDK_ROOT=${MANUMESH_LOCAL_SDK_PREFIX}"
        "-DMANUMESH_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DMANUMESH_EXPECT_ONETBB_NOTICES=${MANUMESH_ONETBB_NOTICES_INSTALLED}"
        "-DMANUMESH_ONETBB_NOTICE_DIR=${CMAKE_INSTALL_DOCDIR}/oneTBB"
        -P "${PROJECT_SOURCE_DIR}/adm/CheckInstalledRuntime.cmake"
      DEPENDS sdk-install-local
      COMMENT "正在使用隔离运行时 PATH 测试已安装 CLI"
      VERBATIM
    )
  else()
    add_custom_target(sdk-runtime-test
      COMMAND "${CMAKE_COMMAND}" -E echo
        "MANUMESH_BUILD_CLI=OFF：跳过已安装 CLI 运行时检查。"
      DEPENDS sdk-install-local
      COMMENT "已关闭 CLI；仅安装 SDK 并继续 consumer 验证"
      VERBATIM
    )
  endif()

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

    set(MANUMESH_C_API_ONLY_CONSUMER_CONFIGURE_ARGS
      "-S" "${PROJECT_SOURCE_DIR}/examples/sdk_consumer"
      "-B" "${MANUMESH_C_API_ONLY_CONSUMER_BUILD_DIR}"
      "-G" "${CMAKE_GENERATOR}"
      "-DMANUMESH_SDK_ROOT=${MANUMESH_C_API_ONLY_SDK_PREFIX}"
      "-DMANUMESH_SDK_C_API_ONLY=ON"
      "-DCMAKE_DISABLE_FIND_PACKAGE_Eigen3=TRUE"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    )
    if(MSVC AND CMAKE_MSVC_RUNTIME_LIBRARY)
      list(APPEND MANUMESH_C_API_ONLY_CONSUMER_CONFIGURE_ARGS
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
      )
    endif()
    if(CMAKE_GENERATOR_PLATFORM)
      list(APPEND MANUMESH_C_API_ONLY_CONSUMER_CONFIGURE_ARGS
        "-A" "${CMAKE_GENERATOR_PLATFORM}"
      )
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
      list(APPEND MANUMESH_C_API_ONLY_CONSUMER_CONFIGURE_ARGS
        "-T" "${CMAKE_GENERATOR_TOOLSET}"
      )
    endif()
    if(NOT CMAKE_CONFIGURATION_TYPES)
      list(APPEND MANUMESH_C_API_ONLY_CONSUMER_CONFIGURE_ARGS
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
      )
    endif()

    add_custom_target(sdk-c-api-consumer-test
      COMMAND "${CMAKE_COMMAND}" -E remove_directory
        "${MANUMESH_C_API_ONLY_SDK_PREFIX}"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${MANUMESH_LOCAL_SDK_PREFIX}"
        "${MANUMESH_C_API_ONLY_SDK_PREFIX}"
      COMMAND "${CMAKE_COMMAND}" -E remove_directory
        "${MANUMESH_C_API_ONLY_SDK_PREFIX}/${CMAKE_INSTALL_DATADIR}/manumesh/thirdParty/eigen"
      COMMAND "${CMAKE_COMMAND}" -E remove_directory
        "${MANUMESH_C_API_ONLY_CONSUMER_BUILD_DIR}"
      COMMAND "${CMAKE_COMMAND}" ${MANUMESH_C_API_ONLY_CONSUMER_CONFIGURE_ARGS}
      COMMAND "${CMAKE_COMMAND}" --build
        "${MANUMESH_C_API_ONLY_CONSUMER_BUILD_DIR}"
        --config "$<CONFIG>"
        --parallel
      COMMAND "${CMAKE_COMMAND}" -E chdir
        "${MANUMESH_C_API_ONLY_CONSUMER_BUILD_DIR}"
        "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>"
        --output-on-failure
      DEPENDS sdk-runtime-test
      COMMENT "Verifying the installed C ABI without bundled or discoverable Eigen"
      VERBATIM
    )

    # 已安装 SDK 的 consumer 共用一个输出目录，并复制同一组运行时 DLL。
    # 串行构建这些小目标，避免 Windows 上的 post-build 复制相互竞争。
    add_custom_target(sdk-consumer-test
      COMMAND "${CMAKE_COMMAND}" -E remove_directory "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
      COMMAND "${CMAKE_COMMAND}" ${MANUMESH_SDK_CONSUMER_CONFIGURE_ARGS}
      COMMAND "${CMAKE_COMMAND}" --build "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
        --config "$<CONFIG>"
        --parallel 1
      COMMAND "${CMAKE_COMMAND}" -E chdir "${MANUMESH_SDK_CONSUMER_BUILD_DIR}"
        "${CMAKE_CTEST_COMMAND}" -C "$<CONFIG>"
        --output-on-failure
      DEPENDS sdk-c-api-consumer-test
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



