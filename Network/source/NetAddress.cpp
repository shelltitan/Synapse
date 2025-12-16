#include <Log.hpp>
#include <NetAddress.hpp>
#include <algorithm>
#include <array>
#include <libassert/assert.hpp>

namespace Synapse::Network {
    NetAddress::NetAddress() {
        Reset();
    }

    NetAddress::NetAddress(std::string_view address, std::uint16_t port, bool IPv6) {
        SetAddress(address, port, IPv6);
    }
    NetAddress::NetAddress(sockaddr* address) {
        DEBUG_ASSERT(address != nullptr);

        if (address->sa_family == AF_INET) {
            (void)std::copy_n(std::bit_cast<sockaddr_in*>(&m_addr_storage), sizeof(sockaddr_in), std::bit_cast<sockaddr_in*>(address));
        }
        else if (address->sa_family == AF_INET6) {
            (void)std::copy_n(std::bit_cast<sockaddr_in6*>(&m_addr_storage), sizeof(sockaddr_in6), std::bit_cast<sockaddr_in6*>(address));
        }
    }

    auto NetAddress::GetSockAddress() -> sockaddr * {
        return std::bit_cast<sockaddr*>(&m_addr_storage);
    }

    auto NetAddress::GetIPAddress() -> std::string {
        if (m_addr_storage.ss_family == AF_INET6) {
            std::array<char, INET6_ADDRSTRLEN> buffer;
            inet_ntop(AF_INET6, &std::bit_cast<sockaddr_in6*>(&m_addr_storage)->sin6_addr, buffer.data(), buffer.size());
            return buffer.data();
        }
        else {
            std::array<char, INET_ADDRSTRLEN> buffer;
            inet_ntop(AF_INET, &std::bit_cast<sockaddr_in*>(&m_addr_storage)->sin_addr, buffer.data(), buffer.size());
            return buffer.data();
        }
    }

    auto NetAddress::GetPort() -> std::uint16_t {
        if (m_addr_storage.ss_family == AF_INET6) {
            return ntohs(std::bit_cast<sockaddr_in6*>(&m_addr_storage)->sin6_port);
        }
        else {
            return ntohs(std::bit_cast<sockaddr_in*>(&m_addr_storage)->sin_port);
        }
    }

    auto NetAddress::SetAddress(std::string_view address, std::uint16_t port, bool IPv6) -> void {
        addrinfo *result, hint = {};
        std::string port_string = std::to_string(port);

        hint.ai_family = AF_UNSPEC;
        hint.ai_socktype = SOCK_DGRAM;
        hint.ai_protocol = IPPROTO_UDP;

        if (!getaddrinfo(address.data(), port_string.c_str(), &hint, &result)) {
            CORE_ERROR("Getaddrinfo failed!");
            return;
        }
        if (IPv6) {
            for (addrinfo* a = result; a != nullptr; a = a->ai_next) {
                if (a->ai_family == AF_INET6) {
                    (void)std::copy_n(std::bit_cast<sockaddr_in6*>(a->ai_addr), sizeof(sockaddr_in6), std::bit_cast<sockaddr_in6*>(&m_addr_storage));
                    break;
                }
            }
        }
        else {
            for (addrinfo* a = result; a != nullptr; a = a->ai_next) {
                if (a->ai_family == AF_INET) {
                    (void)std::copy_n(std::bit_cast<sockaddr_in*>(a->ai_addr), sizeof(sockaddr_in), std::bit_cast<sockaddr_in*>(&m_addr_storage));
                    break;
                }
            }
        }

        freeaddrinfo(result);
    }

    auto NetAddress::SetAddress(sockaddr *address) -> void {
        DEBUG_ASSERT(address != nullptr);
        if (address->sa_family == AF_INET6) {
            (void)std::copy_n(std::bit_cast<sockaddr_in6*>(address), sizeof(sockaddr_in6), std::bit_cast<sockaddr_in6*>(&m_addr_storage));
        }
        else if (address->sa_family == AF_INET) {
            (void)std::copy_n(std::bit_cast<sockaddr_in*>(address), sizeof(sockaddr_in), std::bit_cast<sockaddr_in*>(&m_addr_storage));
        }
    }

    auto NetAddress::Reset() -> void {
        std::fill(std::bit_cast<char*>(&m_addr_storage), std::bit_cast<char*>((&m_addr_storage + 1)), 0);
    }

    constexpr auto NetAddress::IsEqualToAddress(NetAddress *address) -> bool {
        DEBUG_ASSERT(address != nullptr);
        return IsEqualToAddress(address->GetSockAddress());
    }

    constexpr auto NetAddress::IsEqualToAddress(sockaddr *address) -> bool {
        DEBUG_ASSERT(address != nullptr);

        if (m_addr_storage.ss_family != address->sa_family) {
            return false;
        }

        if (m_addr_storage.ss_family == AF_INET) {
            if (std::equal(std::bit_cast<std::byte*>(&m_addr_storage), std::bit_cast<std::byte*>(std::bit_cast<sockaddr_in*>(&m_addr_storage) + 1), std::bit_cast<std::byte*>(address))) {
                return true;
            }
            return false;
        }
        else if (m_addr_storage.ss_family == AF_INET6) {
            if (std::equal(std::bit_cast<std::byte*>(&m_addr_storage), std::bit_cast<std::byte*>(std::bit_cast<sockaddr_in6*>(&m_addr_storage) + 1), std::bit_cast<std::byte*>(address))) {
                return true;
            }
            return false;
        }
        else {
            return false;
        }
    }
}
