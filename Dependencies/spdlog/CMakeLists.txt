cmake_minimum_required(
    VERSION 4.2.0
    FATAL_ERROR
)

project(external-spdlog
    VERSION 1.17.0
    DESCRIPTION "An efficient C++ logging library."
    HOMEPAGE_URL "https://github.com/gabime/spdlog"
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
)

message(STATUS "Preparing external project \"spdlog\" with args:")
foreach(CMAKE_ARG ${CMAKE_ARGS})
    message(STATUS "-- ${CMAKE_ARG}")
endforeach()

ExternalProject_add(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
    PREFIX spdlog
    CMAKE_ARGS "${CMAKE_ARGS}"
)