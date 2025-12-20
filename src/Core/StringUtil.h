#pragma once

#include "type.h"
#include <vector>
#include <string>
#include <charconv>

namespace StringUtil
{
    std::string trim_copy(std::string s);
    std::vector<std::string> split_any(const std::string& s, const std::string& delims);

    // Helper: extract content between the first matching '(' and ')'.
    // Returns true and writes the inner content to `out` on success, false otherwise.
    // Does not attempt to balance nested parentheses; it finds the first '(' and the first ')' after it.
    bool extractBetweenParentheses(const std::string& s, std::string& out);

    // Simple split by single character (keeps empty fields)
    std::vector<std::string> split_char(const std::string& s, char delim);

    // Parse unsigned integer without exceptions using from_chars.
    // Returns true on success and sets 'out'.
    bool parseUint(const std::string& s, u32& out);
}