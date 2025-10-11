/*! \file */
#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <type_traits>

#include "defs.hpp"
#include "memory.hpp"

#ifdef _MSC_VER
static_assert(std::is_same_v<wchar_t, std::filesystem::path::value_type>);
#define VGI_OS(x) L##x
#else
static_assert(std::is_same_v<char, std::filesystem::path::value_type>);
#define VGI_OS(x) x
#endif

namespace vgi {
    /// @brief Directory where the executable was run from.
    /// @sa [`SDL_GetBasePath`](https://wiki.libsdl.org/SDL3/SDL_GetBasePath)
    extern std::filesystem::path base_path;

    /// @brief Reads a file into a memory buffer
    /// @tparam T Elements to be read from the file
    /// @param path Path to the target file
    /// @returns A container with the contents of the file.
    /// @throws `std::runtime_error` if the file is too big or if it's size is not a multiple of the
    /// size of `T`. Any exception thrown whilst interacting with the operating system is passed
    /// through.
    template<class T>
        requires(std::is_trivially_copy_constructible_v<T> && std::is_trivially_destructible_v<T>)
    unique_span<T> read_file(const std::filesystem::path::value_type* path);

    /// @brief Reads a file into a memory buffer
    /// @tparam T Elements to be read from the file
    /// @param path Path to the target file
    /// @returns A container with the contents of the file.
    /// @throws `std::runtime_error` if the file is too big or if it's size is not a multiple of the
    /// size of `T`. Any exception thrown whilst interacting with the operating system is passed
    /// through.
    template<class T>
        requires(std::is_trivially_copy_constructible_v<T> && std::is_trivially_destructible_v<T>)
    inline unique_span<T> read_file(const std::filesystem::path& path) {
        return read_file<T>(path.c_str());
    }

    /// @brief Access to the list of environment variables
    /// @sa [`std::getenv`](https://en.cppreference.com/w/cpp/utility/program/getenv.html)
    [[nodiscard]] bool hasenv(const std::filesystem::path::value_type* name) noexcept;

    /// @brief Access to the list of environment variables
    /// @sa [`std::getenv`](https://en.cppreference.com/w/cpp/utility/program/getenv.html)
    [[nodiscard]] std::optional<std::filesystem::path::string_type> getenv(
            const std::filesystem::path::value_type* name) noexcept;
}  // namespace vgi
