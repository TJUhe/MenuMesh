add_executable(manumesh
  CliArguments.cpp
  CliArguments.h
  CliCsv.cpp
  CliCsv.h
  CliCommands.h
  CliOptionBinding.cpp
  CliOptionBinding.h
  ManuMeshCommands.cpp
  main.cpp
  ManuMeshCli.cpp
  ManuMeshCli.h
)
target_link_libraries(manumesh PRIVATE ManuMesh::manumesh)
manumesh_apply_warnings(manumesh)
manumesh_copy_runtime_dependencies(manumesh)

if(BUILD_TESTING)
  add_test(NAME manumesh_cli_help COMMAND manumesh --help)
  add_test(NAME manumesh_cli_rejects_unknown_option
    COMMAND manumesh simplify --not-a-real-option
  )
  add_test(NAME manumesh_cli_rejects_missing_option_value
    COMMAND manumesh generate --type
  )
  add_test(NAME manumesh_cli_rejects_malformed_number
    COMMAND manumesh generate --type plane --n 12x --out bad.stl
  )
  set_tests_properties(manumesh_cli_rejects_unknown_option
    manumesh_cli_rejects_missing_option_value
    manumesh_cli_rejects_malformed_number
    PROPERTIES WILL_FAIL TRUE
  )
endif()

if(MANUMESH_ENABLE_INSTALL)
  install(TARGETS manumesh
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
endif()
