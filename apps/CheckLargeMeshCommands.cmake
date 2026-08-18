if(NOT DEFINED MANUMESH_EXECUTABLE OR "${MANUMESH_EXECUTABLE}" STREQUAL "" OR
   NOT EXISTS "${MANUMESH_EXECUTABLE}")
  message(FATAL_ERROR "MANUMESH_EXECUTABLE must name an existing executable.")
endif()

if(NOT DEFINED OUTPUT_DIR OR "${OUTPUT_DIR}" STREQUAL "")
  message(FATAL_ERROR "OUTPUT_DIR is required.")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(input_stl "${OUTPUT_DIR}/small_plane.stl")
set(dataset "${OUTPUT_DIR}/small_plane.mmpd")

function(expect_success output_variable)
  execute_process(
    COMMAND "${MANUMESH_EXECUTABLE}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Expected success for: ${ARGN}\nstdout:\n${stdout}\nstderr:\n${stderr}")
  endif()
  set(${output_variable} "${stdout}\n${stderr}" PARENT_SCOPE)
endfunction()

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
      "Expected diagnostic: ${expected_text}\ncommand: ${ARGN}\nstdout:\n${stdout}\nstderr:\n${stderr}"
    )
  endif()
endfunction()

function(require_text transcript expected_text)
  string(FIND "${transcript}" "${expected_text}" expected_index)
  if(expected_index EQUAL -1)
    message(FATAL_ERROR "Expected output text: ${expected_text}\nactual:\n${transcript}")
  endif()
endfunction()

expect_success(generate_output generate --type plane --n 2 --out "${input_stl}")
if(NOT EXISTS "${input_stl}")
  message(FATAL_ERROR "generate did not create the binary STL fixture.")
endif()

expect_success(
  import_output
  large-import "${input_stl}" "${dataset}"
  --partition-triangles 2 --memory-mib 8 --io-buffer-mib 1
)
require_text("${import_output}" "large_import triangles=8 partitions=4")
if(NOT EXISTS "${dataset}")
  message(FATAL_ERROR "large-import did not create the partitioned dataset.")
endif()

expect_success(
  validate_output
  large-validate "${dataset}" --memory-mib 8 --io-buffer-mib 1
)
require_text("${validate_output}" "large_validate triangles=8 partitions=4")
require_text("${validate_output}" "area=4")
require_text("${validate_output}" "degenerate=0")
require_text("${validate_output}" "bounds_min=-1,-1,0 bounds_max=1,1,0")
require_text("${validate_output}" "count_consistency=ok")
require_text("${validate_output}" "checksum_consistency=ok")

expect_failure(
  "--partition-triangles must be greater than zero"
  large-import "${input_stl}" "${OUTPUT_DIR}/invalid_zero.mmpd" --partition-triangles 0
)
expect_failure(
  "--partition-triangles must be a positive integer"
  large-import "${input_stl}" "${OUTPUT_DIR}/invalid_text.mmpd" --partition-triangles 12x
)
expect_failure(
  "--partition-triangles exceeds the supported 32-bit local index range"
  large-import "${input_stl}" "${OUTPUT_DIR}/invalid_wide.mmpd" --partition-triangles 4294967296
)
expect_failure(
  "--memory-mib overflows the supported byte range"
  large-validate "${dataset}" --memory-mib 17592186044416 --io-buffer-mib 1
)
expect_failure(
  "--io-buffer-mib must be less than or equal to --memory-mib"
  large-validate "${dataset}" --memory-mib 1 --io-buffer-mib 2
)
expect_failure(
  "Option --partition-triangles is not valid for the 'large-validate' command"
  large-validate "${dataset}" --partition-triangles 2
)
expect_failure(
  "large-import requires exactly input.stl output.mmpd"
  large-import "${input_stl}"
)
expect_failure(
  "large-import input and output must not refer to the same file"
  large-import "${input_stl}" "${OUTPUT_DIR}/./small_plane.stl"
)
