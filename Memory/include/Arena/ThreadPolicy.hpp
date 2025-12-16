#pragma once
#include <cstddef>
#include <concepts>

namespace Synapse::Memory::Arena {
    /**
     * @brief Concept describing synchronization hooks required by arenas.
     */
    template <typename TPolicy>
    concept ThreadPolicy = requires(const TPolicy p) {
        p.Enter();
        p.Leave();
    };

    /**
     * @brief No-op thread policy for single-threaded use.
     */
    class SingleThreadPolicy {
    public:
        SingleThreadPolicy() = delete;
        auto operator==(const SingleThreadPolicy &other) const -> bool = delete;

        static auto Enter() noexcept -> void {}
        static auto Leave() noexcept -> void {}
    };

    /**
     * @brief Thread policy that forwards to a synchronization primitive.
     * @tparam TSynchronizationPrimitive Type providing `Enter`/`Leave`.
     */
    template <class TSynchronizationPrimitive>
    class MultiThreadPolicy {
    public:
        explicit MultiThreadPolicy(const TSynchronizationPrimitive& primitive) noexcept
            : m_primitive(primitive) {
        }
        MultiThreadPolicy() = delete;
        auto operator==(const MultiThreadPolicy &other) const -> bool = delete;

        /**
         * @brief Acquires the underlying synchronization primitive.
         */
        auto Enter() noexcept -> void {
            m_primitive.Enter();
        }

        /**
         * @brief Releases the underlying synchronization primitive.
         */
        auto Leave() noexcept -> void {
            m_primitive.Leave();
        }

    private:
        TSynchronizationPrimitive& m_primitive;
    };
}
