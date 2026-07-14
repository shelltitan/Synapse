# This toolchain is assumes it is being used on a Windows machine targeting Windows, and used with ninja for buildsystem generation.
# The toolchain also selects the latest available Visual Studio toolchain, which perplexingly means the latest updated or installed.
include_guard(GLOBAL)

# Host system must be Windows and x64
if((NOT (CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)) OR (NOT (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL AMD64)))
    return()
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    CMAKE_SYSTEM_PROCESSOR
    CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE
    CMAKE_VS_PLATFORM_NAME
    MSVS_INSTALL_PATH
    CMAKE_VS_PLATFORM_TOOLSET_VERSION
    CMAKE_WINDOWS_KITS_10_DIR
    CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION
    CMAKE_CXX_SCAN_FOR_MODULES
    VS_TOOLSET_PATH
)

# This is for the Visual Studio Generator
set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "x64")
set(CMAKE_VS_PLATFORM_NAME "x64")

# Target triplet (CPU family/model, vendor, and OS name)
set(CMAKE_SYSTEM_PROCESSOR "AMD64")
set(CMAKE_SYSTEM_NAME "${CMAKE_HOST_SYSTEM_NAME}")
set(CMAKE_SYSTEM_VERSION "${CMAKE_HOST_SYSTEM_VERSION}")
set(CMAKE_CROSSCOMPILING OFF)
set(CMAKE_C_COMPILER_TARGET   x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

# MSVC installation path and finding toolset (compiler, linker, etc.) and some libraries
# Locate Visual Studio via vswhere
find_program(
    VSWHERE_EXECUTABLE
    NAMES vswhere vswhere.exe
    DOC "Visual Studio Locator"
    HINTS "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer"
    REQUIRED
)

message(CHECK_START "Searching for Visual Studio")
execute_process(COMMAND "${VSWHERE_EXECUTABLE}" -nologo -nocolor
    -format json
    -latest # unfortunately this is the last installed or updated version
    -products *
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
    -utf8
    ENCODING UTF-8
    OUTPUT_VARIABLE _vs_json
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(JSON _len LENGTH "${_vs_json}")
if (_len  EQUAL 0)
    message(CHECK_FAIL "not found")
    message(FATAL_ERROR "No Visual Studio installation detected.")
endif()

string(JSON _vs_path GET "${_vs_json}" 0 "installationPath")

if (NOT _vs_path)
    message(CHECK_FAIL "no installationPath")
    message(FATAL_ERROR "Invalid vswhere JSON.")
endif()

cmake_path(CONVERT "${_vs_path}" TO_CMAKE_PATH_LIST _vs_path NORMALIZE)
message(CHECK_PASS "found: ${_vs_path}")
set(MSVS_INSTALL_PATH "${_vs_path}" CACHE PATH "Visual Studio Installation Path")

# MSVC toolset folder and version. Callers may pin MSVC_TOOLSET_VERSION in a
# user preset; otherwise the newest installed toolset is selected.
set(MSVS_MSVC_PATH "${MSVS_INSTALL_PATH}/VC/Tools/MSVC")
file(GLOB _toolsets RELATIVE "${MSVS_MSVC_PATH}" "${MSVS_MSVC_PATH}/*")
list(SORT _toolsets COMPARE NATURAL ORDER DESCENDING)
if(DEFINED MSVC_TOOLSET_VERSION AND NOT MSVC_TOOLSET_VERSION STREQUAL "")
    set(CMAKE_VS_PLATFORM_TOOLSET_VERSION "${MSVC_TOOLSET_VERSION}")
else()
    list(POP_FRONT _toolsets CMAKE_VS_PLATFORM_TOOLSET_VERSION)
endif()
set(VS_TOOLSET_PATH "${MSVS_MSVC_PATH}/${CMAKE_VS_PLATFORM_TOOLSET_VERSION}")

# Windows SDK Path (C runtime and system libraries)
message(CHECK_START "Searching for Windows SDK Root Directory")
cmake_host_system_information(
    RESULT CMAKE_WINDOWS_KITS_10_DIR
    QUERY
    WINDOWS_REGISTRY "HKLM/SOFTWARE/Microsoft/Windows Kits/Installed Roots" VALUE "KitsRoot10" VIEW BOTH
    ERROR_VARIABLE _wsdk_err
)
if(_wsdk_err OR NOT CMAKE_WINDOWS_KITS_10_DIR)
    message(CHECK_FAIL "not found: ${_wsdk_err}")
    message(FATAL_ERROR "Windows 10/11 SDK not found.")
endif()
cmake_path(CONVERT "${CMAKE_WINDOWS_KITS_10_DIR}" TO_CMAKE_PATH_LIST CMAKE_WINDOWS_KITS_10_DIR NORMALIZE)
message(CHECK_PASS "found: ${CMAKE_WINDOWS_KITS_10_DIR}")

# Pick an explicitly requested SDK first, then the developer environment SDK,
# and finally the newest installed SDK.
if(DEFINED WINDOWS_SDK_VERSION AND NOT WINDOWS_SDK_VERSION STREQUAL "")
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "${WINDOWS_SDK_VERSION}")
elseif(DEFINED ENV{WindowsSdkLibVersion})
    string(REGEX REPLACE "/$" "" CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "$ENV{WindowsSdkLibVersion}")
else()
    set(_WSI "${CMAKE_WINDOWS_KITS_10_DIR}/Include")
    file(GLOB _sdks RELATIVE "${_WSI}" "${_WSI}/*")
    list(SORT _sdks COMPARE NATURAL ORDER DESCENDING)
    list(GET _sdks 0 CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
endif()

# SDK include and lib paths
set(WINDOWS_KITS_BIN_PATH "${CMAKE_WINDOWS_KITS_10_DIR}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" CACHE PATH "" FORCE)
set(WINDOWS_KITS_INCLUDE_PATH "${CMAKE_WINDOWS_KITS_10_DIR}/include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" CACHE PATH "" FORCE)
set(WINDOWS_KITS_LIB_PATH "${CMAKE_WINDOWS_KITS_10_DIR}/lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" CACHE PATH "" FORCE)
set(WINDOWS_KITS_PLATFORM_PATH "${CMAKE_WINDOWS_KITS_10_DIR}/Platforms/UAP/${CMAKE_SYSTEM_VERSION}/Platform.xml" CACHE PATH "" FORCE)

if(NOT EXISTS ${WINDOWS_KITS_BIN_PATH})
    message(FATAL_ERROR "Windows SDK ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION} cannot be found: Folder '${WINDOWS_KITS_BIN_PATH}' does not exist.")
endif()

if(NOT EXISTS ${WINDOWS_KITS_INCLUDE_PATH})
    message(FATAL_ERROR "Windows SDK ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION} cannot be found: Folder '${WINDOWS_KITS_INCLUDE_PATH}' does not exist.")
endif()

if(NOT EXISTS ${WINDOWS_KITS_LIB_PATH})
    message(FATAL_ERROR "Windows SDK ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION} cannot be found: Folder '${WINDOWS_KITS_LIB_PATH}' does not exist.")
endif()

set(_SDK_INC_UCRT     "${WINDOWS_KITS_INCLUDE_PATH}/ucrt")
set(_SDK_INC_UM       "${WINDOWS_KITS_INCLUDE_PATH}/um")
set(_SDK_INC_SHARED   "${WINDOWS_KITS_INCLUDE_PATH}/shared")
set(_SDK_INC_WINRT    "${WINDOWS_KITS_INCLUDE_PATH}/winrt")
set(_SDK_INC_CPPWINRT "${WINDOWS_KITS_INCLUDE_PATH}/cppwinrt")

set(_SDK_LIB_UCRT      "${WINDOWS_KITS_LIB_PATH}/ucrt/${CMAKE_VS_PLATFORM_NAME}")
set(_SDK_LIB_UM        "${WINDOWS_KITS_LIB_PATH}/um/${CMAKE_VS_PLATFORM_NAME}")

set(_MSVC_BIN "${VS_TOOLSET_PATH}/bin/Host${CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE}/${CMAKE_VS_PLATFORM_NAME}")
set(_SDK_BIN  "${WINDOWS_KITS_BIN_PATH}/${CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE}")

## Set C and C++ compilers and etc.
find_program(CMAKE_C_COMPILER        NAMES cl   REQUIRED DOC "MSVC C Compiler" HINTS "${_MSVC_BIN}")
find_program(CMAKE_CXX_COMPILER      NAMES cl   REQUIRED DOC "MSVC C++ Compiler" HINTS "${_MSVC_BIN}")
find_program(CMAKE_LINKER            NAMES link REQUIRED DOC "MSVC Linker" HINTS "${_MSVC_BIN}")
find_program(CMAKE_AR                NAMES lib  REQUIRED DOC "MSVC Archiver" HINTS "${_MSVC_BIN}")
# Optional ml/ml64
find_program(CMAKE_MASM_ASM_COMPILER NAMES ml64 ml       DOC "MSVC ASM Compiler" HINTS "${_MSVC_BIN}")

find_program(CMAKE_RC_COMPILER       NAMES rc   REQUIRED DOC "MSVC Resource Compiler" HINTS "${_SDK_BIN}")
find_program(CMAKE_MT                NAMES mt            DOC "MSVC Manifest Tool" HINTS "${_SDK_BIN}")
find_program(MIDL_COMPILER           NAMES midl          DOC "MSVC MIDL Compiler" HINTS "${_SDK_BIN}")
find_program(MDMERGE_TOOL            NAMES mdmerge       DOC "MSVC MDMERGE" HINTS "${_SDK_BIN}")

## Add includes and libraries
# Standard include directories (guarded). Construct a fresh list so repeated
# configure runs do not append another copy to cached CMake variables.
set(_STANDARD_INCLUDE_DIRECTORIES)
foreach(_p
    "${VS_TOOLSET_PATH}/include"
    "${VS_TOOLSET_PATH}/atlmfc/include"
    "${_SDK_INC_UCRT}"
    "${_SDK_INC_UM}"
    "${_SDK_INC_SHARED}"
    "${_SDK_INC_WINRT}"
    "${_SDK_INC_CPPWINRT}")
    if(EXISTS "${_p}")
        list(APPEND _STANDARD_INCLUDE_DIRECTORIES "${_p}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _STANDARD_INCLUDE_DIRECTORIES)
foreach(LANG C CXX RC ASM_MASM)
    set(CMAKE_${LANG}_STANDARD_INCLUDE_DIRECTORIES
        "${_STANDARD_INCLUDE_DIRECTORIES}" CACHE STRING "" FORCE)
endforeach()

# Standard library search. Prefer /LIBPATH so each target gets it reliably.
set(_LIBPATHS)
foreach(_p
    "${VS_TOOLSET_PATH}/lib/${CMAKE_VS_PLATFORM_NAME}"
    "${VS_TOOLSET_PATH}/ATLMFC/lib/${CMAKE_VS_PLATFORM_NAME}"
    "${_SDK_LIB_UCRT}"
    "${_SDK_LIB_UM}")
    if(_p AND EXISTS "${_p}")
        list(APPEND _LIBPATHS "${_p}")
    endif()
endforeach()

if(_LIBPATHS)
    foreach(LANG C CXX RC ASM_MASM)
        set(CMAKE_${LANG}_STANDARD_LINK_DIRECTORIES
                "${_LIBPATHS}" CACHE STRING "Default link directories" FORCE)
    endforeach()
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Set 'CMAKE_<LANG>_COMPILER_PREDEFINES_COMMAND' to allow consumers - like automoc - to obtain the compiler predefines.
set(CMAKE_CXX_COMPILER_PREDEFINES_COMMAND
    ${CMAKE_CXX_COMPILER}
    /nologo
    /Zc:preprocessor
    /PD
    /c
    /Fonul.
    ${CMAKE_ROOT}/Modules/CMakeCXXCompilerABI.cpp
)

set(CMAKE_C_COMPILER_PREDEFINES_COMMAND
    ${CMAKE_C_COMPILER}
    /nologo
    /Zc:preprocessor
    /PD
    /c
    /Fonul.
    ${CMAKE_ROOT}/Modules/CMakeCCompilerABI.c
)

## Set flags
foreach(LANG C CXX)
    set(CMAKE_${LANG}_FLAGS_INIT "${CMAKE_${LANG}_FLAGS_INIT} /X") # ignore %INCLUDE%
endforeach()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL ARM)
    set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} /EHsc")
endif()
