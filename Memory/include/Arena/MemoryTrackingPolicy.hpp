#pragma once
#include <cstddef>
#include <concepts>
#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include <unordered_map>
#include <mutex>
#include <libassert/assert.hpp>
#include <stacktrace>

namespace Synapse::Memory::Arena {
    /*
     * Useful for finding leaks
     * Options:
     * No memory tracking
     * Keeps count of allocation and deallocations useful for testing presence of memory leaks
    */
    /**
     * @brief Concept for policies that record allocation/deallocation events.
     */
    template <typename TPolicy>
    concept MemoryTrackingPolicy = requires(const TPolicy p, void* ptr, const std::size_t size, const std::size_t alignment, const std::source_location& source_info) {
        p.OnAllocation(ptr, size, alignment, source_info);
        p.OnDeallocation(ptr);
    };

    /**
     * @brief Stub tracking policy that performs no recording.
     */
    class NoMemoryTracking {
    public:
        NoMemoryTracking() = delete;
        auto operator==(const NoMemoryTracking &other) const -> bool = delete;

        /**
         * @brief Ignores allocation event.
         */
        static auto OnAllocation([[maybe_unused]] void *ptr, [[maybe_unused]] std::size_t size,
                [[maybe_unused]] std::size_t alignment,
                [[maybe_unused]] const std::source_location &source_location) noexcept -> void {}

        /**
         * @brief Ignores deallocation event.
         */
        static auto OnDeallocation([[maybe_unused]] void *ptr) noexcept -> void {}
    };

    class TracyMemoryTracking {
    public:
        TracyMemoryTracking() = delete;
        auto operator==(const TracyMemoryTracking &other) const -> bool = delete;

                /**
         * @brief Records an allocation event.
         * @param ptr             Raw pointer returned by the allocator.
         * @param size            Requested size including any padding added by the arena.
         * @param alignment       Alignment requested by the caller.
         * @param source_location Call-site metadata automatically filled by the arena.
         */
        static auto OnAllocation(void *ptr, std::size_t size, [[maybe_unused]] std::size_t alignment,
                [[maybe_unused]] const std::source_location &source_location) noexcept -> void {
            DEBUG_ASSERT(ptr != nullptr, "Invalid allocation pointer");
        }

        /**
         * @brief Records a deallocation event and removes tracking from tracy.
         * @param ptr Pointer originally passed to `OnAllocation`.
         */
        static auto OnDeallocation(void *ptr) noexcept -> void {
            DEBUG_ASSERT(ptr != nullptr, "Invalid deallocation, null pointer");
        }
    };

    /**
     * @brief Position tracking policy that records caller metadata for leak detection.
     *
     * Captures file, line, and function from `std::source_location` along with
     * the requested size and alignment.
     */
    class PostitionMemoryTracking {
    public:
        struct AllocationRecord {
            std::size_t size;
            std::size_t alignment;
            std::string file;
            std::uint_least32_t line;
            std::string function;
        };

        PostitionMemoryTracking() = delete;
        auto operator==(const PostitionMemoryTracking &other) const -> bool = delete;

        /**
         * @brief Records an allocation event.
         * @param ptr             Raw pointer returned by the allocator.
         * @param size            Requested size including any padding added by the arena.
         * @param alignment       Alignment requested by the caller.
         * @param source_location Call-site metadata automatically filled by the arena.
         */
        static auto OnAllocation(void *ptr, std::size_t size, std::size_t alignment,
                const std::source_location &source_location) noexcept -> void {
            DEBUG_ASSERT(ptr != nullptr, "Invalid allocation pointer");
            const AllocationRecord record{
                .size = size,
                .alignment = alignment,
                .file = source_location.file_name(),
                .line = source_location.line(),
                .function = source_location.function_name(),
            };
            std::scoped_lock lock{s_mutex};
            s_allocations[ptr] = record;
            ++s_live_allocations;
            ++s_total_allocations;
        }

        /**
         * @brief Records a deallocation event and removes tracking metadata.
         * @param ptr Pointer originally passed to `OnAllocation`.
         */
        static auto OnDeallocation(void *ptr) noexcept -> void {
            DEBUG_ASSERT(ptr != nullptr, "Invalid deallocation, null pointer");
            DEBUG_ASSERT(s_live_allocations > 0U, "Invalid deallocation, no live allocation");
            std::scoped_lock lock{s_mutex};
            const auto it = s_allocations.find(ptr);
            if (it != s_allocations.end()) {
                s_allocations.erase(it);
                --s_live_allocations;
            }
        }

        /**
         * @brief Number of currently live allocations tracked.
         */
        [[nodiscard]] static auto LiveAllocationCount() noexcept -> std::size_t {
            std::scoped_lock lock{s_mutex};
            return s_live_allocations;
        }

