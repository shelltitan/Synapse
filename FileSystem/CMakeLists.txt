set(
	Header_Files
	"include/FileMonitor.hpp"
	"include/FileUtils.hpp"
)

set(
    Source_Files
	"source/FileMonitor.cpp"
	"source/FileUtils.cpp"
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

add_library(FileSystem)

set_target_properties(
    FileSystem
    PROPERTIES 
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    FileSystem
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

target_link_libraries(
    FileSystem
    PUBLIC 
    unordered_dense::unordered_dense
)

install(
    TARGETS FileSystem
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FileSystem"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/FileSystem"
)