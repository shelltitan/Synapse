#pragma once
#include <array>
#include <chrono>
#include <new>
#include <thread>

#include <IOEvent.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <cstdlib>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "IOEvent.h"

namespace CoreNetwork {
    /// \todo we could separate the send and receive data buffers(m_network_data_buffer) and address buffers
    /// \todo IOReceived and IOSent has to be implemented in the connection_manager_class
    /// \todo best concurrency value is the number of cpus, best amount of threads to wait is 2 * cpus
    template <class TMessageProcessor, int max_results, int receive_queue_size, int send_queue_size, int max_network_packet_size, int thread_count>
    class IOCore {
    public:
        IOCore(TMessageProcessor* message_processor, std::string_view ip_address, std::uint16_t port, bool is_ipv6, bool packet_tagging) : m_socket_handler(SocketUtils::CreateUDPSocket(ip, port, is_ipv6, packet_tagging)), m_running_io(0U), m_message_processor(message_processor) {
            if (m_socket_handler == INVALID_SOCKET) {
                throw std::runtime_error("Failed to initialise socket.");
            }
        }
        ~IOCore() {
            Stop();
            m_RIO_fn_table.RIODeregisterBuffer(m_data_buffer_id);
            m_RIO_fn_table.RIODeregisterBuffer(m_address_buffer_id);
            m_RIO_fn_table.RIOCloseCompletionQueue(m_completion_queue);
            CloseHandle(m_iocp_handle);
            SocketUtils::Close(m_socket_handler);
        }

