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

function(run_sweep description)
  execute_process(
    COMMAND "${MANUMESH_EXECUTABLE}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR
      "${description} failed with exit code ${result}\nstdout:\n${stdout}\nstderr:\n${stderr}")
  endif()
endfunction()

set(collision_dir "${OUTPUT_DIR}/distinct_names")
run_sweep(
  "close line weights"
  sweep "${INPUT_MESH}" "${collision_dir}"
  --method line --weights "0.01,0.014" --ratio 0.90 --samples 16
)
file(GLOB collision_outputs "${collision_dir}/*.stl")
list(LENGTH collision_outputs collision_output_count)
if(NOT "${collision_output_count}" STREQUAL "2")
  message(FATAL_ERROR "Expected two distinct STL outputs for close weights; got ${collision_output_count}.")
endif()
file(STRINGS "${collision_dir}/metrics.csv" collision_rows)
list(LENGTH collision_rows collision_row_count)
if(NOT "${collision_row_count}" STREQUAL "3")
  message(FATAL_ERROR "Expected two metrics rows for close weights; got ${collision_row_count} total rows.")
endif()

set(ratio_dir "${OUTPUT_DIR}/adaptive_ratio")
run_sweep(
  "adaptive ratio sweep"
  ratio-sweep "${INPUT_MESH}" "${ratio_dir}"
  --method line --adaptive-scale --adaptive-base-line-weight 0.02 --ratios 0.75 --samples 16
)
file(STRINGS "${ratio_dir}/metrics.csv" ratio_rows)
list(GET ratio_rows 1 ratio_row)
if(NOT "${ratio_row}" MATCHES "^line,0.02,")
  message(FATAL_ERROR "Adaptive ratio sweep did not report its active base line weight: ${ratio_row}")
endif()
file(GLOB ratio_outputs "${ratio_dir}/*.stl")
list(GET ratio_outputs 0 ratio_output)
if(NOT "${ratio_output}" MATCHES "_w_0d02\\.stl$")
  message(FATAL_ERROR "Adaptive ratio sweep filename omitted its active base line weight: ${ratio_output}")
endif()

set(face_dir "${OUTPUT_DIR}/adaptive_face")
run_sweep(
  "adaptive face sweep"
  face-sweep "${INPUT_MESH}" "${face_dir}"
  --method line --adaptive-scale --adaptive-base-line-weight 0.02 --faces 80 --samples 16
)
file(STRINGS "${face_dir}/metrics.csv" face_rows)
list(GET face_rows 1 face_row)
if(NOT "${face_row}" MATCHES "^line,0.02,")
  message(FATAL_ERROR "Adaptive face sweep did not report its active base line weight: ${face_row}")
endif()
file(GLOB face_outputs "${face_dir}/*.stl")
list(GET face_outputs 0 face_output)
if(NOT "${face_output}" MATCHES "_w_0d02\\.stl$")
  message(FATAL_ERROR "Adaptive face sweep filename omitted its active base line weight: ${face_output}")
endif()

set(zero_dir "${OUTPUT_DIR}/zero_line_weight")
run_sweep(
  "zero line weight is reported as standard"
  ratio-sweep "${INPUT_MESH}" "${zero_dir}"
  --method line --line-weight 0 --ratios 0.75 --samples 16
)
file(GLOB zero_outputs "${zero_dir}/*.stl")
list(GET zero_outputs 0 zero_output)
if(NOT "${zero_output}" MATCHES "standard_r_0d75_w_0\\.stl$")
  message(FATAL_ERROR "Zero line weight was not normalized to a standard output: ${zero_output}")
endif()
