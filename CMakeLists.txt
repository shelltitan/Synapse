cmake_minimum_required(
	VERSION 4.0.0 
	FATAL_ERROR
)

project(
	Synapse
	LANGUAGES CXX
    VERSION 0.0.1
)

set(CMAKE_DEBUG_POSTFIX "d")

option(BUILD_TESTS "Build unit tests for the libraries" OFF)
option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" OFF)

include(GNUInstallDirs)

include(cmake/DocumentationGeneration.cmake)
include(cmake/Dependencies.cmake)
include(cmake/SanitizerConfig.cmake)

#add_subdirectory(Config)
#add_subdirectory(Console)
add_subdirectory(FileSystem)
add_subdirectory(Log)
#add_subdirectory(Maths)
add_subdirectory(Memory)
#add_subdirectory(Network)
#add_subdirectory(Serialisation)
#add_subdirectory(STL)
if (BUILD_TESTS)
    enable_testing()
	include(Catch)
    add_subdirectory(Tests)
endif()
#add_subdirectory(Thread)
#add_subdirectory(Utility)

install(
    EXPORT ${PROJECT_NAME}Targets
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION "cmake"
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake"
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake"
)