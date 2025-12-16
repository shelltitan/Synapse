set(
    Header_Files
    "include/AllocationUtility.hpp"
    "include/Allocator/BuddyAllocator.hpp"
    "include/Allocator/FreeListAllocator.hpp"
    "include/Allocator/LinearAllocator.hpp"
    "include/Allocator/PoolAllocator.hpp"
    "include/Allocator/StackAllocator.hpp"
    "include/Area/HeapArea.hpp"
    "include/Area/StackArea.hpp"
    "include/Arena/AllocationPolicy.hpp"
    "include/Arena/BoundsCheckingPolicy.hpp"
    "include/Arena/MemoryArena.hpp"
    "include/Arena/MemoryTaggingPolicy.hpp"
    "include/Arena/MemoryTrackingPolicy.hpp"
    "include/Arena/RecordingArena.hpp"
    "include/Arena/STLArena.hpp"
    "include/Arena/ThreadPolicy.hpp"
)

set(
    Source_Files
    "source/AllocationUtility.cpp"
    "source/Allocator/BuddyAllocator.cpp"
    "source/Allocator/FreeListAllocator.cpp"
    "source/Allocator/LinearAllocator.cpp"
    "source/Allocator/PoolAllocator.cpp"
    "source/Allocator/StackAllocator.cpp"
    "source/Area/HeapArea.cpp"
    "source/Area/StackArea.cpp"
	"source/Arena/AllocationPolicy.cpp"
    "source/Arena/MemoryArena.cpp"
    "source/Arena/RecordingArena.cpp"
    "source/Arena/STLArena.cpp"
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

add_library(Alloc)

set_target_properties(
    Alloc
    PROPERTIES 
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    Alloc
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

target_link_libraries(
    Alloc
    PUBLIC 
    TracyClient
    libassert::assert
    Log
)

install(
    TARGETS Alloc
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Alloc"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/Alloc"
)