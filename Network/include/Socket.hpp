#pragma once
#include <NetAddress.hpp>
#include <memory>

namespace Synapse::Network {
    SOCKET CreateUDPSocket(std::string& ip, std::uint16_t port, bool useIPV6, bool use_packet_tag);

    bool DisableConnectionReset(SOCKET socket);
    bool BindNetAddress(SOCKET socket, NetAddress netAddr);
    bool BindAnyAddress(SOCKET socket, std::uint16_t port = 0, bool IPv6);
    void Close(SOCKET& socket);

#ifdef _WIN32
    class SocketUtils {
    public:
        SocketUtils();
        ~SocketUtils();
    };
    extern std::unique_ptr<SocketUtils> CoreSocket;
#endif
}
