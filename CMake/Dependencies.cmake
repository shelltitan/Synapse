include(FetchContent)

if(NOT tomlplusplus_FOUND)
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG master  # Replace this with a particular git tag or git hash
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    message(STATUS "Using tomlplusplus via FetchContent")
    FetchContent_MakeAvailable(tomlplusplus)
endif()

find_package(libassert QUIET)
if(libassert_FOUND)
    message(STATUS "Using libassert via find_package")
endif()

if(NOT libassert_FOUND)
	FetchContent_Declare(
		libassert
		GIT_REPOSITORY https://github.com/jeremy-rifkin/libassert.git
		GIT_TAG main
	)
	FetchContent_MakeAvailable(libassert)
endif()

if (BUILD_TESTS)
    find_package(Catch2 3 CONFIG QUIET)
    if(Catch2_FOUND)
        message(STATUS "Using Catch2 via find_package")
    endif()

    if(NOT Catch2_FOUND)
        message(STATUS "Fetching Catch2 via FetchContent")
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.12.0
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        FetchContent_MakeAvailable(Catch2)
        # Make the Catch.cmake helper visible when Catch2 is fetched
        list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
    endif()
endif()
