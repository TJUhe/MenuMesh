cmake_minimum_required(VERSION 3.18)

project(line_quadrics_qem_sdk_consumer
  LANGUAGES C CXX
)

set(LQ_SDK_ROOT "" CACHE PATH "Installed line_quadrics_qem SDK root")
if(NOT LQ_SDK_ROOT)
  message(FATAL_ERROR "Set LQ_SDK_ROOT to an installed line_quadrics_qem SDK root.")
endif()

set(LQ_SDK_INCLUDE_DIR "${LQ_SDK_ROOT}/include")
set(LQ_SDK_EIGEN_INCLUDE_DIR
  "${LQ_SDK_ROOT}/share/line_quadrics_qem/thirdParty/eigen/include"
)
set(LQ_SDK_BIN_DIR "${LQ_SDK_ROOT}/bin")

if(NOT EXISTS "${LQ_SDK_INCLUDE_DIR}/line_quadrics_qem/api/CApi.h")
  message(FATAL_ERROR "SDK C ABI header was not found under '${LQ_SDK_INCLUDE_DIR}'.")
endif()
if(NOT EXISTS "${LQ_SDK_INCLUDE_DIR}/line_quadrics_qem/core/Mesh.h")
  message(FATAL_ERROR "SDK C++ headers were not found under '${LQ_SDK_INCLUDE_DIR}'.")
endif()

find_library(LQ_SDK_LIBRARY
  NAMES line_quadrics_qem libline_quadrics_qem
  PATHS "${LQ_SDK_ROOT}/lib"
  NO_DEFAULT_PATH
)
if(NOT LQ_SDK_LIBRARY)
  message(FATAL_ERROR "line_quadrics_qem SDK library was not found under '${LQ_SDK_ROOT}/lib'.")
endif()

function(lq_sdk_copy_runtime target)
  if(EXISTS "${LQ_SDK_BIN_DIR}")
    file(GLOB LQ_SDK_RUNTIME_FILES CONFIGURE_DEPENDS
      "${LQ_SDK_BIN_DIR}/*.dll"
      "${LQ_SDK_BIN_DIR}/*.so"
      "${LQ_SDK_BIN_DIR}/*.dylib"
    )
    foreach(runtime_file IN LISTS LQ_SDK_RUNTIME_FILES)
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${runtime_file}"
          "$<TARGET_FILE_DIR:${target}>"
        VERBATIM
      )
    endforeach()
  endif()
endfunction()

add_executable(sdk_cpp_simplify sdk_cpp_simplify.cpp)
target_compile_features(sdk_cpp_simplify PRIVATE cxx_std_17)
target_include_directories(sdk_cpp_simplify
  PRIVATE
    "${LQ_SDK_INCLUDE_DIR}"
    "${LQ_SDK_EIGEN_INCLUDE_DIR}"
)
target_link_libraries(sdk_cpp_simplify PRIVATE "${LQ_SDK_LIBRARY}")
lq_sdk_copy_runtime(sdk_cpp_simplify)

add_executable(sdk_c_abi_basic sdk_c_abi_basic.c)
set_target_properties(sdk_c_abi_basic PROPERTIES
  LINKER_LANGUAGE CXX
)
target_include_directories(sdk_c_abi_basic PRIVATE "${LQ_SDK_INCLUDE_DIR}")
target_link_libraries(sdk_c_abi_basic PRIVATE "${LQ_SDK_LIBRARY}")
lq_sdk_copy_runtime(sdk_c_abi_basic)

enable_testing()
add_test(NAME sdk_cpp_simplify COMMAND sdk_cpp_simplify)
add_test(NAME sdk_c_abi_basic COMMAND sdk_c_abi_basic)
