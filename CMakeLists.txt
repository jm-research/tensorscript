cmake_minimum_required(VERSION 3.20)
project(
  tensorscript
  VERSION 0.0.1
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE "Debug")
set(project_version "${${PROJECT_NAME}_VERSION}")

message(STATUS "Project '${PROJECT_NAME}', version: '${project_version}'")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # for debug of stl structure while using clang compile
  add_compile_options(-fstandalone-debug)
endif()

# CMake helpers:
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

include(CTest)
enable_testing()

include_directories(include)

set(LIB_NAME ${PROJECT_NAME})

# apt install llvm-15-dev libclang-15-dev
set(LLVM_DIR /usr/lib/llvm-15/lib/cmake/llvm)
# manually install
# set(LLVM_DIR /usr/local/lib/cmake/llvm)

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

file(GLOB_RECURSE LIB_PATH
    ./lib/*.cc
  )

add_library(${LIB_NAME} SHARED ${LIB_PATH})
link_directories(${LLVM_LIBRARY_DIRS})
target_link_libraries(${LIB_NAME} ${LLVM_LIBRARIES})