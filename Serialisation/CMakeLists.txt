set(
    Header_Files
    "include/Serialise.hpp"
    "include/BitReader.hpp"
    "include/Constant.hpp"
    "include/BitWriter.hpp"
    "include/ReadStream.hpp"
    "include/SerialiseBit.hpp"
    "include/WriteStream.hpp"
)

set(
    Source_Files
    "source/Serialise.cpp"
    "source/BitReader.cpp"
    "source/BitWriter.cpp"
    "source/ReadStream.cpp"
    "source/SerialiseBit.cpp"
    "source/WriteStream.cpp"
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

add_library(Serialisation)

set_target_properties(
    Serialisation
    PROPERTIES
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    Serialisation
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

target_link_libraries(
    Serialisation
    PUBLIC
    Utility
    Log
    libassert::assert
)

install(
    TARGETS Serialisation
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Serialisation"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/Serialisation"
)