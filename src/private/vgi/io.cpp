#include "io.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <vgi/log.hpp>
#include <vgi/vgi.hpp>

#ifdef _MSC_VER
#include <windows.h>
#endif

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
    res[std::ios_base::in | std::ios_base::out | std::ios_base::app] =
            res[std::ios_base::in | std::ios_base::app] = "a+";
    res[std::ios_base::binary | std::ios_base::in | std::ios_base::out | std::ios_base::app] =
            res[std::ios_base::binary | std::ios_base::in | std::ios_base::app] = "a+b";
    return res;
}

constexpr static inline const auto file_mode_table = generate_file_mode_array();
constexpr const char* file_mode(std::ios_base::openmode mode) noexcept {
    return file_mode_table[mode & ~std::ios_base::ate];
}

namespace vgi::priv {
    iostream::iostream(std::span<std::byte> bytes) :
        handle(sdl::tri(SDL_IOFromMem(bytes.data(), bytes.size()))) {}

    iostream::iostream(std::span<const std::byte> bytes) :
        handle(sdl::tri(SDL_IOFromConstMem(bytes.data(), bytes.size()))) {}

#ifdef _MSC_VER
    static std::unique_ptr<char[]> encode_path(const std::filesystem::path::value_type* path,
                                               size_t path_len) {
        std::optional<int> utf16_len = math::check_cast<int>(path_len);
        if (!utf16_len) throw vgi_error{"path is too long"};
        int utf8_cap = math::sat_add(math::sat_mul(3, *utf16_len), 1);
        auto utf8_buf = std::make_unique_for_overwrite<char[]>(utf8_cap);
        int utf8_len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, *utf16_len,
                                           utf8_buf.get(), utf8_cap - 1, NULL, NULL);

        if (utf8_len == 0) {
            switch (GetLastError()) {
                case ERROR_INSUFFICIENT_BUFFER:
                    VGI_UNREACHABLE;
                    break;
                case ERROR_NO_UNICODE_TRANSLATION:
                    throw vgi_error{"invalid path"};
                case ERROR_INVALID_PARAMETER:
                case ERROR_INVALID_FLAGS:
                default:
                    throw vgi_error{"unexpected error"};
            }
        }

        VGI_ASSERT(utf8_len < utf8_cap);
        utf8_buf[utf8_len] = 0;
        return utf8_buf;
    }

    iostream::iostream(const std::filesystem::path& path, std::ios_base::openmode mode) {
        auto utf8_buf = encode_path(path.c_str(), path.native().size());
        this->handle = sdl::tri(SDL_IOFromFile(utf8_buf.get(), file_mode(mode)));
        if (mode & std::ios_base::ate) {
            if (SDL_SeekIO(this->handle, 0, SDL_IO_SEEK_END) < 0) {
                throw sdl_error{};
            }
        }
    }

    iostream::iostream(const std::filesystem::path::value_type* path,
                       std::ios_base::openmode mode) {
        auto utf8_buf = encode_path(path, std::wcslen(path));
        this->handle = sdl::tri(SDL_IOFromFile(utf8_buf.get(), file_mode(mode)));
        if (mode & std::ios_base::ate) {
            if (SDL_SeekIO(this->handle, 0, SDL_IO_SEEK_END) < 0) {
                throw sdl_error{};
            }
        }
    }
#else
    iostream::iostream(const std::filesystem::path& path, std::ios_base::openmode mode) :
        iostream(path.c_str(), mode) {}

    iostream::iostream(const std::filesystem::path::value_type* path,
                       std::ios_base::openmode mode) {
        this->handle = sdl::tri(SDL_IOFromFile(path, file_mode(mode)));
        if (mode & std::ios_base::ate) {
            if (SDL_SeekIO(this->handle, 0, SDL_IO_SEEK_END) < 0) {
                throw sdl_error{};
            }
        }
    }
#endif

    void iostream::close() && { sdl::tri(SDL_CloseIO(std::exchange(this->handle, nullptr))); }

    iostream::~iostream() noexcept {
        if (!SDL_CloseIO(this->handle)) {
            vgi::log_err("Error closing iostream: {}", SDL_GetError());
        }
    }

}  // namespace vgi::priv
