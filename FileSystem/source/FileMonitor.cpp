#include <FileMonitor.hpp>
#include <algorithm>

namespace Synapse::FileSystem {
    auto FileMonitor::ThreadFunc(FileMonitorInfo *init_info) -> void {
        bool watch_sub_directories{ init_info->watch_sub_directories };
        std::uint32_t monitor_filter_flag{ init_info->monitor_filter_flag };

        if (watch_sub_directories) {
            for (auto& file : std::filesystem::recursive_directory_iterator(init_info->path_to_watch)) {
                init_info->paths[file.path().string()] = std::filesystem::last_write_time(file);
            }
        }
        else {
            for (auto& file : std::filesystem::directory_iterator(init_info->path_to_watch)) {
                init_info->paths[file.path().string()] = std::filesystem::last_write_time(file);
            }
        }

        while (m_is_running.test()) {
            auto it = init_info->paths.begin();
            while (it != init_info->paths.end()) {
                if ((!std::filesystem::exists(it->first)) && (monitor_filter_flag & FileMonitor::Event::Removed)) {
                    init_info->monitor->AddQueue(init_info->path_to_watch, it->first);
                    it = init_info->paths.erase(it);
                }
                else {
                    ++it;
                }
            }

            // Check if a file was created or modified
            for (auto& file : std::filesystem::recursive_directory_iterator(init_info->path_to_watch)) {
                auto current_file_last_write_time = std::filesystem::last_write_time(file);

                // File creation
                if ((!init_info->paths.contains(file.path().string())) && (monitor_filter_flag & FileMonitor::Event::Added)) {
                    init_info->paths[file.path().string()] = current_file_last_write_time;
                    // File modification
                }
                else {
                    if ((init_info->paths[file.path().string()] != current_file_last_write_time) && (monitor_filter_flag & FileMonitor::Event::Modified)) {
                        init_info->paths[file.path().string()] = current_file_last_write_time;
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    auto FileMonitor::IsRunning() const -> bool {
        return m_is_running.test();
    }

    auto FileMonitor::Add(std::unique_ptr<FileMonitorInfo> init) -> bool {
        if (!std::filesystem::is_directory(init->path_to_watch)) {
            return false;
        }
        init->monitor = this;
        (void)m_threads.emplace_back(&FileMonitor::ThreadFunc, this, init.get());
        m_monitor_info.push_back(std::move(init));

        (void)m_is_running.test_and_set();
        return true;
    }

    auto FileMonitor::Exit() -> void {
        std::scoped_lock lock{ m_monitor_mutex };
        m_is_running.clear();
        for (auto& thread : m_threads) {
            thread.join();
        }
        m_threads.clear();

        m_monitor_info.clear();
        m_change_file_group.clear();
    }

    auto FileMonitor::Clear() -> void {
        std::scoped_lock lock{ m_monitor_mutex };
        m_change_file_group.clear();
    }

    auto FileMonitor::AddQueue(const std::filesystem::path &path, const std::filesystem::path &file_name) -> void {
        std::scoped_lock lock{ m_monitor_mutex };

        auto findIter = std::find(m_change_file_group.begin(), m_change_file_group.end(), file_name);
        if (findIter != m_change_file_group.end()) {
            return;
        }

        std::filesystem::path fin = path / file_name;
        if (m_change_file_group.end() == std::find(m_change_file_group.begin(), m_change_file_group.end(), fin)) {
            m_change_file_group.push_back(fin);
        }
    }

    auto FileMonitor::GetNumberOfChanges() -> std::size_t {
        std::scoped_lock lock{ m_monitor_mutex };
        return m_change_file_group.size();
    }

    auto FileMonitor::PopChangedFileName() -> std::filesystem::path {
        std::scoped_lock lock{ m_monitor_mutex };
        std::filesystem::path file_name= m_change_file_group.front();
        m_change_file_group.pop_front();
        return file_name;
    }

    auto GetFileMonitor() -> FileMonitor * {
        static FileMonitor g_file_monitor;
        return &g_file_monitor;
    }
}
