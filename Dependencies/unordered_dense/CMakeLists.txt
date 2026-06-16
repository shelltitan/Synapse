cmake_minimum_required(
    VERSION 4.2.0
    FATAL_ERROR
)

project(external-unordered_dense
    VERSION 4.8.1
    DESCRIPTION "A fast & densely stored hashmap and hashset based on robin-hood backward shift deletion"
    HOMEPAGE_URL "https://github.com/martinus/unordered_dense"
)
include(ExternalProject)

list(APPEND CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}"
    "-DCMAKE_TOOLCHAIN_FILE:PATH=${CMAKE_TOOLCHAIN_FILE}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
    "-DCMAKE_CXX_STANDARD_REQUIRED=${CMAKE_CXX_STANDARD_REQUIRED}"
    "-DCMAKE_CXX_EXTENSIONS=${CMAKE_CXX_EXTENSIONS}"
    "-DCMAKE_CXX_SCAN_FOR_MODULES=${CMAKE_CXX_SCAN_FOR_MODULES}"
    "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=${CMAKE_INTERPROCEDURAL_OPTIMIZATION}"
    "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
    "-DANKERL_ENABLE_MODULES=${CMAKE_CXX_SCAN_FOR_MODULES}"
)

message(STATUS "Preparing external project \"unordered_dense\" with args:")
foreach(CMAKE_ARG ${CMAKE_ARGS})
    message(STATUS "-- ${CMAKE_ARG}")
endforeach()

ExternalProject_add(
    unordered_dense
    GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
    GIT_TAG v4.8.1
    PREFIX unordered_dense
    CMAKE_ARGS ${CMAKE_ARGS}
)