#include "log.hpp"

#include <chrono>
#include <iostream>
#include <iterator>
#include <syncstream>

#ifdef VGI_LOG_BUF_SIZE
constexpr static inline const size_t log_buf_size = VGI_LOG_BUF_SIZE;
#else
constexpr static inline const size_t log_buf_size = 4096;
#endif

namespace vgi {
    struct input_fmt {
        std::string_view fmt;
        std::format_args args;
    };

    template<size_t N>
    static std::string_view format(char (&log_buf)[N], std::string_view fmt,
                                   std::format_args args) {
        std::chrono::zoned_time current_time{std::chrono::current_zone(),
                                             std::chrono::system_clock::now()};

        std::format_to_n_result result =
                std::format_to_n(log_buf, std::size(log_buf), "[{0:%x} {0:%T} {0:%Ez}] {1}\n",
                                 current_time, input_fmt{fmt, args});

        VGI_ASSERT(result.size >= 0);
        return std::string_view{log_buf, static_cast<size_t>(result.size)};
    }

    void vlog_msg(log_level level, std::string_view fmt, std::format_args args) noexcept {
        if (level < max_log_level) return;
        try {
            char log_buf[log_buf_size];
            std::osyncstream(std::cerr) << format(log_buf, fmt, args);
        } catch (...) {
            // Ignore exceptions here
        }
    }
}  // namespace vgi

/// @brief Specialize `std::formatter` to make `vgi::input_fmt` formattable
template<>
struct std::formatter<vgi::input_fmt, char> {
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<class FmtContext>
    FmtContext::iterator format(vgi::input_fmt val, FmtContext& ctx) const {
        return std::vformat_to(ctx.out(), val.fmt, val.args);
    }
};
