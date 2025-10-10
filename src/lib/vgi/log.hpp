/*! \file */
#pragma once

#include <cstdint>
#include <format>
#include <string_view>

#include "defs.hpp"

namespace vgi {
    /// @brief An enum representing the available verbosity levels of the logger.
    enum struct log_level : uint8_t {
        /// @brief Designates very low priority, often extremely verbose, information.
        verbose,
        /// @brief Designates lower priority information.
        debug,
        /// @brief Designates useful information.
        info,
        /// @brief Designates hazardous situations.
        warn,
        /// @brief Designates very serious errors.
        error,
    };

    /// @brief Maximum log level that will be processed.
    /// If a message with a looser log level is sent, it will **not** be processed.
#ifdef VGI_LOG_LEVEL
    constexpr static inline const log_level max_log_level =
            static_cast<VGI_LOG_LEVEL>(VGI_LOG_LEVEL);
#elif !defined(NDEBUG)
    constexpr static inline const log_level max_log_level = log_level::debug;
#else
    constexpr static inline const log_level max_log_level = log_level::error;
#endif

    /// @brief Logs the message by formatting the arguments according to the format string.
    /// @param level Level of the logged message
    /// @param fmt Formatting string
    /// @param args Arguments to be formatted
    void vlog_msg(log_level level, std::string_view fmt, std::format_args args) noexcept;

    /// @brief Logs the message by formatting the arguments according to the format string.
    /// @param level Level of the logged message
    /// @param fmt Formatting string
    /// @param args Arguments to be formatted
    template<class... Args>
    VGI_FORCEINLINE void log_msg(log_level level, std::format_string<Args...> fmt,
                                 Args&&... args) noexcept {
        return vlog_msg(level, fmt.get(), std::make_format_args(args...));
    }

    /// @brief Logs the message by formatting the arguments according to the format string.
    /// @tparam level Level of the logged message
    /// @param fmt Formatting string
    /// @param args Arguments to be formatted
    template<log_level level>
    VGI_FORCEINLINE void vlog_msg(std::string_view fmt, std::format_args args) noexcept {
        if constexpr (level < max_log_level) return;
        vlog_msg(level, fmt, args);
    }

    /// @brief Logs the message by formatting the arguments according to the format string.
    /// @tparam level Level of the logged message
    /// @param fmt Formatting string
    /// @param args Arguments to be formatted
    template<log_level level, class... Args>
    VGI_FORCEINLINE void log_msg(std::format_string<Args...> fmt, Args&&... args) noexcept {
        if constexpr (level < max_log_level) return;
        vlog_msg(level, fmt.get(), std::make_format_args(args...));
    }

    /// @brief Logs the verbose message by formatting the arguments according to the format string.
    /// @param fmt Formatting string
    /// @param args Arguments to be formatted
    template<class... Args>
    VGI_FORCEINLINE void log_verbose(std::format_string<Args...> fmt, Args&&... args) noexcept {
        log_msg<log_level::verbose>(fmt, std::forward<Args>(args)...);
    }

    /// @brief Logs the debug message by formatting the arguments according to the format string.
    /// @param fmt Formatting string
    /// @param args Arguments to be formatted
    template<class... Args>
    VGI_FORCEINLINE void log_dbg(std::format_string<Args...> fmt, Args&&... args) noexcept {
        log_msg<log_level::debug>(fmt, std::forward<Args>(args)...);
    }

    /// @brief Logs the information message by formatting the arguments according to the format
    /// string.
    /// @param fmt Formatting string
    /// @param args Arguments to be formatted
    template<class... Args>
    VGI_FORCEINLINE void log(std::format_string<Args...> fmt, Args&&... args) noexcept {
        log_msg<log_level::info>(fmt, std::forward<Args>(args)...);
    }

    /// @brief Logs the warning message by formatting the arguments according to the format string.
    /// @param fmt Formatting string
    /// @param args Arguments to be formatted
    template<class... Args>
    VGI_FORCEINLINE void log_warn(std::format_string<Args...> fmt, Args&&... args) noexcept {
        log_msg<log_level::warn>(fmt, std::forward<Args>(args)...);
    }

    /// @brief Logs the error message by formatting the arguments according to the format string.
    /// @param fmt Formatting string
    /// @param args Arguments to be formatted
    template<class... Args>
    VGI_FORCEINLINE void log_err(std::format_string<Args...> fmt, Args&&... args) noexcept {
        log_msg<log_level::error>(fmt, std::forward<Args>(args)...);
    }
}  // namespace vgi
