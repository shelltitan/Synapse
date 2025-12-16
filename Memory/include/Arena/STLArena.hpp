#pragma once
#include <source_location>
#include "MemoryArena.hpp"

namespace Synapse::Memory::Arena {
    /**
     * @brief STL-compatible allocator adapter that forwards to a memory arena.
     *
     * Provides the minimal interface required by standard containers so they
     * can allocate from a custom `TArena` instance.
     *
     * @tparam TType  Value type requested by the STL container.
     * @tparam TArena Underlying arena type implementing `Allocate`/`Deallocate`.
     */
    template <typename TType, class TArena>
    class STLArena {
    public:
        using value_type = TType;

        STLArena() = delete;
        ~STLArena() = default;
        /**
         * @brief Constructs an allocator bound to a specific arena instance.
         */
        explicit STLArena(TArena& arena) noexcept : m_arena(arena) {}

        template <typename U>
        explicit STLArena(const STLArena<U, TArena>& other) = delete;
        template <typename U>
        explicit STLArena(STLArena<U, TArena>&&) = delete;
        template <typename U>
        auto operator=(const STLArena<U, TArena> &) -> STLArena & = delete;
        template <typename U>
        auto operator=(STLArena<U, TArena> &&) -> STLArena & = delete;

        template <typename U>
        auto operator==(const STLArena<U, TArena> &rhs) const -> bool = delete;
        template <typename U>
        auto operator!=(const STLArena<U, TArena> &rhs) const -> bool = delete;

        /**
         * @brief Allocates storage for `n` objects of `TType`.
         */
        [[nodiscard]] constexpr auto allocate(const std::size_t n) noexcept -> TType * { return static_cast<TType*>(m_arena.Allocate(n * sizeof(TType), alignof(TType), std::source_location::current())); }
        /**
         * @brief Releases storage previously allocated with `allocate`.
         */
        constexpr auto deallocate(TType *p, [[maybe_unused]] std::size_t n) noexcept -> void { m_arena.Deallocate(p); }

        /**
         * @brief Reports the maximum number of bytes this allocator can provide.
         */
        [[nodiscard]] auto MaxAllocationSize() const noexcept -> std::size_t { return m_arena.GetSize(); }

    private:
        TArena& m_arena;
    };
}