        /**
         * @brief Total allocations recorded since startup.
         */
        [[nodiscard]] static auto TotalAllocationCount() noexcept -> std::size_t {
            std::scoped_lock lock{s_mutex};
            return s_total_allocations;
        }

        /**
         * @brief Looks up metadata for a given pointer, if present.
         * @param ptr Pointer to search for.
         * @return Optional record describing the allocation.
         */
        [[nodiscard]] static auto FindAllocation(void *ptr) noexcept -> std::optional<AllocationRecord> {
            DEBUG_ASSERT(ptr != nullptr, "Invalid deallocation, null pointer");
            DEBUG_ASSERT(s_live_allocations > 0U, "Invalid deallocation, no live allocation");
            std::scoped_lock lock{s_mutex};
            if (const auto it = s_allocations.find(ptr); it != s_allocations.end()) {
                return it->second;
            }
            return std::nullopt;
        }

    private:
        static inline std::mutex s_mutex{};
        static inline std::unordered_map<void *, AllocationRecord> s_allocations{};
        static inline std::size_t s_live_allocations{ 0 };
        static inline std::size_t s_total_allocations{ 0 };
    };

    /**
     * @brief Complete tracking policy that records caller metadata and callstacks for leak detection.
     *
     * Captures file, line, and function from `std::source_location` along with
     * the requested size and alignment. When `<stacktrace>` is available, a
     * stack snapshot is stored for each allocation.
     */
    class CompleteMemoryTracking {
    public:
        struct AllocationRecord {
            std::size_t size;
            std::size_t alignment;
            std::string file;
            std::uint_least32_t line;
            std::string function;
            std::stacktrace stack;
        };

        CompleteMemoryTracking() = delete;
        auto operator==(const CompleteMemoryTracking &other) const -> bool = delete;

        /**
         * @brief Records an allocation event.
         * @param ptr             Raw pointer returned by the allocator.
         * @param size            Requested size including any padding added by the arena.
         * @param alignment       Alignment requested by the caller.
         * @param source_location Call-site metadata automatically filled by the arena.
         */
        static auto OnAllocation(void *ptr, std::size_t size, std::size_t alignment,
                const std::source_location &source_location) noexcept -> void {
            DEBUG_ASSERT(ptr != nullptr, "Invalid allocation pointer");
            const AllocationRecord record{
                .size = size,
                .alignment = alignment,
                .file = source_location.file_name(),
                .line = source_location.line(),
                .function = source_location.function_name(),
                .stack = std::stacktrace::current()
            };
            std::scoped_lock lock{s_mutex};
            s_allocations[ptr] = record;
            ++s_live_allocations;
            ++s_total_allocations;
        }

        /**
         * @brief Records a deallocation event and removes tracking metadata.
         * @param ptr Pointer originally passed to `OnAllocation`.
         */
        static auto OnDeallocation(void *ptr) noexcept -> void {
            DEBUG_ASSERT(ptr != nullptr, "Invalid deallocation, null pointer");
            DEBUG_ASSERT(s_live_allocations > 0U, "Invalid deallocation, no live allocation");
            std::scoped_lock lock{s_mutex};
            const auto it = s_allocations.find(ptr);
            if (it != s_allocations.end()) {
                s_allocations.erase(it);
                --s_live_allocations;
            }
        }

        /**
         * @brief Number of currently live allocations tracked.
         */
        [[nodiscard]] static auto LiveAllocationCount() noexcept -> std::size_t {
            std::scoped_lock lock{s_mutex};
            return s_live_allocations;
        }

        /**
         * @brief Total allocations recorded since startup.
         */
        [[nodiscard]] static auto TotalAllocationCount() noexcept -> std::size_t {
            std::scoped_lock lock{s_mutex};
            return s_total_allocations;
        }

        /**
         * @brief Looks up metadata for a given pointer, if present.
         * @param ptr Pointer to search for.
         * @return Optional record describing the allocation.
         */
        [[nodiscard]] static auto FindAllocation(void *ptr) noexcept -> std::optional<AllocationRecord> {
            DEBUG_ASSERT(ptr != nullptr, "Invalid deallocation, null pointer");
            DEBUG_ASSERT(s_live_allocations > 0U, "Invalid deallocation, no live allocation");
            std::scoped_lock lock{s_mutex};
            if (const auto it = s_allocations.find(ptr); it != s_allocations.end()) {
                return it->second;
            }
            return std::nullopt;
        }

    private:
        static inline std::mutex s_mutex{};
        static inline std::unordered_map<void *, AllocationRecord> s_allocations{};
        static inline std::size_t s_live_allocations{ 0 };
        static inline std::size_t s_total_allocations{ 0 };
    };
}
