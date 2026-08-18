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

execute_process(
    COMMAND "${MANUMESH_EXECUTABLE}" sweep
    "${INPUT_MESH}" "${OUTPUT_DIR}"
    --method standard
    --weights 0
    --ratio 0.90
    --samples 16
  RESULT_VARIABLE sweep_result
  OUTPUT_VARIABLE sweep_stdout
  ERROR_VARIABLE sweep_stderr
)

if(NOT "${sweep_result}" STREQUAL "0")
  message(FATAL_ERROR
    "sweep failed with exit code ${sweep_result}\n"
    "stdout:\n${sweep_stdout}\n"
    "stderr:\n${sweep_stderr}")
endif()

set(metrics_path "${OUTPUT_DIR}/metrics.csv")
if(NOT EXISTS "${metrics_path}")
  message(FATAL_ERROR "sweep did not create ${metrics_path}")
endif()

file(STRINGS "${metrics_path}" metrics_rows)
list(LENGTH metrics_rows metrics_row_count)
if(NOT "${metrics_row_count}" STREQUAL "2")
  message(FATAL_ERROR
    "Expected one metrics row for --weights 0.01; got ${metrics_row_count} rows.")
endif()

list(GET metrics_rows 0 metrics_header)
if(NOT "${metrics_header}" MATCHES "^method,line_weight,weight_mode,")
  message(FATAL_ERROR "Unexpected metrics header: ${metrics_header}")
endif()

list(GET metrics_rows 1 metrics_row)
if(NOT "${metrics_row}" MATCHES "^standard,")
  message(FATAL_ERROR
    "Expected standard-QEM metrics row for --method standard; got: ${metrics_row}")
endif()
