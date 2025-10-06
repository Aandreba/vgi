#include "log.hpp"

#include <iostream>
#include <iterator>

namespace vgi {
    void log_msg(log_level level, std::string_view fmt, std::format_args args) noexcept {
        if (level < max_log_level) return;
        try {
            std::vformat_to(std::ostream_iterator<char>(std::clog, nullptr), fmt, args);
            std::clog << std::endl;
        } catch (...) {
        }
    }
}  // namespace vgi
