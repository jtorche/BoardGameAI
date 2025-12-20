#include "StringUtil.h"

namespace StringUtil
{
    std::string trim_copy(std::string s) {
        auto notspace = [](int ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
        return s;
    }

    std::vector<std::string> split_any(const std::string& s, const std::string& delims) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s) {
            if (delims.find(c) != std::string::npos) {
                if (!cur.empty()) {
                    out.push_back(cur);
                    cur.clear();
                }
            }
            else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    }

    // Helper: extract content between the first matching '(' and ')'.
    // Returns true and writes the inner content to `out` on success, false otherwise.
    // Does not attempt to balance nested parentheses; it finds the first '(' and the first ')' after it.
    bool extractBetweenParentheses(const std::string& s, std::string& out)
    {
        size_t open = s.find('(');
        if (open == std::string::npos) return false;
        size_t close = s.find(')', open + 1);
        if (close == std::string::npos) return false;
        out.assign(s.data() + open + 1, close - (open + 1));
        return true;
    }

    // Simple split by single character (keeps empty fields)
    std::vector<std::string> split_char(const std::string& s, char delim) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s) {
            if (c == delim) {
                out.push_back(cur);
                cur.clear();
            }
            else {
                cur.push_back(c);
            }
        }
        out.push_back(cur);
        return out;
    }

    // Parse unsigned integer without exceptions using from_chars.
    // Returns true on success and sets 'out'.
    bool parseUint(const std::string& s, u32& out) {
        out = 0;
        if (s.empty()) return false;
        auto first = s.data();
        auto last = s.data() + s.size();
        auto res = std::from_chars(first, last, out);
        return (res.ec == std::errc() && res.ptr == last);
    }
}
