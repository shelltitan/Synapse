#include <Log.hpp>
#include <SocketUtils.hpp>

#ifdef _WIN32
#include <iphlpapi.h>
#include <qos2.h>
#include <winsock2.h>
#pragma comment(lib, "Qwave.lib")
#endif


namespace Synapse::Network {
#if _WIN32
    SocketUtils::SocketUtils() {
        /// \todo move to init function
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            return;
        }
    }
    SocketUtils::~SocketUtils() {

        WSACleanup();
    }
#endif
    SOCKET SocketUtils::CreateUDPSocket(std::string& ip, std::uint16_t port, bool useIPV6, bool use_packet_tag) {

        int af = AF_INET;
        if (useIPV6) {
            af = AF_INET6;
        }
        int socket_type = SOCK_DGRAM;

        SOCKET _socket;
#ifdef _WIN32
        _socket = WSASocketW(af, socket_type, IPPROTO_UDP, nullptr, 0, WSA_FLAG_REGISTERED_IO);
        if (_socket == INVALID_SOCKET) {
            CORE_ERROR("Failed to create socket");
            return INVALID_SOCKET;
        }
#else

            _socket = socket(af, socket_type, IPPROTO_UDP);
            if (socket_handler == -1) {
                CORE_ERROR("Failed to create socket");
                return INVALID_SOCKET;
            }


#endif
        if (!DisableConnectionReset(_socket)) {
            Close(_socket);
        }

        if (ip.empty()) {
            BindAnyAddress(_socket, port, useIPV6);
        }
        else if (!ip.empty()) {
            BindNetAddress(_socket, NetAddress(ip, port, useIPV6));
        }
        else {
            Close(_socket);
            return INVALID_SOCKET;
        }

        if (use_packet_tag) {
#ifdef _WIN32
            QOS_VERSION QosVersion = { 1, 0 };
            HANDLE qosHandle;
            QOS_FLOWID flowId = 0;
            if (QOSCreateHandle(&QosVersion, &qosHandle) == FALSE) {
                CORE_ERROR("{}", GetLastError());
                return INVALID_SOCKET;
            }
            if (QOSAddSocketToFlow(qosHandle, _socket, nullptr, QOSTrafficTypeAudioVideo, QOS_NON_ADAPTIVE_FLOW, &flowId) == FALSE) {
                CORE_ERROR("{}", GetLastError());
                return INVALID_SOCKET;
            }
#else
                if (useIPV6) {
                    int tos = 46;
                    if (setsockopt(_socket, IPPROTO_IPV6, IPV6_TCLASS, (NETCODE_CONST char*)&tos, sizeof(tos)) != 0) {
                        CORE_ERROR("Failed to enable packet tagging");
                        close(_socket);
                        return INVALID_SOCKET;
                    }
                }
                else {
                    int tos = 46;
                    if (setsockopt(_socket, IPPROTO_IP, IP_TOS, (NETCODE_CONST char*)&tos, sizeof(tos)) != 0) {
                        CORE_ERROR("Failed to enable packet tagging");
                        close(_socket);
                        return INVALID_SOCKET;
                    }
                }
#endif
        }

        return _socket;
    }

    bool SocketUtils::DisableConnectionReset(SOCKET socket) {
#ifdef _WIN32
        // #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
        BOOL bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        if (WSAIoctl(socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL) != 0) {
            CORE_ERROR("Failed to disable UDP CONNRESET (port unreachable) message reporting on socket.");
            return false;
        }
#endif
        return true;
    }

    bool SocketUtils::BindNetAddress(SOCKET socket, NetAddress netAddr) {
        if (netAddr.GetAddressFamily() == AF_INET6) {
            return SOCKET_ERROR != bind(socket, netAddr.GetSockAddress(), sizeof(sockaddr_in6));
        }
        else {
            return SOCKET_ERROR != bind(socket, netAddr.GetSockAddress(), sizeof(sockaddr_in));
        }
    }

    bool SocketUtils::BindAnyAddress(SOCKET socket, std::uint16_t port, bool IPv6) {
        if (IPv6) {
            sockaddr_in6 addr{};
            addr.sin6_family = AF_INET6;
            if (port != 0) {
                addr.sin6_port = htons(port);
            }

            return bind(socket, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR;
        }
        else {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            if (port != 0) {
                addr.sin_port = htons(port);
            }

            return bind(socket, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR;
        }
    }

    void SocketUtils::Close(SOCKET& socket) {
        if (socket != INVALID_SOCKET) {
#if defined(__linux__) || defined(__unix__)
                close(socket);
#elif _WIN32
            closesocket(socket);
#endif
        }
        socket = INVALID_SOCKET;
    }
#ifdef _WIN32
    std::unique_ptr<SocketUtils> CoreNetwork = std::make_unique<SocketUtils>();
#endif
}
