#pragma once
#include <Maths.hpp>
#include <atomic>
#include <new>
#include <Concurrent/AtomicQueueCommon.hpp>



namespace Synapse::STL::Concurrent {
    template <typename TType>
    concept LockFreeType = std::atomic<TType>::is_always_lock_free;

    namespace Runtime {

    }
    namespace Static {
        template <LockFreeType TType, unsigned int TSize, bool TMinimiseContention = true, bool TMaximiseThroughput = true, bool TTotalOrder = false, bool TSingleProducerSingleConsumer = false>
        class AtomicQueue : public AtomicQueueCommon<AtomicQueue<TType, TSize, TMinimiseContention, TMaximiseThroughput, TTotalOrder, TSingleProducerSingleConsumer>> {
        public:
            using value_type = TType;

            AtomicQueue() noexcept {
                for (auto p = m_elements, q = m_elements + m_size; p != q; ++p) {
                    p->store(TType{}, std::memory_order_relaxed);
                }
            }

            AtomicQueue(const AtomicQueue&) = delete;
            auto operator=(const AtomicQueue&) -> AtomicQueue& = delete;

        private:
            using Base = AtomicQueueCommon<AtomicQueue>;
            friend Base;

            auto DoPop(unsigned tail) noexcept -> TType {
                std::atomic<TType>& q_element = SwapMapIndex<m_shuffle_bits>(m_elements, tail % m_size);
                return Base::template DoPopAtomic<TType>(q_element); // we can probably do this with constexpr between lock-free and non-lock free
            }

            auto DoPush(TType element, unsigned head) noexcept -> void {
                std::atomic<TType>& q_element = SwapMapIndex<m_shuffle_bits>(m_elements, head % m_size);
                Base::template DoPushAtomic<TType>(element, q_element); // we can probably do this with constexpr between lock-free and non-lock free
            }

            static constexpr unsigned int m_size = TMinimiseContention ? Maths::RoundUpToPowerOf2(TSize) : TSize;
            static constexpr int m_shuffle_bits =
                    GetCacheIndexSwapBitShift<TMinimiseContention, m_size, sizeof(std::atomic<TType>)>();
            static constexpr bool m_total_order = TTotalOrder;
            static constexpr bool m_single_producer_single_consumer = TSingleProducerSingleConsumer;
            static constexpr bool m_maximize_throughput = TMaximiseThroughput;

            alignas(std::hardware_destructive_interference_size) std::atomic<TType> m_elements[m_size];
        };
    }
}
