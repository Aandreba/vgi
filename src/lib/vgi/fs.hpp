#pragma once

#include <cstddef>
#include <filesystem>
#include <type_traits>

#include "defs.hpp"
#include "memory.hpp"

namespace vgi {
    template<class T>
        requires(std::is_trivially_copy_constructible_v<T> && std::is_trivially_destructible_v<T>)
    unique_span<T> read_file(const std::filesystem::path::value_type* VGI_RESTRICT path);

}  // namespace vgi
