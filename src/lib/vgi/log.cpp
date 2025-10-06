#include "log.hpp"

#include <chrono>
#include <iostream>
#include <iterator>

namespace vgi {
    void vlog_msg(log_level level, std::string_view fmt, std::format_args args) noexcept {
        if (level < max_log_level) return;
        try {
            std::chrono::zoned_time current_time{std::chrono::current_zone(),
                                                 std::chrono::system_clock::now()};
            auto it = std::format_to(std::ostream_iterator<char>(std::clog, nullptr),
                                     "[{0:%x} {0:%T} {0:%Ez}] ", current_time);
            it = std::vformat_to(it, fmt, args);
            std::clog << std::endl;
        } catch (...) {
        }
    }
}  // namespace vgi
