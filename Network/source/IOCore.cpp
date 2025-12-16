#include <IOCore.hpp>
#include <chrono>

#ifdef _WIN32
#else

#include <cassert>
#include <random>
#include "rioring/io_service.h"
#include "rioring/io_context.h"
#include "rioring/tcp_server.h"

namespace CoreLib {
    namespace Network {
        thread_local io_uring* io_service::thread_ring = nullptr;

        template <typename T, typename = typename std::enable_if<std::is_integral<T>::value, T>::type>
        inline T random(T begin, T end) {
            static std::random_device dev;
            static std::mt19937_64 r{ dev() };
            static critical_section lock;

            std::scoped_lock sc{ lock };
            std::uniform_int_distribution<T> dist{ begin, end };
            return dist(r);
        }

        io_service::io_service() : context_pool{ RIORING_CONTEXT_POOL_SIZE } {}

        bool io_service::run(int concurrency) {
            if (concurrency < 1)
                return false;

            io_array.reserve(concurrency);
            tg.run_object(this, concurrency);

            while (concurrency > m_running_io) {
                std::this_thread::sleep_for(1ms);
            }

            return true;
        }

        void io_service::stop() {
            for (auto ring : io_array) {
                io_uring_sqe* sqe = io_uring_get_sqe(ring);
                sqe->user_data = 0;

                io_uring_prep_cancel(sqe, nullptr, 0);
                io_uring_submit(ring);
            }

            io_array.clear();
            tg.wait_for_terminate();
        }

        io_context* io_service::allocate_context() {
            return context_pool.pop();
        }

        void io_service::deallocate_context(io_context* ctx) {
            ctx->handler.reset();
            ctx->iov.iov_base = nullptr;
            ctx->iov.iov_len = 0;

            context_pool.push(ctx);
        }

        sockaddr* io_service::current_sockaddr(io_context* ctx) {
            sockaddr* sa;
            if (ctx->ctype == io_context::context_type::tcp_context) {
                auto socket = to_socket_ptr(ctx->handler);
                sa = socket->socket_address();
            }
            else {
                sa = (sockaddr*)&ctx->addr;
            }

            return sa;
        }

        bool io_service::submit(io_context* ctx) {
            auto ring = thread_ring;
            if (!ring) {
                ring = io_array[rioring::random<std::size_t>(0, io_array.size() - 1)];
            }

            io_uring_sqe* sqe = io_uring_get_sqe(ring);

            if (!sqe) {
                ctx->handler->io_error(std::make_error_code(std::errc(EIO)));
                return false;
            }

            switch (ctx->type) {
            case io_context::io_type::accept:
                if (auto server = to_tcp_server_ptr(ctx->handler); server != nullptr) {
                    io_uring_prep_accept(sqe,
                        server->server_socket,
                        std::bit_cast<sockaddr*>(&server->addr6),
                        &server->addr_len,
                        0);
                }
                break;
            case io_context::io_type::read:
                if (auto socket = to_socket_ptr(ctx->handler); socket != nullptr) {
                    if (ctx->ctype == io_context::context_type::tcp_context) {
                        io_uring_prep_readv(sqe, socket->socket_handler, &ctx->iov, 1, 0);
                    }
                    else {
                        io_uring_prep_recvmsg(sqe, socket->socket_handler, &ctx->msg, 0);
                    }
                }
                break;
            case io_context::io_type::write:
                if (auto socket = to_socket_ptr(ctx->handler); socket != nullptr) {
                    if (ctx->ctype == io_context::context_type::tcp_context) {
                        io_uring_prep_writev(sqe, socket->socket_handler, &ctx->iov, 1, 0);
                    }
                    else {
                        io_uring_prep_sendmsg(sqe, socket->socket_handler, &ctx->msg, 0);
                    }
                }
                break;
            case io_context::io_type::shutdown:
                if (auto socket = to_socket_ptr(ctx->handler); socket != nullptr) {
                    io_uring_prep_shutdown(sqe, socket->socket_handler, 0);
                }
                else {
                    if (auto server = to_tcp_server_ptr(ctx->handler); server != nullptr) {
                        io_uring_prep_shutdown(sqe, server->server_socket, 0);
                    }
                }
                break;
            default:
                break;
            }

            io_uring_sqe_set_data(sqe, ctx);
            io_uring_submit(ring);

            return true;
        }

        void io_service::on_thread() {
            io_uring ring{};
            io_uring_params params{};

            if (auto r = io_uring_queue_init_params(RIORING_IO_URING_ENTRIES, &ring, &params);
                r != 0 || !(params.features & IORING_FEAT_FAST_POLL)) {
                assert((params.features & IORING_FEAT_FAST_POLL) && "Kernel does not support io uring fast poll!");
                int* p = nullptr;
                *p = 0;
            }
            else {
                std::scoped_lock sc{ lock };
                io_array.push_back(&ring);
            }

            thread_ring = &ring;
            ++m_running_io;

            io(&ring);
            io_uring_queue_exit(&ring);

            --m_running_io;
            thread_ring = nullptr;
        }

        // io_uring should not handle socket close directly. (submit required)
        // io_uring should not directly treat the socket's close.
        void io_service::io(io_uring* ring) {
            io_uring_cqe* cqe;

            while (true) {
                if (io_uring_wait_cqe(ring, &cqe) < 0) {
                    continue;
                }

                if (!cqe->user_data) {
                    io_uring_cqe_seen(ring, cqe);
                    break;
                }

                auto context = std::bit_cast<io_context*>(cqe->user_data);
                auto socket = to_socket_ptr(context->handler);

                switch (context->type) {
                case io_context::io_type::accept:
                    if (cqe->res > 0) {
                        if (auto server = to_tcp_server_ptr(context->handler); server != nullptr) {
                            server->io_accepting(cqe->res, std::bit_cast<sockaddr*>(&server->addr6));
                        }
                    }
                    break;
                case io_context::io_type::read:
                    if (cqe->res > 0) {
                        socket->io_received(cqe->res, current_sockaddr(context));
                    }
                    else {
                        socket->io_shutdown();
                    }
                    break;
                case io_context::io_type::write:
                    if (cqe->res > 0) {
                        socket->io_sent(cqe->res, current_sockaddr(context));
                    }
                    else {
                        socket->io_error(std::make_error_code(std::errc(-cqe->res)));
                    }
                    break;
                case io_context::io_type::shutdown:
                    if (socket) {
                        if (socket->socket_handler) {
                            int fd = socket->socket_handler;
                            socket->socket_handler = 0;

                            ::shutdown(fd, SHUT_RDWR);
                            ::close(fd);
                        }
                    }
                    else {
                        if (auto server = to_tcp_server_ptr(context->handler); server != nullptr) {
                            int fd = server->server_socket;
                            server->server_socket = 0;

                            ::shutdown(fd, SHUT_RDWR);
                            ::close(fd);
                        }
                    }

                    break;
                default:
                    break;
                }

                deallocate_context(context);
                io_uring_cqe_seen(ring, cqe);
            }
        }
    }
}

#endif