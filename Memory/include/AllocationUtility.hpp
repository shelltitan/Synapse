#pragma once
#include <algorithm>
#include <bit>
#include <cstddef>
#include <source_location>

namespace Synapse::Memory::Allocator {
    /**
     * @brief Primary template used to extract array type/count at compile time.
     */
    template<typename TType>
    struct TypeAndCount {
        TypeAndCount() = delete;
        auto operator==(const TypeAndCount &other) const -> bool = delete;
    };

    /**
     * @brief Specialization capturing the element type and compile-time count of an array type.
     */
    template<typename TType, std::size_t TCount>
    struct TypeAndCount<TType[TCount]> {
        using Type = TType;
        static constexpr std::size_t Count{ TCount };

        TypeAndCount() = delete;
        auto operator==(const TypeAndCount &other) const -> bool = delete;
    };

    /**
     * @brief Allocates and constructs an array inside a memory arena.
     */
    template<typename TType, class TArena>
    [[nodiscard]] auto NewArray(TArena &arena, std::size_t N,
            const std::source_location &source_location = std::source_location::current()) noexcept -> TType * {
        std::byte *ptr;
        if constexpr (sizeof(TType) > sizeof(std::size_t)) {
            ptr = arena.Allocate(sizeof(TType) * (N + 1), alignof(TType), source_location);
            // store the number of instances in first std::size_t bytes
            (void)std::copy_n(std::bit_cast<std::byte *>(&N), sizeof(std::size_t), ptr);

            if constexpr (std::is_trivially_default_constructible_v<TType>) {
                return std::bit_cast<TType *>(ptr + sizeof(TType));
            } else {
                // construct instances using placement new
                // the first element is the size
                const TType *const one_past_last = std::bit_cast<TType *>(ptr + sizeof(TType) * (N + 1));
                TType *as_TType = std::bit_cast<TType *>(ptr + sizeof(TType));
                while (as_TType < one_past_last) {
                    new (as_TType++) TType;
                }
                // hand user the pointer to the first instance
                return (as_TType - N);
            }
        } else {
            ptr = arena.Allocate(sizeof(TType) * N + sizeof(std::size_t), alignof(TType), source_location);
            // store the number of instances in first std::size_t bytes
            (void)std::copy_n(std::bit_cast<std::byte *>(&N), sizeof(std::size_t), ptr);

            if constexpr (std::is_trivially_default_constructible_v<TType>) {
                return std::bit_cast<TType *>(ptr + sizeof(std::size_t));
            } else {
                // construct instances using placement new
                const TType *const one_past_last =
                        std::bit_cast<TType *>(ptr + sizeof(std::size_t) + (sizeof(TType) * N));
                TType *as_TType = std::bit_cast<TType *>(ptr + sizeof(std::size_t));
                while (as_TType < one_past_last) {
                    new (as_TType++) TType;
                }
                // hand user the pointer to the first instance
                return (as_TType - N);
            }
        }
    }

    /**
     * @brief Calls the destructor and frees memory using the arena.
     */
    template<typename T, class TArena>
    auto Delete(T *object, TArena &arena) noexcept -> void {
        DEBUG_ASSERT(object != nullptr, "Invalid pointer");
        // call the destructor first...
        object->~T();

        // ...and free the associated memory
        arena.Deallocate(object);
    }

    /**
     * @brief Destroys an array allocated with `NewArray` and returns its memory to the arena.
     */
    template<typename TType, class TArena>
    auto DeleteArray(TType *ptr, TArena &arena) noexcept -> void {
        DEBUG_ASSERT(ptr != nullptr, "Invalid pointer");

        // user pointer points to the first instance...
        std::byte *pointer = std::bit_cast<std::byte *>(ptr);

        if constexpr (std::is_trivially_default_constructible_v<TType>) {
            if constexpr (alignof(TType) > alignof(std::size_t)) {
                arena.Deallocate(pointer - sizeof(TType));
            } else {
                arena.Deallocate(pointer - sizeof(std::size_t));
            }
        } else {
            std::size_t N;

            if constexpr (alignof(TType) > alignof(std::size_t)) {
                // ...so go to the last sizeof(std::size_t) bytes and grab a number of instances
                (void)std::copy_n(pointer - sizeof(TType), sizeof(std::size_t), std::bit_cast<std::byte *>(&N));

                // call instances' destructor in reverse order
                for (std::size_t i = N; i > 0; --i) {
                    ptr[i - 1].~TType();
                }

                arena.Deallocate(pointer - sizeof(TType));
            } else {
                // ...so go back sizeof(std::size_t) bytes and grab a number of instances
                (void)std::copy_n(pointer - sizeof(std::size_t), sizeof(std::size_t), std::bit_cast<std::byte *>(&N));

                // call instances' destructor in reverse order
                for (std::size_t i = N; i > 0; --i) {
                    ptr[i - 1].~TType();
                }

                arena.Deallocate(pointer - sizeof(std::size_t));
            }
        }
    }
}

#define SYNAPSE_NEW(type, arena) new (arena.Allocate(sizeof(type), alignof(type))) type
#define SYNAPSE_NEW_ARRAY(type, arena)                                                                                    \
    Synapse::Alloc::NewArray<TypeAndCount<type>::Type>(arena, TypeAndCount<type>::Count)
#define SYNAPSE_DELETE(object, arena) Synapse::Alloc::Delete(object, arena)
#define SYNAPSE_DELETE_ARRAY(object, arena) Synapse::Alloc::DeleteArray(object, arena)
