#pragma once
#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <new>
#include <Concurrent/ConcurrentCommon.hpp>


namespace Synapse::STL::Concurrent {

    // Swaps lower TShiftBits amount of bits with upper bits
    template<unsigned int TShiftBits>
    constexpr auto SwapUpperAndLowerBits(unsigned index) noexcept -> unsigned {
        constexpr unsigned int mix_mask{ (1U << TShiftBits) - 1U };
        // I assume the number has max 2 * TShiftBits bits
        // Upper bits are a( ]TShiftBits, 2 * TShiftBits] )
        // Lower bits are b( [first bit, TShiftBits]  )
        // Upper bits (a ^ 0) & 0 = 0
        // Lower bits (b ^ a) & 1 = b ^ a
        const unsigned int mix{ ((index ^ (index >> TShiftBits))) & mix_mask };
        // Upper bits (a ^ 0) ^ (b ^ a) = b
        // Lower bits b ^ (b ^ a) ^ 0 = a
        return index ^ mix ^ (mix << TShiftBits);
    }

    // Return the index without swapping, special case
    template<>
    constexpr auto SwapUpperAndLowerBits<0>(unsigned int index) noexcept -> unsigned {
        return index;
    }

    // Returns the shift bits needed to reduce false sharing, if the size is adequate, and we are minimising the contention
    template<bool TMinimiseContention, std::size_t array_size, std::size_t element_size>
    constexpr auto GetCacheIndexSwapBitShift() -> std::size_t {
        if constexpr (std::has_single_bit(array_size) && TMinimiseContention) {
            std::size_t elements_per_cache_line = std::hardware_destructive_interference_size / element_size;
            if constexpr (std::has_single_bit(elements_per_cache_line)) {
                constexpr int mask_bits = std::countr_zero(elements_per_cache_line);

                // The element counting starts from 0, so the actual size is the last index + 1
                unsigned int minimum_size = 1u << (mask_bits * 2);
                return array_size < minimum_size ? 0 : mask_bits;
            }
        } 
        return 0;
    }

    // Multiple writers/readers contend on the same cache line when storing/loading elements at
    // subsequent indexes, aka false sharing. For power of 2 ring buffer size it is possible to re-map
    // the index in such a way that each subsequent element resides on another cache line, which
    // minimizes contention. This is done by swapping the lowest order N bits (which are the index of
    // the element within the cache line) with the next N bits (which are the index of the cache line)
    // of the element index.
    template<int TShiftBits, class TType>
    constexpr auto SwapMapIndex(TType *elements, unsigned int index) noexcept -> TType & {
        return elements[SwapUpperAndLowerBits<TShiftBits>(index)];
    }


    template<class TDerived>
    class AtomicQueueCommon {
    public:
        /*
         * Appends an element to the end of the queue. Returns false when the queue is full.
         */
        template<typename TType>
        auto TryPush(TType &&element) noexcept -> bool ;

        /*
         * Removes an element from the front of the queue. Returns false when the queue is empty.
         */
        template<typename TType>
        auto TryPop(TType &element) noexcept -> bool ;

        /*
         * Appends an element to the end of the queue. Busy waits when the queue is full. Faster than TryPush when the queue is not full.
         * Optional FIFO producer queuing and total order.
         */
        template<typename TType>
        auto Push(TType &&element) noexcept -> void {
            unsigned int head;
            if constexpr (TDerived::single_producer_single_consumer) {
                head = m_head.load(std::memory_order::memory_order_relaxed);
                m_head.store(head + 1, std::memory_order::memory_order_relaxed);
            } else {
                constexpr auto memory_order = TDerived::total_order
                                                  ? std::memory_order_seq_cst
                                                  : std::memory_order_relaxed;
                head = m_head.fetch_add(1, memory_order);
            }
            static_cast<TDerived &>(*this).do_push(std::forward<TType>(element), head);
        }

        /*
         * Removes an element from the front of the queue. Busy waits when the queue is empty. Faster than TryPop when the queue is not empty.
         * Optional FIFO consumer queuing and total order.
         */
        auto Pop() noexcept {
            unsigned int tail;
            if constexpr (TDerived::single_producer_single_consumer) {
                tail = m_tail.load(std::memory_order::memory_order_relaxed);
                m_tail.store(tail + 1, std::memory_order::memory_order_relaxed);
            } else {
                constexpr auto memory_order = TDerived::total_order
                                                  ? std::memory_order_seq_cst
                                                  : std::memory_order_relaxed;
                tail = m_tail.fetch_add(1, memory_order);
            }
            return static_cast<TDerived &>(*this).do_pop(tail);
        }

