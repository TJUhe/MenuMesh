add_executable(manumesh
  CliArguments.cpp
  CliArguments.h
  CliCsv.cpp
  CliCsv.h
  CliCommands.h
  CliOptionBinding.cpp
  CliOptionBinding.h
  ManuMeshCommands.cpp
  ManuMeshFeatureCommands.cpp
  ManuMeshFeatureCommands.h
  ManuMeshWorkflowCommands.cpp
  ManuMeshWorkflowCommands.h
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
  add_test(NAME manumesh_cli_rejects_cross_command_option
    COMMAND manumesh generate --type plane --n 12
      --out "${CMAKE_CURRENT_BINARY_DIR}/cli_cross_command_option.stl"
      --ratio 0.5
  )
  add_test(NAME manumesh_cli_simplify_smooth_curvature
    COMMAND manumesh simplify
      "${PROJECT_SOURCE_DIR}/tests/data/feature_fixtures/boss_pocket_plate.obj"
      "${CMAKE_CURRENT_BINARY_DIR}/cli_smooth_curvature.stl"
      --ratio 0.90
      --smooth-curvature-features
      --smooth-curvature-base-rings 2
      --smooth-curvature-scales 2
      --smooth-curvature-min-persistent-scales 1
  )
  add_test(NAME manumesh_cli_feature_report
    COMMAND manumesh feature-report
      "${PROJECT_SOURCE_DIR}/tests/data/feature_fixtures/coaxial_hole_plate.obj"
      --smooth-curvature-features
      --smooth-curvature-base-rings 2
      --smooth-curvature-scales 3
      --smooth-curvature-min-persistent-scales 2
      --smooth-curvature-stable-scale
      --feature-normal-filter
      --feature-normal-filter-iterations 2
      --feature-graph-consolidation
      --surface-patches
  )
  add_test(NAME manumesh_cli_feature_benchmark
    COMMAND manumesh feature-benchmark
      "${PROJECT_SOURCE_DIR}/tests/data/feature_fixtures/coaxial_hole_plate.obj"
      "${PROJECT_SOURCE_DIR}/tests/data/feature_labels/coaxial_hole_plate_inner_top_edges.csv"
  )
  add_test(NAME manumesh_cli_feature_compare
    COMMAND manumesh feature-compare
      "${PROJECT_SOURCE_DIR}/tests/data/feature_fixtures/coaxial_hole_plate.obj"
      "${PROJECT_SOURCE_DIR}/tests/data/feature_fixtures/coaxial_hole_plate.obj"
  )
  add_test(NAME manumesh_cli_quality_refinement
    COMMAND manumesh simplify
      "${PROJECT_SOURCE_DIR}/tests/data/feature_fixtures/boss_pocket_plate.obj"
      "${CMAKE_CURRENT_BINARY_DIR}/cli_quality_refinement.stl"
      --ratio 0.75
      --preserve-boundary
      --quality-refinement-iterations 2
  )
  set_tests_properties(manumesh_cli_rejects_unknown_option
    manumesh_cli_rejects_missing_option_value
    manumesh_cli_rejects_malformed_number
    manumesh_cli_rejects_cross_command_option
      PROPERTIES WILL_FAIL TRUE
  )
  set_tests_properties(manumesh_cli_feature_report
    manumesh_cli_feature_benchmark
    manumesh_cli_feature_compare
    manumesh_cli_quality_refinement
    manumesh_cli_simplify_smooth_curvature
    manumesh_cli_rejects_cross_command_option
    PROPERTIES LABELS cli
  )
  set_tests_properties(manumesh_cli_feature_report
    PROPERTIES PASS_REGULAR_EXPRESSION "surface_patches="
  )
  set_tests_properties(manumesh_cli_feature_benchmark
    PROPERTIES FAIL_REGULAR_EXPRESSION "unparsable label row"
  )
endif()

if(MANUMESH_ENABLE_INSTALL)
  install(TARGETS manumesh
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
endif()
