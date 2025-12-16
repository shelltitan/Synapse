set(
    Header_Files
    "include/AlignmentUtility.hpp"
    "include/StringUtility.hpp"
)

set(
    Source_Files
    "source/AlignmentUtility.cpp"
    "source/StringUtility.cpp"
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

add_library(Utility)

set_target_properties(
    Utility
    PROPERTIES 
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    Utility
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

target_link_libraries(
    Utility
    PUBLIC 
    Log
    libassert::assert
)

install(
    TARGETS Utility
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Utility"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/Utility"
)