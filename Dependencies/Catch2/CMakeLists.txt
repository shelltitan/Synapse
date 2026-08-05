cmake_minimum_required(
    VERSION 4.2.0
    FATAL_ERROR
)

project(external-catch2
    VERSION 3.15.3
    DESCRIPTION "A modern, C++-native, test framework for unit-tests, TDD and BDD - using C++14, C++17 and later"
    HOMEPAGE_URL "https://github.com/catchorg/Catch2"
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
	"-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
)

message(STATUS "Preparing external project \"Catch2\" with args:")
foreach(CMAKE_ARG ${CMAKE_ARGS})
	message(STATUS "-- ${CMAKE_ARG}")
endforeach()

ExternalProject_add(
	catch2
	GIT_REPOSITORY https://github.com/catchorg/Catch2.git
	GIT_TAG v3.15.3
	GIT_SHALLOW TRUE
	PREFIX catch2
	CMAKE_ARGS "${CMAKE_ARGS}"
)
