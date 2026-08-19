if(NOT DEFINED MANUMESH_EXECUTABLE OR "${MANUMESH_EXECUTABLE}" STREQUAL "" OR
   NOT EXISTS "${MANUMESH_EXECUTABLE}")
  message(FATAL_ERROR "MANUMESH_EXECUTABLE must name an existing executable.")
endif()

if(NOT DEFINED INPUT_MESH OR "${INPUT_MESH}" STREQUAL "" OR NOT EXISTS "${INPUT_MESH}")
  message(FATAL_ERROR "INPUT_MESH must name an existing fixture.")
endif()

if(NOT DEFINED OUTPUT_DIR OR "${OUTPUT_DIR}" STREQUAL "")
  message(FATAL_ERROR "OUTPUT_DIR is required.")
endif()

function(expect_failure expected_text)
  execute_process(
    COMMAND "${MANUMESH_EXECUTABLE}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )

  if("${result}" STREQUAL "0")
    message(FATAL_ERROR "Expected failure for: ${ARGN}\nstdout:\n${stdout}\nstderr:\n${stderr}")
  endif()

  set(transcript "${stdout}\n${stderr}")
  string(FIND "${transcript}" "${expected_text}" expected_index)
  if(expected_index EQUAL -1)
    message(FATAL_ERROR
      "Failure did not explain the invalid contract. Expected: ${expected_text}\n"
      "command: ${ARGN}\nstdout:\n${stdout}\nstderr:\n${stderr}"
    )
  endif()
endfunction()

function(expect_success_with_output expected_text)
  execute_process(
    COMMAND "${MANUMESH_EXECUTABLE}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )

  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Expected success for: ${ARGN}\nstdout:\n${stdout}\nstderr:\n${stderr}")
  endif()

  set(transcript "${stdout}\n${stderr}")
  string(FIND "${transcript}" "${expected_text}" expected_index)
  if(expected_index EQUAL -1)
    message(FATAL_ERROR
      "Resolved configuration was not reported. Expected: ${expected_text}\n"
      "command: ${ARGN}\nstdout:\n${stdout}\nstderr:\n${stderr}"
    )
  endif()
endfunction()

function(expect_success_without_output forbidden_text)
  execute_process(
    COMMAND "${MANUMESH_EXECUTABLE}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )

  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Expected success for: ${ARGN}\nstdout:\n${stdout}\nstderr:\n${stderr}")
  endif()

  set(transcript "${stdout}\n${stderr}")
  string(FIND "${transcript}" "${forbidden_text}" forbidden_index)
  if(NOT forbidden_index EQUAL -1)
    message(FATAL_ERROR
      "Output contains removed contract text: ${forbidden_text}\n"
      "command: ${ARGN}\nstdout:\n${stdout}\nstderr:\n${stderr}"
    )
  endif()
endfunction()

function(require_performance_csv path command)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Performance CSV was not created: ${path}")
  endif()

  file(STRINGS "${path}" lines)
  list(LENGTH lines line_count)
  if(NOT line_count EQUAL 2)
    message(FATAL_ERROR "Performance CSV must contain one header and one row: ${path}")
  endif()

  list(GET lines 0 header)
  list(GET lines 1 row)
  set(expected_header
    "schema_version,command,backend,threads_requested,input_vertices,input_faces,output_vertices,output_faces,source_bytes,triangles,partitions,memory_mib,io_buffer_mib,partition_triangles,load_ms,feature_detect_ms,simplify_ms,save_ms,postprocess_ms,operation_ms,total_ms"
  )
  if(NOT "${header}" STREQUAL "${expected_header}")
    message(FATAL_ERROR "Unexpected performance CSV schema in ${path}: ${header}")
  endif()
  string(FIND "${row}" "1,${command}," command_index)
  if(NOT command_index EQUAL 0)
    message(FATAL_ERROR "Unexpected performance CSV command row in ${path}: ${row}")
  endif()
  string(REGEX MATCH ",[0-9][0-9.eE+.-]*$" total_field "${row}")
  if("${total_field}" STREQUAL "")
    message(FATAL_ERROR "Performance CSV must end in a numeric total_ms: ${row}")
  endif()
endfunction()

expect_failure(
  "ratio-sweep derives each target from --ratios"
  ratio-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/ratio_conflict" --ratio 0.5
)
expect_failure(
  "face-sweep derives each target from --faces"
  face-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/face_conflict" --target-faces 80
)
expect_failure(
  "sweep derives line-quadric weights from --weights"
  sweep "${INPUT_MESH}" "${OUTPUT_DIR}/weight_conflict" --line-weight 0.01
)
expect_failure(
  "standard QEM does not use line-quadric weights"
  sweep "${INPUT_MESH}" "${OUTPUT_DIR}/standard_weight_conflict" --method standard --weights "0,0.01"
)
expect_failure(
  "--weights contains an empty value"
  sweep "${INPUT_MESH}" "${OUTPUT_DIR}/empty_weights" --weights "0.01,"
)
expect_failure(
  "--ratios values must be strictly between 0 and 1"
  ratio-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/invalid_ratios" --ratios "0,2"
)
expect_failure(
  "--faces values must be positive"
  face-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/invalid_faces" --faces "0,80"
)
expect_failure(
  "--weights contains duplicate value"
  sweep "${INPUT_MESH}" "${OUTPUT_DIR}/duplicate_weights" --weights "0.01,1e-2"
)
expect_failure(
  "--ratios contains duplicate value"
  ratio-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/duplicate_ratios" --ratios "0.75,0.750"
)
expect_failure(
  "--faces contains duplicate value"
  face-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/duplicate_faces" --faces "80,80"
)
expect_failure(
  "--samples must be a positive integer"
  ratio-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/invalid_samples" --ratios 0.75 --samples 0
)
expect_failure(
  "--metrics-csv must not overwrite the simplified output"
  simplify "${INPUT_MESH}" "${OUTPUT_DIR}/metrics_alias_output.stl"
    --metrics-csv "${OUTPUT_DIR}/./metrics_alias_output.stl"
)
expect_failure(
  "--csv must not overwrite the input mesh"
  feature-report "${INPUT_MESH}" --csv "${INPUT_MESH}"
)
expect_failure(
  "--csv must not overwrite the label CSV"
  feature-benchmark
    "${INPUT_MESH}"
    "${PROJECT_SOURCE_DIR}/tests/data/feature_labels/coaxial_hole_plate_inner_top_edges.csv"
    --csv "${PROJECT_SOURCE_DIR}/tests/data/feature_labels/coaxial_hole_plate_inner_top_edges.csv"
)
expect_failure(
  "--performance-csv must not overwrite the --csv output"
  feature-report "${INPUT_MESH}" --csv "${OUTPUT_DIR}/feature_business.csv"
    --performance-csv "${OUTPUT_DIR}/./feature_business.csv"
)
expect_failure(
  "--performance-csv must not overwrite the simplified output"
  simplify "${INPUT_MESH}" "${OUTPUT_DIR}/performance_alias_output.stl"
    --performance-csv "${OUTPUT_DIR}/./performance_alias_output.stl"
)

expect_failure(
  "simplify requires exactly input.stl output.stl"
  simplify "${INPUT_MESH}" "${OUTPUT_DIR}/extra_path.stl" ignored.stl
)
expect_failure(
  "compare requires exactly original.stl simplified.stl"
  compare "${INPUT_MESH}" "${INPUT_MESH}" ignored.stl
)
expect_failure(
  "feature-report requires exactly one input.stl path"
  feature-report "${INPUT_MESH}" ignored.stl
)
expect_failure(
  "feature-benchmark requires exactly input.stl labels.csv"
  feature-benchmark "${INPUT_MESH}" ignored.csv extra.csv
)
expect_failure(
  "feature-compare requires exactly original.stl simplified.stl"
  feature-compare "${INPUT_MESH}" "${INPUT_MESH}" ignored.stl
)
expect_failure(
  "generate does not accept positional paths"
  generate ignored.stl --out "${OUTPUT_DIR}/unexpected.stl"
)
expect_failure(
  "summarize-metrics accepts at most output_root and summary.csv"
  summarize-metrics "${OUTPUT_DIR}" one.csv extra.csv
)
expect_failure("demo does not accept positional paths" demo ignored.stl)
expect_failure("validate-features does not accept positional paths" validate-features ignored.stl)
expect_failure("validate-external does not accept positional paths" validate-external ignored.stl)
expect_failure(
  "Unknown --profile"
  feature-report "${OUTPUT_DIR}/missing_profile_input.obj" --profile unsupported
)
expect_failure(
  "Unknown --profile"
  feature-report "${INPUT_MESH}" --profile smooth
)
expect_failure(
  "Unknown --profile"
  feature-benchmark "${OUTPUT_DIR}/missing_profile_input.obj" "${OUTPUT_DIR}/missing_profile_labels.csv" --profile unsupported
)
expect_failure(
  "Unknown --profile"
  feature-compare "${OUTPUT_DIR}/missing_profile_original.stl" "${OUTPUT_DIR}/missing_profile_simplified.stl" --profile unsupported
)
expect_success_without_output(
  "--profile smooth"
  --help
)

expect_success_with_output(
  "target: ratio=0.75"
  ratio-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/resolved_ratio" --ratios 0.75 --samples 16 --print-resolved-config
)
expect_success_with_output(
  "target: faces=80"
  face-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/resolved_faces" --faces 80 --samples 16 --print-resolved-config
)
expect_success_with_output(
  "line_weight=0.01"
  sweep "${INPUT_MESH}" "${OUTPUT_DIR}/resolved_weight" --weights 0.01 --samples 16 --print-resolved-config
)
expect_success_with_output(
  "line_quadrics: enabled=off"
  ratio-sweep "${INPUT_MESH}" "${OUTPUT_DIR}/resolved_zero_weight" --method line --line-weight 0 --ratios 0.75 --samples 16 --print-resolved-config
)

set(performance_feature_report "${OUTPUT_DIR}/feature_report_performance.csv")
expect_success_with_output(
  "performance command=feature-report"
  feature-report "${INPUT_MESH}" --performance-csv "${performance_feature_report}"
)
require_performance_csv("${performance_feature_report}" "feature-report")

get_filename_component(manumesh_source_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(feature_fixture "${manumesh_source_dir}/tests/data/feature_fixtures/coaxial_hole_plate.obj")
set(feature_labels "${manumesh_source_dir}/tests/data/feature_labels/coaxial_hole_plate_inner_top_edges.csv")
set(performance_feature_benchmark "${OUTPUT_DIR}/feature_benchmark_performance.csv")
expect_success_with_output(
  "performance command=feature-benchmark"
  feature-benchmark "${feature_fixture}" "${feature_labels}"
    --performance-csv "${performance_feature_benchmark}"
)
require_performance_csv("${performance_feature_benchmark}" "feature-benchmark")

set(performance_feature_compare "${OUTPUT_DIR}/feature_compare_performance.csv")
expect_success_with_output(
  "performance command=feature-compare"
  feature-compare "${feature_fixture}" "${feature_fixture}"
    --performance-csv "${performance_feature_compare}"
)
require_performance_csv("${performance_feature_compare}" "feature-compare")

set(performance_simplify_output "${OUTPUT_DIR}/performance_simplify.stl")
set(performance_simplify_csv "${OUTPUT_DIR}/simplify_performance.csv")
expect_success_with_output(
  "performance command=simplify"
  simplify "${INPUT_MESH}" "${performance_simplify_output}" --ratio 0.75 --samples 16
    --performance-csv "${performance_simplify_csv}"
)
require_performance_csv("${performance_simplify_csv}" "simplify")
