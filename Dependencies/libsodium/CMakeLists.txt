cmake_minimum_required(
	VERSION 4.2.0
	FATAL_ERROR
)

project(external-libsodium
    VERSION 1.0.23
    DESCRIPTION "A modern, portable, easy to use crypto library."
    HOMEPAGE_URL "https://github.com/shelltitan/libsodium-cmake.git"
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
        "-DBUILD_SHARED_LIBS=OFF"
        "-DSODIUM_ENABLE_SSP=OFF"
)

message(STATUS "Preparing external project \"libsodium\" with args:")
foreach(CMAKE_ARG ${CMAKE_ARGS})
	message(STATUS "-- ${CMAKE_ARG}")
endforeach()

ExternalProject_add(
		libsodium
		GIT_REPOSITORY https://github.com/shelltitan/libsodium-cmake.git
		GIT_TAG main
		PREFIX libsodium
		CMAKE_ARGS "${CMAKE_ARGS}"
)