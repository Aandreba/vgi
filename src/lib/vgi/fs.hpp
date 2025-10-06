/*! \file */
#pragma once

#include <cstddef>
#include <filesystem>
#include <type_traits>

#include "defs.hpp"
#include "memory.hpp"

namespace vgi {

    /// @brief Reads a file into a memory buffer
    /// @tparam T Elements to be read from the file
    /// @param path Path to the target file
    /// @returns A container with the contents of the file.
    /// @throws `std::exception` if the file is too big or if it's size is not a multiple of the
    /// size of `T`. Any exception thrown whilst interacting with the operating system is passed
    /// through.
    template<class T>
        requires(std::is_trivially_copy_constructible_v<T> && std::is_trivially_destructible_v<T>)
    unique_span<T> read_file(const std::filesystem::path::value_type* VGI_RESTRICT path);

}  // namespace vgi
