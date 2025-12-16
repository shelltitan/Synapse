#pragma once
#include <filesystem>
#include <toml++/toml.hpp>

namespace Synapse::Config {
    class Config {
    public:
        explicit Config(std::filesystem::path& path_to_config);

        explicit Config(std::filesystem::path&& path_to_config);
        ~Config() = default;

        template <typename T>
        auto ReadValue(std::string_view section, std::string_view field, T default_value) const -> T {
            return config_table[section][field].value_or(default_value);
        }

    private:
        toml::table config_table;
    };

    static auto CreateSection(std::string_view section, std::ofstream &config_file) -> void {
        config_file << std::format("[{}]", section) << std::endl;
    }
    template <typename T>
    auto SaveValue(std::string_view field, std::ofstream &config_file, T value) -> void {
        config_file << std::format("{} = {}", field, value) << std::endl;
    }
}
