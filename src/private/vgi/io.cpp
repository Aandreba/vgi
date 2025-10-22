#include "io.hpp"

#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <vgi/log.hpp>
#include <vgi/vgi.hpp>

using openmode_t = std::ios_base::openmode;
using uopenmode_t = std::make_unsigned_t<openmode_t>;

constexpr openmode_t access_mode_mask = std::ios_base::binary | std::ios_base::in |
                                        std::ios_base::out | std::ios_base::trunc |
                                        std::ios_base::app;

constexpr size_t access_array_size = static_cast<size_t>(1)
                                     << std::bit_width(static_cast<uopenmode_t>(access_mode_mask));

consteval std::array<const char*, access_array_size> generate_file_mode_array() {
    std::array<const char*, access_array_size> res;
    std::fill(res.begin(), res.end(), nullptr);
    res[std::ios_base::in] = "r";
    res[std::ios_base::binary | std::ios_base::in] = "rb";
    res[std::ios_base::in | std::ios_base::out] = "r+";
    res[std::ios_base::binary | std::ios_base::in | std::ios_base::out] = "r+b";
    res[std::ios_base::out] = res[std::ios_base::out | std::ios_base::trunc] = "w";
    res[std::ios_base::binary | std::ios_base::out] =
            res[std::ios_base::binary | std::ios_base::out | std::ios_base::trunc] = "wb";
    res[std::ios_base::in | std::ios_base::out | std::ios_base::trunc] = "w+";
    res[std::ios_base::binary | std::ios_base::in | std::ios_base::out | std::ios_base::trunc] =
            "w+b";
    res[std::ios_base::out | std::ios_base::app] = res[std::ios_base::app] = "a";
    res[std::ios_base::binary | std::ios_base::out | std::ios_base::app] =
            res[std::ios_base::binary | std::ios_base::app] = "ab";
    res[std::ios_base::in | std::ios_base::out | std::ios_base::app] = res[std::ios_base::app] =
            "a+";
    res[std::ios_base::binary | std::ios_base::in | std::ios_base::out | std::ios_base::app] =
            res[std::ios_base::binary | std::ios_base::app] = "a+b";
    return res;
}

constexpr const char* file_mode(std::ios_base::openmode mode) noexcept {
    constexpr auto file_mode_table = generate_file_mode_array();
}

namespace vgi::priv {
    iostream::iostream(std::span<std::byte> bytes) :
        handle(sdl::tri(SDL_IOFromMem(bytes.data(), bytes.size()))) {}

    iostream::iostream(std::span<const std::byte> bytes) :
        handle(sdl::tri(SDL_IOFromConstMem(bytes.data(), bytes.size()))) {}

#ifdef _MSC_VER
    iostream::iostream(const std::filesystem::path& path, std::ios_base::openmode mode) {
        this->handle = sdl::tri(SDL_IOFromFile(nullptr, nullptr));
    }

    iostream::iostream(const std::filesystem::path::value_type* path,
                       std::ios_base::openmode mode) {
        this->handle = sdl::tri(SDL_IOFromFile(nullptr, nullptr));
    }
#else
    iostream::iostream(const std::filesystem::path& path, std::ios_base::openmode mode) {
        this->handle = sdl::tri(SDL_IOFromFile(nullptr, nullptr));
    }

    iostream::iostream(const std::filesystem::path::value_type* path,
                       std::ios_base::openmode mode) {
        this->handle = sdl::tri(SDL_IOFromFile(nullptr, nullptr));
    }
#endif

    void iostream::close() && { sdl::tri(SDL_CloseIO(std::exchange(this->handle, nullptr))); }

    iostream::~iostream() noexcept {
        if (!SDL_CloseIO(this->handle)) {
            vgi::log_err("Error closing iostream: {}", SDL_GetError());
        }
    }

}  // namespace vgi::priv
