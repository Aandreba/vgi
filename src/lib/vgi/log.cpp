#include "log.hpp"

#include <chrono>
#include <codecvt>
#include <iostream>
#include <iterator>
#include <locale>
#include <memory>
#include <syncstream>

#ifdef VGI_LOG_BUF_SIZE
constexpr static inline const size_t log_buf_size = VGI_LOG_BUF_SIZE;
#else
constexpr static inline const size_t log_buf_size = 4096;
#endif

namespace vgi {
    std::vector<std::unique_ptr<logger>> loggers;

    //! @cond Doxygen_Suppress
    struct input_fmt {
        std::string_view fmt;
        std::format_args args;
    };
    struct winput_fmt {
        std::wstring_view fmt;
        std::wformat_args args;
    };
    //! @endcond

    template<size_t N>
    static std::string_view format(char (&log_buf)[N], std::string_view fmt,
                                   std::format_args args) {
        // Some compilers (looking at you clang) still don't support calendars & time zones
#if __cpp_lib_chrono >= 201907L
        std::chrono::zoned_time current_time{std::chrono::current_zone(),
                                             std::chrono::system_clock::now()};
#else
        std::chrono::sys_time current_time = std::chrono::system_clock::now();
#endif

        std::format_to_n_result result =
                std::format_to_n(log_buf, std::size(log_buf), "[{0:%x} {0:%T} {0:%Ez}] {1}\n",
                                 current_time, input_fmt{fmt, args});

        VGI_ASSERT(result.size >= 0);
        return std::string_view{log_buf, static_cast<size_t>(result.size)};
    }

    template<size_t N>
    static std::wstring_view format(wchar_t (&log_buf)[N], std::wstring_view fmt,
                                    std::wformat_args args) {
        // Some compilers (looking at you clang) still don't support calendars & time zones
#if __cpp_lib_chrono >= 201907L
        std::chrono::zoned_time current_time{std::chrono::current_zone(),
                                             std::chrono::system_clock::now()};
#else
        std::chrono::sys_time current_time = std::chrono::system_clock::now();
#endif

        std::format_to_n_result result =
                std::format_to_n(log_buf, std::size(log_buf), L"[{0:%x} {0:%T} {0:%Ez}] {1}\n",
                                 current_time, winput_fmt{fmt, args});

        VGI_ASSERT(result.size >= 0);
        return std::wstring_view{log_buf, static_cast<size_t>(result.size)};
    }

    void vlog_msg(log_level level, std::string_view fmt, std::format_args args) noexcept {
        if (level < max_log_level) return;
        try {
            char log_buf[log_buf_size];
            std::string_view msg = format(log_buf, fmt, args);

            for (std::unique_ptr<logger>& logger: loggers) {
                try {
                    logger->log(msg);
                } catch (...) {
                    // Ignore exceptions here
                }
            }
        } catch (...) {
            // Ignore exceptions here
        }
    }

    void vlog_msg(log_level level, std::wstring_view fmt, std::wformat_args args) noexcept {
        if (level < max_log_level) return;
        try {
            wchar_t log_buf[log_buf_size / sizeof(wchar_t)];
            std::wstring_view msg = format(log_buf, fmt, args);

            for (std::unique_ptr<logger>& logger: loggers) {
                try {
                    logger->log(msg);
                } catch (...) {
                    // Ignore exceptions here
                }
            }
        } catch (...) {
            // Ignore exceptions here
        }
    }

    void logger::log(std::wstring_view msg) {
        struct local_codecvt : public std::codecvt<wchar_t, char, std::mbstate_t> {};
        std::wstring_convert<local_codecvt> cvt;
        this->log(cvt.to_bytes(&*msg.begin(), &*msg.end()));
    }

    void default_logger::log(std::string_view msg) {
#ifdef __cpp_lib_syncbuf
        std::osyncstream(std::cerr) << msg;
#else
        std::lock_guard log_guard{this->lock};
        std::cerr << msg;
#endif
    }

    void default_logger::log(std::wstring_view msg) {
#ifdef __cpp_lib_syncbuf
        std::wosyncstream(std::wcerr) << msg;
#else
        std::lock_guard log_guard{this->lock};
        std::wcerr << msg;
#endif
    }

    void add_logger(std::unique_ptr<logger>&& logger) { loggers.push_back(std::move(logger)); }
}  // namespace vgi

//! @cond Doxygen_Suppress
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
template<>
struct std::formatter<vgi::winput_fmt, wchar_t> {
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<class FmtContext>
    FmtContext::iterator format(vgi::winput_fmt val, FmtContext& ctx) const {
        return std::vformat_to(ctx.out(), val.fmt, val.args);
    }
};
//! @endcond
