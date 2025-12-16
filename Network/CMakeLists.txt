set(
    Header_Files
	"include/IOCore.hpp"
	"include/IOEvent.hpp"
	"include/NetAddress.hpp"
	"include/Socket.hpp"
	"include/ReliableUDP/Buffer.hpp"
	"include/ReliableUDP/Connection.hpp"
	"include/ReliableUDP/ConnectionStatistics.hpp"
	"include/ReliableUDP/Errors.hpp"
	"include/ReliableUDP/MessageChannelShared.hpp"
	"include/ReliableUDP/ReliableOrderedMessageChannel.hpp"
	"include/ReliableUDP/Serialise.hpp"
	"include/ReliableUDP/UnreliableMessageChannel.hpp"
	"include/ReliableUDP/UnreliableProcessMessageChannel.hpp"
	"include/Secure/Crypto.hpp"
	"include/Secure/ReplayPacketProtection.hpp"
)

set(
    Source_Files
    "source/IOCore.cpp"
    "source/NetAddress.cpp"
    "source/Socket.cpp"
    "source/ReliableUDP/Buffer.cpp"
    "source/ReliableUDP/Connection.cpp"
    "source/ReliableUDP/ConnectionStatistics.cpp"
    "source/ReliableUDP/ReliableOrderedMessageChannel.cpp"
    "source/ReliableUDP/Serialise.cpp"
    "source/ReliableUDP/UnreliableMessageChannel.cpp"
    "source/ReliableUDP/UnreliableProcessMessageChannel.cpp"
    "source/Secure/Crypto.cpp"
    "source/Secure/ReplayPacketProtection.cpp"
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

add_library(Network)

set_target_properties(
    Network
    PROPERTIES 
    FOLDER ${PROJECT_NAME}
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

target_sources(
    Network
    PRIVATE 
    ${Source_Files}
    PUBLIC 
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${Header_Files}
)

target_link_libraries(
    Network
    PUBLIC
    $<$<BOOL:${WIN32}>:ws2_32>
    Log
    Memory
    Thread
    libassert::assert
)

install(
    TARGETS Network
    EXPORT ${PROJECT_NAME}Targets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Network"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}/Network"
)