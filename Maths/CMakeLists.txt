set(
    Header_Files
    "include/Maths.hpp"
)

set(
    Source_Files
    "source/Maths.cpp"
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

add_library(Maths)

set_target_properties(
    Maths
    PROPERTIES 
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    Maths
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

install(
    TARGETS Maths
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Maths"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/Maths"
)