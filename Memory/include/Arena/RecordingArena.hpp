#pragma once
#include <cstddef>
#include <source_location>

namespace Synapse::Memory::Arena {
    /**
     * @brief Decorator arena that forwards calls while optionally recording them.
     *
     * Intended for instrumentation (e.g., sending allocation data over the
     * network) without changing the underlying arena implementation.
     */
    template <class TArena>
    class RecordingArena {
    public:
        RecordingArena() = delete;
        ~RecordingArena() = default;

        explicit RecordingArena(TArena& arena) noexcept : m_arena(arena) {}
        RecordingArena(const RecordingArena&) = delete;
        RecordingArena(RecordingArena&&) = delete;
        auto operator=(const RecordingArena &) -> RecordingArena & = delete;
        auto operator=(RecordingArena &&) -> RecordingArena & = delete;
        auto operator==(const RecordingArena &other) const -> bool = delete;

        /**
         * @brief Forwards allocation to the wrapped arena.
         * @param size       Requested size.
         * @param alignment  Alignment requirement.
         * @param source_location Optional caller metadata.
         */
        [[nodiscard]] auto Allocate(const std::size_t size, const std::size_t alignment,
                const std::source_location &source_location = std::source_location::current()) noexcept -> std::byte * {
            // send info via TCP/IP...
            return m_arena.Allocate(size, alignment, source_location);
        }

        /**
         * @brief Forwards deallocation to the wrapped arena.
         */
        auto Deallocate(void *ptr) noexcept -> void {
            // send info via TCP/IP...
            m_arena.Deallocate(ptr);
        }

    private:
        TArena& m_arena;
    };
}
