#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <ankerl/unordered_dense.h>

namespace Synapse::FileSystem {
    class FileMonitor;

    struct FileMonitorInfo {
        FileMonitor* monitor;
        std::filesystem::path path_to_watch;
        bool watch_sub_directories;
        std::uint32_t monitor_filter_flag;
        ankerl::unordered_dense::map<std::string, std::filesystem::file_time_type> paths;
    };

    class FileMonitor {
    public:
        enum Event {
            Added = 0x1,
            Removed = 0x2,
            Modified = 0x4,
        };

        FileMonitor() = default;
        ~FileMonitor() = default;

        auto Add(std::unique_ptr<FileMonitorInfo> init) -> bool;
        auto Exit() -> void;
        auto IsRunning() const -> bool;

        auto ThreadFunc(FileMonitorInfo *init_info) -> void;

        auto Clear() -> void;
        auto AddQueue(const std::filesystem::path &path, const std::filesystem::path &file_name) -> void;
        auto GetNumberOfChanges() -> std::size_t;
        auto PopChangedFileName() -> std::filesystem::path;

    private:
        std::vector<std::thread> m_threads{};
        std::vector<std::unique_ptr<FileMonitorInfo>> m_monitor_info{};
        std::deque<std::filesystem::path> m_change_file_group{};
        std::mutex m_monitor_mutex{};
        std::atomic_flag m_is_running{};
    };

    auto GetFileMonitor() -> FileMonitor *;
}
