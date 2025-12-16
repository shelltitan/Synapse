find_package(cpptrace)
if(cpptrace_FOUND)
    message(STATUS "Using cpptrace via find_package")
endif()

find_package(unordered_dense CONFIG QUIET)
if(unordered_dense_FOUND)
    message(STATUS "Using unordered_dense via find_package")
endif()

if(NOT unordered_dense_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG main  # Replace this with a particular git tag or git hash
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    message(STATUS "Using unordered_dense via FetchContent")
    FetchContent_MakeAvailable(unordered_dense)
	install(
		TARGETS unordered_dense 
		EXPORT unordered_dense_Targets
	)
endif()

if (BUILD_TESTS)
    find_package(Catch2 3 CONFIG QUIET)
    if(Catch2_FOUND)
        message(STATUS "Using Catch2 via find_package")
    endif()

    if(NOT Catch2_FOUND)
        message(STATUS "Fetching Catch2 via FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.9.1 # <- replace with the version you want; avoid 'devel' in C
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON)  # don't update on every configure
        FetchContent_MakeAvailable(Catch2)
        # Make the Catch.cmake helper visible when Catch2 is fetched
        list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
    endif()
endif()

find_package(spdlog QUIET)
if(spdlog_FOUND)
    message(STATUS "Using spdlog via find_package")
endif()

if(NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG main  # Replace this with a particular git tag or git hash
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    message(STATUS "Using spdlog via FetchContent")
    FetchContent_MakeAvailable(spdlog)
endif()
set_target_properties(spdlog::spdlog PROPERTIES IMPORTED_GLOBAL TRUE)

option(PROFILING_ENABLED "" ON)
find_package(tracy CONFIG QUIET)
if(tracy_FOUND)
    message(STATUS "Using tracy via find_package")
endif()

if(NOT tracy_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    message(STATUS "Using tracy via FetchContent")
    FetchContent_MakeAvailable(tracy)
endif()

find_package(tomlplusplus QUIET)
if(tomlplusplus_FOUND)
    message(STATUS "Using tomlplusplus via find_package")
	set_target_properties(tomlplusplus::tomlplusplus PROPERTIES IMPORTED_GLOBAL TRUE)
endif()

if(NOT tomlplusplus_FOUND)
    include(FetchContent)
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
    include(FetchContent)
	FetchContent_Declare(
		libassert
		GIT_REPOSITORY https://github.com/jeremy-rifkin/libassert.git
		GIT_TAG main
	)
	FetchContent_MakeAvailable(libassert)
endif()

set(sodium_USE_STATIC_LIBS ON)
find_package(libsodium QUIET)
if(libsodium_FOUND)
    message(STATUS "Using libsodium via find_package")
endif()