        auto Initialise(std::uint16_t port, bool isIPV6, bool packet_tagging,
                connection_manager_class *connection_manager) -> bool {
            if (m_socket_handler == INVALID_SOCKET) {
                CORE_ERROR("Invalid socket");
                return false;
            }

            GUID function_table_id = WSAID_MULTIPLE_RIO;
            DWORD dw_bytes = 0;

            if (NULL != WSAIoctl(m_socket_handler, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &function_table_id, sizeof(GUID),
                            static_cast<void**>(&m_RIO_fn_table), sizeof(m_RIO_fn_table), &dw_bytes, nullptr, nullptr)) {
                CORE_ERROR("WSAIoctl Error: {}", GetLastError());
                return false;
            }

            m_iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
            if (m_iocp_handle == INVALID_HANDLE_VALUE) {
                CORE_ERROR("CreateIoCompletionPort failed");
                return false;
            }

            OVERLAPPED overlapped;

            RIO_NOTIFICATION_COMPLETION completionType;

            completionType.Type = RIO_IOCP_COMPLETION;
            completionType.Iocp.IocpHandle = m_iocp_handle;
            completionType.Iocp.CompletionKey = static_cast<void*>(CK_START);
            completionType.Iocp.Overlapped = &overlapped;

            m_completion_queue = m_RIO_fn_table.RIOCreateCompletionQueue(receive_queue_size + send_queue_size, &completionType);
            if (m_iocp_handle == RIO_INVALID_CQ) {
                CORE_ERROR("RIOCreateCompletionQueue failed");
                return false;
            }

            m_request_queue = m_RIO_fn_table.RIOCreateRequestQueue(m_socket_handler, receive_queue_size, 1,
                send_queue_size, 1, m_completion_queue, m_completion_queue, nullptr);

            m_data_buffer_id = m_RIO_fn_table.RIORegisterBuffer(std::bit_cast<char*>(m_network_data_buffer.data()), static_cast<DWORD>(max_network_packet_size * (receive_queue_size + send_queue_size)));
            m_address_buffer_id = m_RIO_fn_table.RIORegisterBuffer(std::bit_cast<char*>(m_address_buffer.data()), sizeof(sockaddr_storage) * static_cast<DWORD>(receive_queue_size + send_queue_size));

            ULONG buffer_count = 0;
            for (std::size_t i = 0; i < receive_queue_size; ++i) {
                IOReceiveContext& receive_context = m_receive_context_array[i];
                receive_context.m_request_queue = m_request_queue;
                receive_context.m_binded_address_buffer.BufferId = m_address_buffer_id;
                receive_context.m_binded_address_buffer.Offset = sizeof(sockaddr_storage) * buffer_count;
                receive_context.m_binded_address_buffer.Length = sizeof(sockaddr_storage);
                receive_context.m_binded_data_buffer.BufferId = m_data_buffer_id;
                receive_context.m_binded_data_buffer.Offset = max_network_packet_size * buffer_count;
                receive_context.m_binded_data_buffer.Length = max_network_packet_size;
                if (!m_RIO_fn_table.RIOReceiveEx(receive_context.m_request_queue, &receive_context.m_binded_data_buffer, 1, nullptr, &receive_context.m_binded_address_buffer, nullptr, nullptr, 0, &receive_context)) {
                    CORE_ERROR("Failed to queue buffer for receive queue (RIOReceiveEx)");
                    return false;
                }
                ++buffer_count;
            }

            for (auto& send_context : m_send_context_pool) {
                send_context.m_request_queue = m_request_queue;
                send_context.m_binded_address_buffer.BufferId = m_address_buffer_id;
                send_context.m_binded_address_buffer.Offset = sizeof(sockaddr_storage) * buffer_count;
                send_context.m_binded_address_buffer.Length = sizeof(sockaddr_storage);
                send_context.m_binded_data_buffer.BufferId = m_data_buffer_id;
                send_context.m_binded_data_buffer.Offset = max_network_packet_size * buffer_count;
                send_context.m_binded_data_buffer.Length = max_network_packet_size;
                ++buffer_count;
            }

            static_assert(thread_count > 0);

            for (std::uint32_t i = 0; i < thread_count; ++i) {
                m_io_thread_manager.Launch([=]() {
                    StartIOProcess();
                });
            }

            while (thread_count > m_running_io.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            INT notify_result = m_RIO_fn_table.RIONotify(m_completion_queue);
            if (notify_result != ERROR_SUCCESS) {
                CORE_ERROR("RIONotify failed");
                return false;
            }

            return true;
        }

        auto Stop() -> void {
            while (m_running_io.load() > 0) {
                if (0 == PostQueuedCompletionStatus(m_iocp_handle, 0, CK_STOP, nullptr)) {
                    CORE_ERROR("PostQueuedCompletionStatus Error: {}", GetLastError());
                }
            }
            m_io_thread_manager.Join();
        }

        auto GetSendContext() -> IOSendContext * {
            WRITE_LOCK_IDX(0)
            return m_send_context_pool.Pop();
        }
        auto GetSendBuffer(IOSendContext *context) -> std::byte * { return m_network_data_buffer.data() + context->m_binded_data_buffer.Offset; }
        auto GetSendAddressBuffer(IOSendContext *context) -> sockaddr_storage * { return std::bit_cast<sockaddr_storage*>(m_address_buffer.data() + context->m_binded_address_buffer.Offset); }

        auto Send(IOSendContext *context) -> bool {
            WRITE_LOCK_IDX(2)
            return m_RIO_fn_table.RIOSendEx(context->m_request_queue, &context->m_binded_data_buffer, 1, nullptr, &context->m_binded_address_buffer, nullptr, nullptr, 0, context);
        }
        auto SendPacket(NetAddress *to, std::byte *packet_data, int packet_bytes) -> bool {
            DEBUG_ASSERT(to);
            DEBUG_ASSERT(to->GetAddressFamily() == AF_INET6 || to->GetAddressFamily() == AF_INET);
            DEBUG_ASSERT(packet_data);
            DEBUG_ASSERT(packet_bytes > 0);
            DEBUG_ASSERT(max_network_packet_size >= packet_bytes);

            IOSendContext* send_context = GetSendContext();
            std::byte* send_buffer = GetSendBuffer(send_context);
            sockaddr_storage* address_buffer = GetSendAddressBuffer(send_context);

            (void)std::copy_n(packet_data, packet_bytes, send_buffer);
            send_context->m_binded_data_buffer.Length = packet_bytes;

            if (to->GetAddressFamily() == AF_INET6) {
                (void)std::copy_n(std::bit_cast<sockaddr_in6*>(to->GetSockAddress()), sizeof(sockaddr_in6), std::bit_cast<sockaddr_in6*>(address_buffer));
                send_context->m_binded_address_buffer.Length = sizeof(sockaddr_in6);
            }
            else if (to->GetAddressFamily() == AF_INET6) {
                (void)std::copy_n(std::bit_cast<sockaddr_in*>(to->GetSockAddress()), sizeof(sockaddr_in), std::bit_cast<sockaddr_in*>(address_buffer));
                send_context->m_binded_address_buffer.Length = sizeof(sockaddr_in6);
            }

#ifdef _DEBUG
            if (!Send(send_context)) {
                CORE_ERROR("SendPacketWSAIoctl Error: {}", GetLastError());
                DEBUG_ASSERT(false);
            }
#else
            return Send(send_context);
#endif
        }

    private:
        enum COMPLETION_KEY {
            CK_STOP = 0,
            CK_START = 1
        };

        auto StartIOProcess() -> void {
            m_running_io.fetch_add(1);

            ProcessIOEvent();

            m_running_io.fetch_add(-1);
        }
        auto ProcessIOEvent() -> void {
            DWORD number_of_bytes = 0;
            ULONG_PTR completion_key = 0;
            OVERLAPPED* pOverlapped = nullptr;

            INT notify_result;
            ULONG number_of_results;

            auto* results = std::bit_cast<RIORESULT*>(malloc(sizeof(RIORESULT) * max_results));
            std::fill(std::bit_cast<std::byte*>(results), std::bit_cast<std::byte*>(results + max_results), std::byte(0x00));

            while (true) {
                if (!GetQueuedCompletionStatus(m_iocp_handle, &number_of_bytes, &completion_key, &pOverlapped, INFINITE)) {
                    CORE_ERROR("GetQueuedCompletionStatus error: {}", GetLastError());
                    break;
                }

                if (completion_key == CK_STOP) {
                    CORE_INFO("Stopping IO thread");
                    break;
                }

                number_of_results = m_RIO_fn_table.RIODequeueCompletion(m_completion_queue, results, max_results);

                if (0 == number_of_results || RIO_CORRUPT_CQ == number_of_results) {
                    CORE_ERROR("RIODequeueCompletion error: {}", GetLastError());
                    break;
                }

                notify_result = m_RIO_fn_table.RIONotify(m_completion_queue);

                if (notify_result != ERROR_SUCCESS) {
                    CORE_ERROR("RIONotify error: {}", GetLastError());
                    break;
                }

                for (ULONG i = 0; i < number_of_results; ++i) {
                    IOEventContext* context = std::bit_cast<IOEventContext*>(results[i].RequestContext);

                    ULONG bytes_transferred = results[i].BytesTransferred;

                    switch (context->m_event_type) {
                    case IOEventType::Receive:
                        m_connection_manager->IOReceived(bytes_transferred, m_network_data_buffer.data() + context->m_binded_data_buffer.Offset, CurrentSockaddr(context));
                        {
                            WRITE_LOCK_IDX(1)
                            ASSERT_CRASH(m_RIO_fn_table.RIOReceiveEx(context->m_request_queue, &context->m_binded_data_buffer, 1, nullptr, &context->m_binded_address_buffer, nullptr, nullptr, 0, context));
                        }
                        break;
                    case IOEventType::Send:
                        m_connection_manager->IOSent(bytes_transferred, CurrentSockaddr(context));
                        {
                            WRITE_LOCK_IDX(0)
                            m_send_context_pool.Push(std::bit_cast<IOSendContext*>(context));
                        }
                        break;
                    default:
                        break;
                    }
                }
            }

            free(results);
        }
        auto CurrentSockaddr(IOEventContext *context) -> sockaddr * {
            return std::bit_cast<sockaddr*>(m_address_buffer.data() + context->m_binded_address_buffer.Offset);
        }

    private:
        SOCKET m_socket_handler;

        USE_MANY_LOCKS(3)
        CoreThread::ThreadManager m_io_thread_manager;

        HANDLE m_iocp_handle;
        std::atomic<std::uint32_t> m_running_io;
        RIO_CQ m_completion_queue{};
        RIO_RQ m_request_queue{ nullptr };

        RIO_BUFFERID m_address_buffer_id;
        RIO_BUFFERID m_data_buffer_id;

        alignas(std::hardware_destructive_interference_size) std::array<std::byte, sizeof(sockaddr_storage) * (receive_queue_size + send_queue_size)> m_address_buffer;
        alignas(std::hardware_destructive_interference_size) std::array<IOReceiveContext, receive_queue_size> m_receive_context_array;
        alignas(std::hardware_destructive_interference_size) std::array<std::byte, max_network_packet_size*(receive_queue_size + send_queue_size)> m_network_data_buffer;
        CoreMemory::StaticObjectPool<IOSendContext, send_queue_size> m_send_context_pool;

        RIO_EXTENSION_FUNCTION_TABLE m_RIO_fn_table;
        TMessageProcessor* m_message_processor;
    };
}

#else

#include <liburing.h>
#include "IOEvent.h"

namespace CoreNetwork {

