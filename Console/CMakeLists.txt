set(
    Header_Files
    "include/Console.hpp"
)

set(
    Source_Files
    "source/Console.cpp"
)

source_group(
	TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" 
	PREFIX "Header Files" 
	FILES ${Header_Files}
)

source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/source" 
    PREFIX "Source Files" 
    FILES ${Source_Files}
)

add_library(Console)

set_target_properties(
    Console
    PROPERTIES 
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    Console
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

target_link_libraries(
    Console
    PUBLIC 
    Log
)

install(
    TARGETS ${MODULE_NAME}
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Console"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/Console"
)