        /*
         * Returns the number of unconsumed elements during the call. The state may have changed by the time the return value is examined.
         */
        auto WasSize() const noexcept -> unsigned int {
            // m_tail can be greater than m_head because of consumers doing pop, rather that TryPop, when the queue is empty.
            return std::max(
                    static_cast<int>(m_head.load(std::memory_order_relaxed) - m_tail.load(std::memory_order_relaxed)),
                    0);
        }

        /*
         * Returns true if the container was empty during the call. The state may have changed by the time the return value is examined.
         */
        auto WasEmpty() const noexcept -> bool {
            return !WasSize();
        }

        /*
         * Returns true if the container was full during the call. The state may have changed by the time the return value is examined.
         */
        auto WasFull() const noexcept -> bool {
            return WasSize() >= static_cast<int>(static_cast<const TDerived &>(*this).m_size);
        }

        /*
         * Returns the maximum number of elements the queue can possibly hold.
         */
        auto Capacity() const noexcept -> unsigned int {
            return static_cast<const TDerived &>(*this).m_size;
        }

    protected:
        enum State : std::uint8_t {
            Empty,
            Storing,
            Stored,
            Loading
        };

        AtomicQueueCommon() noexcept = default;

        /*
         * Not thread safe function
         */
        AtomicQueueCommon(const AtomicQueueCommon &b) noexcept :
            m_head(b.m_head.load(std::memory_order_relaxed)), m_tail(b.m_tail.load(std::memory_order_relaxed)) {}

        /*
         * Not thread safe function
         */
        auto operator=(const AtomicQueueCommon &b) noexcept -> AtomicQueueCommon & {
            m_head.store(b.m_head.load(std::memory_order_relaxed), std::memory_order_relaxed);
            m_tail.store(b.m_tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
            return *this;
        }

        /*
         * Not thread safe function
         */
        auto Swap(AtomicQueueCommon &b) noexcept -> void {
            unsigned head = m_head.load(std::memory_order_relaxed);
            unsigned tail = m_tail.load(std::memory_order_relaxed);
            m_head.store(b.m_head.load(std::memory_order_relaxed), std::memory_order_relaxed);
            m_tail.store(b.m_tail.load(std::memory_order_relaxed), std::memory_order_relaxed);
            b.m_head.store(head, std::memory_order_relaxed);
            b.m_tail.store(tail, std::memory_order_relaxed);
        }

        template<typename TType>
        static auto DoPopAtomic(std::atomic<TType> &q_element) noexcept -> TType {
            if constexpr (TDerived::m_single_producer_single_consumer) {
                for (;;) {
                    TType element = q_element.load(std::memory_order_acquire);
                    if (element != TType{}) [[likely]] {
                        q_element.store(TType{}, std::memory_order_relaxed);
                        return element;
                    }
                    if constexpr (TDerived::m_maximize_throughput) {
                        SpinLoopPause();
                    }
                }
            } else {
                for (;;) {
                    TType element = q_element.exchange(TType{}, std::memory_order_acquire);
                    // (2) The store to wait for.
                    if (element != TType{}) [[likely]] {
                        return element;
                    }
                    // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                    if constexpr (TDerived::m_maximize_throughput) {
                        do {
                            SpinLoopPause();
                        } while (q_element.load(std::memory_order_relaxed) == TType{});
                    } else {
                        SpinLoopPause();
                    }
                }
            }
        }

        template<typename TType>
        static auto DoPushAtomic(TType element, std::atomic<TType> &q_element) noexcept -> void {
            assert(element != TType{});
            if constexpr (TDerived::m_single_producer_single_consumer) {
                while (q_element.load(std::memory_order_relaxed) != TType{}) [[unlikely]] {
                    if constexpr (TDerived::m_maximize_throughput) {
                        SpinLoopPause();
                    }
                }
                q_element.store(element, std::memory_order_release);
            } else {
                for (TType expected = TType{}; !q_element.compare_exchange_weak(expected, element,
                             std::memory_order_release, std::memory_order_relaxed); expected = TType{}) [[unlikely]] {
                    // (1) Wait for store (2) to complete.
                    if constexpr (TDerived::m_maximize_throughput) {
                        do {
                            SpinLoopPause();
                        } while (q_element.load(std::memory_order_relaxed) != TType{});
                    } else {
                        SpinLoopPause();
                    }
                }
            }
        }

