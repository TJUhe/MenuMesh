cmake_minimum_required(VERSION 3.20)

project(line_quadrics_qem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Eigen3 3.3 QUIET NO_MODULE)

if(NOT Eigen3_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
  )
  FetchContent_MakeAvailable(eigen)
endif()

add_executable(linequadrics
  src/main.cpp
  src/Mesh.cpp
  src/MeshGenerators.cpp
  src/Metrics.cpp
  src/QEMSimplifier.cpp
)

target_include_directories(linequadrics PRIVATE src)

if(TARGET Eigen3::Eigen)
  target_link_libraries(linequadrics PRIVATE Eigen3::Eigen)
elseif(DEFINED eigen_SOURCE_DIR)
  target_include_directories(linequadrics PRIVATE "${eigen_SOURCE_DIR}")
else()
  message(FATAL_ERROR "Eigen3 was not found and FetchContent did not provide eigen_SOURCE_DIR.")
endif()

if(MSVC)
  target_compile_options(linequadrics PRIVATE /W4 /permissive- /utf-8 /external:anglebrackets /external:W0)
else()
  target_compile_options(linequadrics PRIVATE -Wall -Wextra -Wpedantic)
endif()
