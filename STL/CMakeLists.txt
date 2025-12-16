set(
    Header_Files
    "include/Concurrent/AtomicQueue.hpp"
    "include/Concurrent/AtomicQueueCommon.hpp"
    "include/Concurrent/ConcurrentCommon.hpp"
    "include/DynamicBitSet.hpp"
    "include/ObjectPool.hpp"
)

set(
    Source_Files
    "source/Concurrent/AtomicQueue.cpp"
    "source/Concurrent/AtomicQueueCommon.cpp"
    "source/Concurrent/ConcurrentCommon.cpp"
    "source/DynamicBitSet.cpp"
    "source/ObjectPool.cpp"
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

add_library(STL)

set_target_properties(
    STL
    PROPERTIES 
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    STL
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

target_link_libraries(
    STL
    PUBLIC 
    libassert::assert 
    Utility
    Maths
)

install(
    TARGETS STL
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/STL"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/STL"
)