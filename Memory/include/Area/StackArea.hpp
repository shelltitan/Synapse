#pragma once
#include <array>
#include <cstddef>

namespace Synapse::Memory::Area {
    /**
     * @brief Fixed-size stack-allocated buffer used as an arena backing store.
     * @tparam TSizeInBytes Capacity of the buffer in bytes.
     */
    template <std::size_t TSizeInBytes>
    class StackArea {
    public:
        StackArea() noexcept {};
        ~StackArea() = default;

        StackArea(const StackArea&) = delete;
        StackArea(StackArea&&) = delete;
        auto operator=(const StackArea &) -> StackArea & = delete;
        auto operator=(StackArea &&) -> StackArea & = delete;
        auto operator==(const StackArea &other) const -> bool = delete;

        /**
         * @brief Returns a pointer to the first byte of the buffer.
         */
        [[nodiscard]] auto GetStart() const noexcept -> std::byte * { return &m_buffer.data(); }
        /**
         * @brief Returns a pointer one past the last byte of the buffer.
         */
        [[nodiscard]] auto GetEnd() const noexcept -> std::byte * { return &m_buffer.data() + m_buffer.size(); }
        /**
         * @brief Reports the buffer capacity in bytes.
         */
        [[nodiscard]] auto GetSize() const noexcept -> std::size_t { return m_buffer.size(); }

    private:
        std::array<std::byte, TSizeInBytes> m_buffer{};
    };
}
