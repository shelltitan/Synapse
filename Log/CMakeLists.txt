set(
    Header_Files
    "include/Log.hpp"
)

set(
    Source_Files
    "source/Log.cpp"
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

add_library(Log)

set_target_properties(
    Log
    PROPERTIES 
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    Log
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

target_link_libraries(
    Log
    PUBLIC 
    spdlog::spdlog
    cpptrace::cpptrace
    FileSystem
)

install(
    TARGETS Log
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Log"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/Log"
)