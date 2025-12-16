#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace Synapse::FileSystem {
    auto ReadFile(const std::string &path) -> std::vector<unsigned char>;
    auto GetAbsoluteExecutablePath() -> std::filesystem::path;
    auto GetAbsoluteExecutableDirectory() -> std::filesystem::path;
}
