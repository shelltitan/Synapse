set(Header_Files
    "include/Job/GlobalQueue.hpp"
    "include/Job/Job.hpp"
    "include/Job/JobQueue.hpp"
    "include/Job/JobTimer.hpp"
    "include/DeadlockProfiler.hpp"
    "include/Lock.hpp"
    "include/Thread.hpp"
    "include/ThreadManager.hpp"
    "include/PeriodicTaskThread.hpp"
    "include/LockQueue.hpp"
    "include/ThreadLocals.hpp"
)

set(Source_Files
    "source/Job/GlobalQueue.cpp"
    "source/Job/Job.cpp"
    "source/Job/JobQueue.cpp"
    "source/Job/JobTimer.cpp"
    "source/ThreadLocals.cpp"
    "source/DeadlockProfiler.cpp"
    "source/Lock.cpp"
    "source/ThreadManager.cpp"
    "source/PeriodicTaskThread.cpp"
    "source/LockQueue.cpp"
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

add_library(Thread)

set_target_properties(
    Thread
    PROPERTIES 
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    Thread
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

target_link_libraries(
    Thread
    PUBLIC 
    Log
    Memory
    libassert::assert
    unordered_dense::unordered_dense
)

install(
    TARGETS Thread
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Thread"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/Thread"
)