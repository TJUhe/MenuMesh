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
  "smoothCurvatureScaleCount must be in"
  feature-report "${OUTPUT_DIR}/missing_range_input.obj" --smooth-curvature-scales 0
)
expect_failure(
  "Unknown --profile"
  feature-benchmark "${OUTPUT_DIR}/missing_profile_input.obj" "${OUTPUT_DIR}/missing_profile_labels.csv" --profile unsupported
)
expect_failure(
  "Unknown --profile"
  feature-compare "${OUTPUT_DIR}/missing_profile_original.stl" "${OUTPUT_DIR}/missing_profile_simplified.stl" --profile unsupported
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
