#pragma once
#include <string>
#include <vector>

namespace Synapse::Utility {
    /**
     * @brief Splits a string into tokens based on a specified delimiter.
     *
     * Extracts substrings from the input `str` by splitting it at each occurrence
     * of `delimiter`. The resulting tokens are appended to `tokens`.
     *
     * @param str        The input string to tokenize.
     * @param delimiter  The delimiter string used for splitting.
     * @param tokens     Vector to which the resulting tokens will be appended.
     */
    auto FilterString(const std::string &str, const std::string &delimiter, std::vector<std::string> &tokens) -> void;
}
