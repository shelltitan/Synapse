#include <Config.hpp>


namespace Synapse::Config {
    Config::Config(std::filesystem::path &path_to_config) {
        config_table = toml::parse_file(path_to_config.c_str());
    }

    Config::Config(std::filesystem::path &&path_to_config) {
        config_table = toml::parse_file(path_to_config.c_str());
    }
}
