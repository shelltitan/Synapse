#pragma once
#include <cstdint>

#ifdef _WIN32
#define NOMINMAX
#include <mswsockdef.h>
#include <ws2def.h>
#include <ws2ipdef.h>

namespace CoreNetwork {

    enum class IOEventType : std::uint8_t {
        None,
        Receive,
        Send
    };

    /*--------------
        IOEvent
    ---------------*/
    struct IOEventContext {
        explicit IOEventContext(const IOEventType type) :
            m_event_type(type), m_request_queue{ nullptr }, m_binded_data_buffer{}, m_binded_address_buffer{} {}

        IOEventContext() = default;
        ~IOEventContext() = default;

        IOEventType m_event_type{ IOEventType::None };

        RIO_RQ m_request_queue;

        RIO_BUF m_binded_data_buffer;
        RIO_BUF m_binded_address_buffer;
    };

    struct IOReceiveContext : public IOEventContext {
        IOReceiveContext() : IOEventContext(IOEventType ::Receive) {}
    };

    struct IOSendContext : public IOEventContext {
        IOSendContext() : IOEventContext(IOEventType ::Send) {}
    };
}

#else
#include <bits/types/struct_iovec.h>
#include <netinet/in.h>

namespace CoreNetwork {
    struct io_context {
        enum class EventType : std::uint8_t {
            Unknown,
            Connect,
            Disconnect,
            Accept,
            Receive,
            Send
        };

        enum class ContextType {
            TCP,
            UDP
        };

        io_context() {
            msg.msg_name = &addr;
            msg.msg_namelen = sizeof(sockaddr_storage);
        }
        ~io_context() = default;

        io_type type{ io_type::unknown };
        context_type ctype{ context_type::tcp_context };
        iovec iov{};
        std::shared_ptr<object_base> handler;

        msghdr msg{};
        sockaddr_storage addr{};
    };
}
#endif