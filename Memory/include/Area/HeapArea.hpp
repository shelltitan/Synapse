#pragma once

#include <cstddef>

namespace Synapse::Memory::Area {
    /**
     * @brief Heap-backed storage provider for arenas.
     *
     * Allocates a contiguous buffer on construction and releases it on
     * destruction. Copy operations are disallowed to avoid double-free.
     */
    class HeapArea {
    public:
        HeapArea() = delete;

        explicit HeapArea(std::size_t size) noexcept;
        ~HeapArea() noexcept;

        HeapArea(const HeapArea&) = delete;
        HeapArea(HeapArea&&) = delete;
        auto operator=(const HeapArea &) -> HeapArea & = delete;
        auto operator=(HeapArea &&) -> HeapArea & = delete;
        auto operator==(const HeapArea &other) const -> bool = delete;

        /**
         * @brief Pointer to the first byte of the allocated buffer.
         */
        [[nodiscard]] auto GetStart() const noexcept -> std::byte * { return m_start; }
        /**
         * @brief Pointer one past the last byte of the allocated buffer.
         */
        [[nodiscard]] auto GetEnd() const noexcept -> std::byte * { return m_end; }
        /**
         * @brief Reports the size of the allocated buffer in bytes.
         */
        [[nodiscard]] auto GetMemory() const noexcept -> std::size_t { return m_end - m_start; }

    private:
        std::byte* m_start;
        std::byte* m_end;
    };
}
