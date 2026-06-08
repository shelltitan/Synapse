option(ENABLE_CLANG_TIDY "Enable clang-tidy for project targets" ON)
option(ENABLE_CLANG_TIDY_IN_RELEASE "Enable clang-tidy for Release builds" OFF)

set(CLANG_TIDY_ENABLED OFF)
set(CLANG_TIDY_COMMAND "")

if (ENABLE_CLANG_TIDY)
    if (CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT ENABLE_CLANG_TIDY_IN_RELEASE)
        message(STATUS "clang-tidy disabled for Release build type")
    else ()
        find_program(CLANGTIDY clang-tidy)
        if (CLANGTIDY)
            message(STATUS "Using clang-tidy")
            set(CLANG_TIDY_ARGS "${CLANGTIDY}")

            if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
                # clang-tidy parses RTM with Clang's frontend even when the real compiler is cl.exe.
                # Keep SIMD enabled for the build, but use RTM's scalar API while analyzing.
                list(APPEND CLANG_TIDY_ARGS "--extra-arg-before=-DRTM_NO_INTRINSICS")
                list(APPEND CLANG_TIDY_ARGS "--extra-arg-before=-Wno-unused-command-line-argument")
            endif ()

            set(CLANG_TIDY_FLAG_SOURCES)
            if (CMAKE_CXX_FLAGS_INIT)
                list(APPEND CLANG_TIDY_FLAG_SOURCES "${CMAKE_CXX_FLAGS_INIT}")
            endif ()
            if (CMAKE_CXX_FLAGS)
                list(APPEND CLANG_TIDY_FLAG_SOURCES "${CMAKE_CXX_FLAGS}")
            endif ()

            if (CMAKE_BUILD_TYPE)
                string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
                if (DEFINED CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER})
                    list(APPEND CLANG_TIDY_FLAG_SOURCES "${CMAKE_CXX_FLAGS_${BUILD_TYPE_UPPER}}")
                endif ()
            elseif (CMAKE_CONFIGURATION_TYPES)
                foreach (CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
                    string(TOUPPER "${CONFIG}" CONFIG_UPPER)
                    if (DEFINED CMAKE_CXX_FLAGS_${CONFIG_UPPER})
                        list(APPEND CLANG_TIDY_FLAG_SOURCES "${CMAKE_CXX_FLAGS_${CONFIG_UPPER}}")
                    endif ()
                endforeach ()
            endif ()

            foreach (FLAG_SOURCE IN LISTS CLANG_TIDY_FLAG_SOURCES)
                separate_arguments(CLANG_TIDY_PARSED_FLAGS NATIVE_COMMAND "${FLAG_SOURCE}")
                foreach (FLAG IN LISTS CLANG_TIDY_PARSED_FLAGS)
                    if ((CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
                            AND FLAG MATCHES "^/Zc:(preprocessor|externConstexpr|throwingNew)$")
                        continue()
                    endif ()

                    list(APPEND CLANG_TIDY_ARGS "--extra-arg-before=${FLAG}")
                endforeach ()
            endforeach ()

            list(REMOVE_DUPLICATES CLANG_TIDY_ARGS)
            set(CLANG_TIDY_ENABLED ON)
            set(CLANG_TIDY_COMMAND "${CLANG_TIDY_ARGS}")
        else ()
            message(SEND_ERROR "clang-tidy requested but executable not found")
        endif ()
    endif ()
endif ()

function(enable_clang_tidy_for_targets)
    if (NOT CLANG_TIDY_ENABLED)
        return()
    endif ()

    foreach (TARGET_NAME IN LISTS ARGN)
        if (TARGET "${TARGET_NAME}")
            set_property(TARGET "${TARGET_NAME}" PROPERTY CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
        else ()
            message(WARNING "clang-tidy target '${TARGET_NAME}' does not exist in this directory scope")
        endif ()
    endforeach ()
endfunction()

option(USE_SANITIZERS "Enable sanitizers" OFF)
set(SANITIZERS "" CACHE STRING "Comma-separated list of sanitizers to enable (e.g., address,undefined,thread,memory)")

if (USE_SANITIZERS)
    message(STATUS "[Sanitizers] Requested: ${SANITIZERS}")

    string(REPLACE "," ";" SANITIZER_LIST "${SANITIZERS}")

    set(SAN_ADDRESS_FOUND OFF)
    set(SAN_THREAD_FOUND OFF)
    set(SAN_MEMORY_FOUND OFF)

    foreach(s IN LISTS SANITIZER_LIST)
        if(s STREQUAL "address")
            set(SAN_ADDRESS_FOUND ON)
        elseif(s STREQUAL "thread")
            set(SAN_THREAD_FOUND ON)
        elseif(s STREQUAL "memory")
            set(SAN_MEMORY_FOUND ON)
        endif()
    endforeach()

    # Sanity checks
    if(SAN_ADDRESS_FOUND AND (SAN_THREAD_FOUND OR SAN_MEMORY_FOUND))
        message(FATAL_ERROR "[Sanitizers] 'address' cannot be combined with 'thread' or 'memory'")
    elseif(SAN_THREAD_FOUND AND SAN_MEMORY_FOUND)
        message(FATAL_ERROR "[Sanitizers] 'thread' cannot be combined with 'memory'")
    endif()

    # Compiler-specific
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        foreach(sanitizer IN LISTS SANITIZER_LIST)
            if(sanitizer STREQUAL "memory")
                if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64"))
                    message(FATAL_ERROR "[Sanitizers] 'memory' only supported on Linux x86_64")
                endif()
                add_compile_options(-fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer -g)
                add_link_options(-fsanitize=memory -fno-omit-frame-pointer)
            elseif(sanitizer STREQUAL "thread")
                if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
                    message(FATAL_ERROR "[Sanitizers] 'thread' only supported on Linux")
                endif()
                add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
                add_link_options(-fsanitize=thread -fno-omit-frame-pointer)
            else()
                add_compile_options(-fsanitize=${sanitizer} -fno-omit-frame-pointer -g)
                add_link_options(-fsanitize=${sanitizer} -fno-omit-frame-pointer)
            endif()
        endforeach()

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC" OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        if("address" IN_LIST SANITIZER_LIST)
            message(STATUS "[Sanitizers] Enabling MSVC AddressSanitizer")
            add_compile_options(/fsanitize=address /Zi /Od)
            add_link_options(/INCREMENTAL:NO /fsanitize=address)
        else()
            message(FATAL_ERROR "[Sanitizers] Only 'address' is supported for MSVC/Clang-CL")
        endif()
    endif()
endif()

set(CMAKE_NO_SYSTEM_FROM_IMPORTED OFF)

function(enable_strict_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(
            "${target}"
            PRIVATE
            "SHELL:-Weverything"
            "SHELL:-Werror"
            "SHELL:-Wno-c++98-compat"
            "SHELL:-Wno-c++98-compat-pedantic"
            "SHELL:-Wno-switch-default"
            "SHELL:-Wno-padded"
        )
    elseif(MSVC)
        target_compile_options(
            "${target}"
            PRIVATE
            /W4
            /WX
        )
    endif()
endfunction()

function(mark_imported_includes_as_system target)
    foreach(dependency IN LISTS ARGN)
        if(TARGET "${dependency}")
            target_include_directories(
                "${target}"
                SYSTEM
                PRIVATE
                $<TARGET_PROPERTY:${dependency},INTERFACE_INCLUDE_DIRECTORIES>
            )
        endif()
    endforeach()
endfunction()
