#pragma once

#if _WIN32
#define NOMINMAX
#include <mswsock.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(__linux__) || defined(__unix__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#endif

#include <string>


namespace Synapse::Network {
    class NetAddress {
    public:
        NetAddress();
        NetAddress(std::string_view address, std::uint16_t port, bool IPv6);
        NetAddress(sockaddr* address);
        auto SetAddress(std::string_view address, std::uint16_t port, bool IPv6) -> void;
        auto SetAddress(sockaddr *address) -> void;
        auto GetSockAddress() -> sockaddr *;
        auto GetAddressFamily() -> ADDRESS_FAMILY { return m_addr_storage.ss_family; }
        auto GetIPAddress() -> std::string;
        auto GetPort() -> std::uint16_t;
        auto Reset() -> void;
        auto IsEqualToAddress(NetAddress *address) -> bool;
        auto IsEqualToAddress(sockaddr *address) -> bool;

    private:
        sockaddr_storage m_addr_storage;
    };
}
