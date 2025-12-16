#include <FileUtils.hpp>
#include <algorithm>
#include <array>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace Synapse::FileSystem {
    auto ReadFile(const std::string &path) -> std::vector<unsigned char> {
        std::vector<unsigned char> ret{};

        const std::filesystem::path file_path{ path };

        const std::size_t file_size{ std::filesystem::file_size(file_path) };
        ret.resize(file_size);

        std::basic_ifstream<unsigned char> input_stream{ file_path };
        (void)input_stream.read(&ret[0U], file_size);

        return ret;
    }

    auto GetAbsoluteExecutablePath() -> std::filesystem::path {
#if defined(_MSC_VER)
        std::array<wchar_t, FILENAME_MAX> path{};
        (void)GetModuleFileNameW(nullptr, path.data(), FILENAME_MAX);
        return std::filesystem::path(path.data());
#else
            std::array<char, FILENAME_MAX> path{};
            ssize_t count = readlink("/proc/self/exe", path.data(), FILENAME_MAX);
            return std::filesystem::path(std::string(path.data(), (count > 0) ? count : 0));
#endif
    }

    auto GetAbsoluteExecutableDirectory() -> std::filesystem::path {
#if defined(_MSC_VER)
        std::array<wchar_t, FILENAME_MAX> path{};
        (void)GetModuleFileNameW(nullptr, path.data(), FILENAME_MAX);
        return std::filesystem::path(path.data()).parent_path();
#else
            std::array<char, FILENAME_MAX> path{};
            ssize_t count = readlink("/proc/self/exe", path.data(), FILENAME_MAX);
            return std::filesystem::path(std::string(path.data(), (count > 0) ? count : 0)).parent_path();
#endif
    }
}
