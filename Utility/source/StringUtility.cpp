#include <StringUtility.hpp>

namespace Synapse::Utility {
    auto FilterString(const std::string &str, const std::string &delimiter, std::vector<std::string> &tokens) -> void {
        std::string_view view{ str };
        std::size_t pos{};

        while ((pos = view.find(delimiter)) != std::string_view::npos) {
            (void)tokens.emplace_back(view.substr(0U, pos));
            view.remove_prefix(pos + delimiter.length());
        }

        (void)tokens.emplace_back(view);
    }
}
