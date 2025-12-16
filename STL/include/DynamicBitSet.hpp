#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>
#include <libassert/assert.hpp>

namespace Synapse::STL {
    template <typename TBlockType>
    concept BlockType = requires(TBlockType a) {
        std::is_integral_v<TBlockType>;
        std::is_unsigned_v<TBlockType>;
    };

    template <typename TAllocatorType>
    concept AllocatorType = requires(TAllocatorType allocator) {
        { allocator.Allocate() } -> std::convertible_to<std::byte*>;
        allocator.Deallocate();
    };

    /// \todo make it work with the allocators
    template <BlockType TBlockType, AllocatorType TAllocator>
    class DynamicBitSet {
    public:
        static constexpr std::size_t bits_per_block = std::numeric_limits<TBlockType>::digits;

        DynamicBitSet() noexcept : m_array(nullptr), m_set_size(0U) {}
        DynamicBitSet(const std::size_t size) noexcept {
            if ((size % bits_per_block) == 0U) {
                m_set_size = size / bits_per_block;
            }
            else {
                m_set_size = (size / bits_per_block) + 1;
            }

            m_array = m_allocator.Allocate(sizeof(TBlockType) * m_set_size);
            std::fill(m_array, m_array + m_set_size, 0);
        }
        ~DynamicBitSet() noexcept {
            if (m_array) {
                m_allocator.Allocate(m_array);
                m_array = nullptr;
            }
        }

        DynamicBitSet(const DynamicBitSet&) = delete;
        DynamicBitSet(DynamicBitSet&&) = delete;
        auto operator=(const DynamicBitSet &) -> DynamicBitSet & = delete;
        auto operator=(DynamicBitSet &&) -> DynamicBitSet & = delete;
        auto operator==(const DynamicBitSet &other) const -> bool = delete;

        // Resize the bit array
        auto Resize(const std::size_t size) noexcept -> void {
            std::size_t set_size;
            if ((size % bits_per_block) == 0) {
                set_size = size / bits_per_block;
            }
            else {
                set_size = (size / bits_per_block) + 1;
            }

            if (m_set_size == set_size) {
                return;
            }

            /// \todo make it work with the allocators
            TBlockType* tmp_array = new TBlockType[set_size];
            std::fill(tmp_array, tmp_array + set_size, 0);

            if (m_array) {
                (void)std::copy_n(m_array, std::min(m_set_size, set_size), tmp_array);

                /// \todo make it work with the allocators
                delete m_array; 
                m_array = tmp_array;
            }
            else {
                m_array = tmp_array;
            }
            m_set_size = set_size;
        }

        // Set a bit to 1 at a given index
        auto SetBit(const std::size_t index) noexcept -> void {
            DEBUG_ASSERT((index >= 0) && (index < m_set_size * bits_per_block));
            m_array[index / bits_per_block] |= (static_cast<TBlockType>(1) << (index % bits_per_block));
        }

        // Set a bit to 0 at a given index
        auto ClearBit(const std::size_t index) noexcept -> void {
            DEBUG_ASSERT((index >= 0) && (index < m_set_size * bits_per_block));
            m_array[index / bits_per_block] &= ~(static_cast<TBlockType>(1) << (index % bits_per_block));
        }

        // Get a bit value at a given index
        [[nodiscard]] auto GetBit(const std::size_t index) const noexcept -> bool {
            DEBUG_ASSERT((index >= 0) && (index < m_set_size * bits_per_block));
            return (m_array[index / bits_per_block] & (static_cast<TBlockType>(1) << (index % bits_per_block)));
        }

        // Set everything to
        auto SetAll() noexcept -> void {
            std::fill(m_array, m_array + m_set_size, std::numeric_limits<TBlockType>::max());
        }

        // Set everything to 0
        auto ClearAll() noexcept -> void {
            std::fill(m_array, m_array + m_set_size, 0);
        }

    private:
        TBlockType* m_array;
        std::size_t m_set_size;
        TAllocator& m_allocator;
    };
}