        template<typename TType>
        static auto DoPopAny(std::atomic<unsigned char> &state, TType &q_element) noexcept -> TType {
            if constexpr (TDerived::m_single_producer_single_consumer) {
                while (state.load(std::memory_order::memory_order_acquire) != State::Stored) [[unlikely]] {
                    if constexpr (TDerived::maximize_throughput) {
                        SpinLoopPause();
                    }
                }

                TType element{ std::move(q_element) };
                state.store(State::Empty, std::memory_order_release);
                return element;
            } else {
                for (;;) {
                    State expected = State::Stored;
                    if (state.compare_exchange_weak(expected, State::Loading, std::memory_order_acquire,
                            std::memory_order_relaxed)) [[likely]] {
                        TType element{ std::move(q_element) };
                        state.store(State::Empty, std::memory_order_release);
                        return element;
                    }
                    // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                    if constexpr (TDerived::maximize_throughput) {
                        do {
                            SpinLoopPause();
                        } while (state.load(std::memory_order_relaxed) != State::Stored);
                    } else {
                        SpinLoopPause();
                    }
                }
            }
        }

        template<class U, class T>
        static auto DoPushAny(U &&element, std::atomic<unsigned char> &state, T &q_element) noexcept -> void {
            if (TDerived::m_single_producer_single_consumer) {
                while (state.load(std::memory_order_acquire) != State::Empty) [[unlikely]] {
                    if constexpr (TDerived::m_maximize_throughput) {
                        SpinLoopPause();
                    }
                }
                q_element = std::forward<U>(element);
                state.store(State::Stored, std::memory_order_release);
            } else {
                for (;;) {
                    State expected = State::Empty;
                    if (state.compare_exchange_weak(expected, State::Storing, std::memory_order_acquire,
                            std::memory_order_relaxed)) [[likely]] {
                        q_element = std::forward<U>(element);
                        state.store(State::Stored, std::memory_order_release);
                        return;
                    }
                    // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                    if constexpr (TDerived::m_maximize_throughput) {
                        do {
                            SpinLoopPause();
                        } while (state.load(std::memory_order_relaxed) == State::Empty);
                    } else {
                        SpinLoopPause();
                    }
                }
            }
        }

        // Put these on different cache lines to avoid false sharing between readers and writers.
        alignas(std::hardware_destructive_interference_size) std::atomic<unsigned int> m_head = {};
        alignas(std::hardware_destructive_interference_size) std::atomic<unsigned int> m_tail = {};
    };

    template<class TDerived>
    template<typename TType>
    auto AtomicQueueCommon<TDerived>::TryPush(TType &&element) noexcept -> bool {
        auto head = m_head.load(std::memory_order_relaxed);
        if constexpr (TDerived::single_producer_single_consumer) {
            // Integer overflow on unsgined is not UB, but you are limited to 2^31 elements max
            if (static_cast<int>(head - m_tail.load(std::memory_order_relaxed)) >= static_cast<int>(static_cast<
                    TDerived &>(*this).m_size)) {
                return false;
            }
            m_head.store(head + 1, std::memory_order_relaxed);
        } else {
            do {
                // Integer overflow on unsgined is not UB, but you are limited to 2^31 elements max
                if (static_cast<int>(head - m_tail.load(std::memory_order_relaxed)) >= static_cast<int>(static_cast<
                        TDerived &>(*this).m_size)) {
                    return false;
                }
            }
            while (!m_head.compare_exchange_weak(head, head + 1, std::memory_order_relaxed,
                    std::memory_order_relaxed)) [[unlikely]]; // This loop is not FIFO.
        }

        static_cast<TDerived &>(*this).do_push(std::forward<TType>(element), head);
        return true;
    }

    template<class TDerived>
    template<typename TType>
    auto AtomicQueueCommon<TDerived>::TryPop(TType &element) noexcept -> bool {
        auto tail = m_tail.load(std::memory_order_relaxed);
        if constexpr (TDerived::single_producer_single_consumer) {
            if (static_cast<int>(m_head.load(std::memory_order_relaxed) - tail) <= 0) {
                return false;
            }
            m_tail.store(tail + 1U, std::memory_order_relaxed);
        } else {
            do  {
                if (static_cast<int>(m_head.load(std::memory_order_relaxed) - tail) <= 0) {
                    return false;
                }
            }
            while (!m_tail.compare_exchange_weak(tail, tail + 1U, std::memory_order_relaxed,
                    std::memory_order_relaxed)) [[unlikely]]; // This loop is not FIFO.
        }

        element = static_cast<TDerived &>(*this).do_pop(tail);
        return true;
    }
}
