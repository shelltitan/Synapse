if(NOT (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux))
    return()
endif()

list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        CMAKE_SYSTEM_PROCESSOR
        CMAKE_CXX_SCAN_FOR_MODULES
        CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS
        CMAKE_C_COMPILER
        CMAKE_CXX_COMPILER
        CMAKE_ASM_COMPILER
        CMAKE_AR
        CMAKE_C_COMPILER_AR
        CMAKE_CXX_COMPILER_AR
        CMAKE_RANLIB
        CMAKE_C_COMPILER_RANLIB
        CMAKE_CXX_COMPILER_RANLIB
        CMAKE_LINKER
)

## Target triplet (CPU family/model, vendor, and OS name)
if (NOT DEFINED CMAKE_SYSTEM_PROCESSOR)
    set(CMAKE_SYSTEM_PROCESSOR "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

# If `CMAKE_SYSTEM_PROCESSOR` is not equal to `CMAKE_HOST_SYSTEM_PROCESSOR`, this is cross-compilation.
# CMake expects `CMAKE_SYSTEM_NAME` to be set to reflect cross-compilation.
if(NOT (CMAKE_SYSTEM_PROCESSOR STREQUAL "${CMAKE_HOST_SYSTEM_PROCESSOR}"))
    set(CMAKE_SYSTEM_NAME "${CMAKE_HOST_SYSTEM_NAME}")
    set(CMAKE_SYSTEM_VERSION "${CMAKE_HOST_SYSTEM_VERSION}")
    set(CMAKE_CROSSCOMPILING ON)
else()
    set(CMAKE_CROSSCOMPILING OFF)
endif()

# Resolve LLVM tools explicitly. Ubuntu runners often provide versioned
# binaries such as llvm-ar-18 without an unversioned llvm-ar symlink.
find_program(CMAKE_C_COMPILER
        NAMES clang clang-20 clang-19 clang-18 clang-17 clang-16
        REQUIRED
        DOC "Clang C compiler"
)
find_program(CMAKE_CXX_COMPILER
        NAMES clang++ clang++-20 clang++-19 clang++-18 clang++-17 clang++-16
        REQUIRED
        DOC "Clang C++ compiler"
)

get_filename_component(_CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

find_program(CMAKE_ASM_COMPILER
        NAMES clang clang-20 clang-19 clang-18 clang-17 clang-16
        HINTS "${_CLANG_BIN_DIR}"
        REQUIRED
        DOC "Clang assembler"
)
find_program(CMAKE_AR
        NAMES llvm-ar llvm-ar-20 llvm-ar-19 llvm-ar-18 llvm-ar-17 llvm-ar-16
        HINTS "${_CLANG_BIN_DIR}"
        REQUIRED
        DOC "LLVM archiver"
)
find_program(CMAKE_RANLIB
        NAMES llvm-ranlib llvm-ranlib-20 llvm-ranlib-19 llvm-ranlib-18 llvm-ranlib-17 llvm-ranlib-16
        HINTS "${_CLANG_BIN_DIR}"
        REQUIRED
        DOC "LLVM ranlib"
)
find_program(CMAKE_LINKER
        NAMES ld.lld ld.lld-20 ld.lld-19 ld.lld-18 ld.lld-17 ld.lld-16 lld lld-20 lld-19 lld-18 lld-17 lld-16
        HINTS "${_CLANG_BIN_DIR}"
        REQUIRED
        DOC "LLVM linker"
)

set(CMAKE_C_COMPILER_AR "${CMAKE_AR}")
set(CMAKE_CXX_COMPILER_AR "${CMAKE_AR}")
set(CMAKE_C_COMPILER_RANLIB "${CMAKE_RANLIB}")
set(CMAKE_CXX_COMPILER_RANLIB "${CMAKE_RANLIB}")

if(DEFINED CLANG_TARGET)
    set(CMAKE_C_COMPILER_TARGET ${CLANG_TARGET})
    set(CMAKE_CXX_COMPILER_TARGET ${CLANG_TARGET})
    set(CMAKE_ASM_COMPILER_TARGET ${CLANG_TARGET})
endif()

# Use LLVM's lld linker
set(CMAKE_LINKER_TYPE LLD)
