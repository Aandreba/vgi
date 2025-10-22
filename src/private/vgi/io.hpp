#pragma once

#include <SDL3/SDL.h>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <utility>

namespace vgi::priv {
    template<class T>
    concept iostream_interface =
            requires(T* ptr, std::span<std::byte> dst, std::span<const std::byte> src,
                     int64_t offset, SDL_IOWhence whence, SDL_IOStatus* status) {
                { ptr->size() } -> std::convertible_to<int64_t>;
                { ptr->seek(offset, whence) } -> std::convertible_to<int64_t>;
                { ptr->read(dst, status) } -> std::convertible_to<size_t>;
                { ptr->write(src, status) } -> std::convertible_to<size_t>;
                { ptr->flush(status) } -> std::same_as<void>;
                { ptr->close() } -> std::same_as<void>;
            };

    struct iostream {
        iostream(std::span<std::byte> bytes);
        iostream(std::span<const std::byte> bytes);
        iostream(std::fstream&& file);

        iostream(const std::filesystem::path::value_type* path,
                 std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out);
        iostream(const std::filesystem::path& path,
                 std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) :
            iostream(path.c_str(), mode) {}

        iostream(const iostream&) = delete;
        iostream& operator=(const iostream&) = delete;

        iostream(iostream&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}
        iostream& operator=(iostream&& other) noexcept {
            if (this == &other) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        constexpr operator SDL_IOStream*() const noexcept { return this->handle; }

        SDL_IOStream* release() && noexcept { return std::exchange(this->handle, nullptr); }
        void close() &&;

        ~iostream() noexcept;

    private:
        SDL_IOStream* handle;
    };
}  // namespace vgi::priv
