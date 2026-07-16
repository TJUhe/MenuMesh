if(NOT DEFINED MANUMESH_SOURCE_DIR)
  message(FATAL_ERROR "MANUMESH_SOURCE_DIR is required")
endif()

file(GLOB_RECURSE MANUMESH_SRC_DOXYGEN_FILES
  "${MANUMESH_SOURCE_DIR}/src/*.c"
  "${MANUMESH_SOURCE_DIR}/src/*.cpp"
  "${MANUMESH_SOURCE_DIR}/src/*.h"
  "${MANUMESH_SOURCE_DIR}/src/*.hpp"
)

set(MANUMESH_SRC_DOXYGEN_ERRORS)
foreach(source_file IN LISTS MANUMESH_SRC_DOXYGEN_FILES)
  file(RELATIVE_PATH relative_file "${MANUMESH_SOURCE_DIR}" "${source_file}")
  string(REPLACE "\\" "/" relative_file "${relative_file}")
  file(READ "${source_file}" source_text)

  string(REGEX MATCH "^/\\*\\*" has_block_header "${source_text}")
  string(FIND "${source_text}" "*/" header_end)
  if(NOT has_block_header OR header_end LESS 0)
    list(APPEND MANUMESH_SRC_DOXYGEN_ERRORS
      "${relative_file}: file must begin with a /** ... */ Doxygen header"
    )
  else()
    math(EXPR header_length "${header_end} + 2")
    string(SUBSTRING "${source_text}" 0 ${header_length} file_header)
    foreach(required_tag
        "@file ${relative_file}"
        "@brief"
        "@ingroup")
      string(FIND "${file_header}" "${required_tag}" tag_index)
      if(tag_index LESS 0)
        list(APPEND MANUMESH_SRC_DOXYGEN_ERRORS
          "${relative_file}: file header is missing ${required_tag}"
        )
      endif()
    endforeach()
  endif()

  string(REGEX MATCH "(^|\n)[ \t]*///" has_triple_slash "${source_text}")
  string(REGEX MATCH "(^|\n)[ \t]*//!" has_bang_line "${source_text}")
  string(FIND "${source_text}" "///<" has_trailing_slash)
  string(FIND "${source_text}" "//!<" has_trailing_bang)
  if(has_triple_slash OR has_bang_line OR
     has_trailing_slash GREATER -1 OR has_trailing_bang GREATER -1)
    list(APPEND MANUMESH_SRC_DOXYGEN_ERRORS
      "${relative_file}: use /** ... */ or /**< ... */ for Doxygen comments; line-style forms are forbidden"
    )
  endif()
endforeach()

if(MANUMESH_SRC_DOXYGEN_ERRORS)
  list(JOIN MANUMESH_SRC_DOXYGEN_ERRORS "\n  " formatted_errors)
  message(FATAL_ERROR "src Doxygen convention check failed:\n  ${formatted_errors}")
endif()

list(LENGTH MANUMESH_SRC_DOXYGEN_FILES checked_file_count)
message(STATUS
  "Validated block-style Doxygen metadata in ${checked_file_count} src files"
)
