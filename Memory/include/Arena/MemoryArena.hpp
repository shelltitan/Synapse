#pragma once

#include <concepts>
#include <cstddef>
#include <source_location>
#include <type_traits>
#include <Arena/AllocationPolicy.hpp>
#include <Arena/BoundsCheckingPolicy.hpp>
#include <Arena/ThreadPolicy.hpp>
#include <Arena/MemoryTrackingPolicy.hpp>
#include <Arena/MemoryTaggingPolicy.hpp>

#ifdef _WIN32
#define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

namespace Synapse::Memory::Arena {
    /**
     * @brief Memory arena uses the underlying alloctor to allocate memory.
     * @details
     * Uses policy based design to support MT and other uses cases.
     * Cannot be copied.
     * Example usages:
     *
     * @code{.cpp}
     * typedef MemoryArena<LinearAllocator, SingleThreadPolicy, NoBoundsChecking, NoMemoryTracking, NoMemoryTagging> SimpleArena;
     * #if ME_DEBUG || ME_RELEASE
     * typedef MemoryArena<LinearAllocator, SingleThreadPolicy, SimpleBoundsChecking, SimpleMemoryTracking, SimpleMemoryTagging> ApplicationArena;
     * #else
     * typedef MemoryArena<LinearAllocator, SingleThreadPolicy, NoBoundsChecking, NoMemoryTracking, NoMemoryTagging> ApplicationArena;
     * #endif
     *
     * StackArea<compile-time constant size> stackArea;
     * std::byte* stackArea = alloca(size);
     * typedef MemoryArena<PoolAllocator, SingleThreadPolicy, NoBoundsChecking, NoMemoryTracking, NoMemoryTagging> ST_PoolStackArena;
     * void SomeFunction() {
     *     StackArea<2048> stackArea;
     *      ST_PoolStackArena arena(stackArea);
     *      MyObject* obj = ME_NEW(MyObject, arena);
     *     ...
     *      ME_DELETE(obj, arena);
     *    }
     *
     * typedef MemoryArena<StackBasedAllocator, MultiThreadPolicy<CriticalSection>, NoBoundsChecking, NoMemoryTracking, NoMemoryTagging> MT_StackBasedArena;
     *
     * GpuArea gpuArea(16*1024*1024);
     * MT_StackBasedArena gpuArena(gpuArea);
     * TextureData* data = ME_NEW(TextureData, gpuArena);
     * // ...
     * ME_DELETE(data , gpuArena);
     * @endcode
    */
    template <AllocationPolicy TAllocator, ThreadPolicy TThread, BoundsCheckingPolicy TBoundsChecking, MemoryTrackingPolicy TMemoryTracking, MemoryTaggingPolicy TMemoryTagging>
    class MemoryArena {
    public:
        MemoryArena() = delete;

        template <AreaPolicy TAreaPolicy>
        /**
         * @brief Constructs a memory arena using the provided backing area.
         * @param area Storage provider exposing start/end pointers.
         */
        explicit MemoryArena(const TAreaPolicy& area) noexcept
            : m_allocator(area.GetStart(), area.GetEnd()) {
        }
        ~MemoryArena() = default;

        MemoryArena(const MemoryArena&) = delete;
        MemoryArena(MemoryArena&&) = delete;
        auto operator=(const MemoryArena &) -> MemoryArena & = delete;
        auto operator=(MemoryArena &&) -> MemoryArena & = delete;
        auto operator==(const MemoryArena &other) const -> bool = delete;

        /**
         * @brief Allocates memory while applying bounds checking, tagging, and tracking policies.
         * @param original_size Size requested by the caller (excluding guard bytes).
         * @param alignment     Alignment requirement.
         * @param source_location Optional call-site metadata.
         * @return Pointer to the user memory or nullptr on failure.
         */
        [[nodiscard]] auto Allocate(const std::size_t original_size, const std::size_t alignment,
                const std::source_location &source_location = std::source_location::current()) noexcept ->
            std::byte * {
            m_thread_guard.Enter();

            const std::size_t new_size{ original_size + TBoundsChecking::SIZE_BACK };

            // The allocators have a TOffset template parameter that has to match the TBoundsChecking::SIZE_FRONT
            auto plain_memory{ static_cast<std::byte*>(m_allocator.Allocate(new_size, alignment)) };

            m_bounds_checker.GuardFront(plain_memory);
            m_memory_tagger.TagAllocation(plain_memory + TBoundsChecking::SIZE_FRONT, original_size);
            m_bounds_checker.GuardBack(plain_memory + TBoundsChecking::SIZE_FRONT + original_size);

            m_memory_tracker.OnAllocation(plain_memory, new_size, alignment, source_location);

            m_thread_guard.Leave();

            return (plain_memory + TBoundsChecking::SIZE_FRONT);
        }

        /**
         * @brief Frees memory previously allocated by this arena.
         * @param ptr Pointer returned by Allocate.
         */
        auto Deallocate(std::byte *ptr) noexcept -> void {
            m_thread_guard.Enter();

            std::byte* original_memory{ ptr - TBoundsChecking::SIZE_FRONT };
            const std::size_t allocation_size{ m_allocator.GetAllocationSize(ptr) };

            m_bounds_checker.CheckFront(original_memory);
            m_memory_tagger.TagDeallocation(ptr, allocation_size - TBoundsChecking::SIZE_BACK);
            // The back padding size was added to the allocation size on allocation
            m_bounds_checker.CheckBack(ptr + (allocation_size - TBoundsChecking::SIZE_BACK));

            m_memory_tracker.OnDeallocation(original_memory);

            m_allocator.Deallocate(original_memory);

            m_thread_guard.Leave();
        }

    private:
        TAllocator m_allocator;
        TThread m_thread_guard;
        NO_UNIQUE_ADDRESS TBoundsChecking m_bounds_checker;
        NO_UNIQUE_ADDRESS TMemoryTracking m_memory_tracker;
        NO_UNIQUE_ADDRESS TMemoryTagging m_memory_tagger;
    };
}