    /*
     io_uring is essentially async, but configures thread context for each io_uring to increase efficiency.
    */
    class IOCore {
    public:
        IOCore();
        ~IOCore() = default;

        bool Run(int concurrency);
        bool Submit(io_context* ctx);
        void Stop();

        int RunningCount() const { return m_running_io; }

        io_context* AllocateContext();

    protected:
        void StartIOProcess();

    private:
        void ProcessIOEvent(io_uring* ring);
        void DeallocateContext(io_context* ctx);
        static sockaddr* CurrentSockaddr(io_context* ctx);

    private:
        spin_lock m_lock;
        std::vector<io_uring*> m_io_array;
        simple_pool<io_context> m_context_pool;
        std::atomic_int m_running_io{ 0 };
        thread_generator tg;

        static thread_local io_uring* thread_ring;
    };

    // Example for the linux io_uring very similar flow TODO linux
    // context->iov.iov_base = std::data(recv_bind_buffer);
    // context->iov.iov_len = RIORING_DATA_BUFFER_SIZE;

    // context->msg.msg_iov = &context->iov;
    // context->msg.msg_iovlen = 1;
    /* Sending data on linux io_uring
    //ctx->iov.iov_base = const_cast<unsigned char*>(*send_buffer);
    ctx->iov.iov_len = send_buffer.size();
    ctx->msg.msg_iov = &ctx->iov;
    ctx->msg.msg_iovlen = 1;

    std::size_t addr_len = 0;
    switch (addr->sa_family) {
    case AF_INET:
        addr_len = sizeof(sockaddr_in);
        break;
    case AF_INET6:
        addr_len = sizeof(sockaddr_in6);
        break;
    default:
        break;
    }

    if (addr_len) {
        std::memcpy(&ctx->addr, addr, addr_len);
        current_io->submit(ctx);
    }*/

    // this will probably move to IOCore to break out from the IO loop I guess
    // we can do it with a atomic_flag too but we have to look up if we need to clean up
    // the queues or something before we shut it down
    void SimpleServer::SubmitShutdown() {

        auto ctx = current_io->allocate_context();
        ctx->handler = shared_from_this();
        ctx->type = io_context::io_type::shutdown;

        current_io->submit(ctx);
    }
}
#endif