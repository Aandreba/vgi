#pragma once

#include <SDL3/SDL.h>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <utility>

namespace vgi::priv {
    template<class T>
    concept iostream_interface =
            requires(T* ptr, std::span<std::byte> dst, std::span<const std::byte> src,
                     Sint64 offset, SDL_IOWhence whence, SDL_IOStatus* status) {
                { ptr->size() } -> std::convertible_to<uint64_t>;
                { ptr->seek(offset, whence) } -> std::convertible_to<uint64_t>;
                { ptr->read(dst, status) } -> std::convertible_to<size_t>;
                { ptr->write(src, status) } -> std::convertible_to<size_t>;
                { ptr->flush(status) } -> std::same_as<void>;
                { ptr->close() } -> std::same_as<void>;
            };

    struct iostream {
        iostream(std::fstream&& file);

        iostream(const iostream&) = delete;
        iostream& operator=(const iostream&) = delete;

        iostream(iostream&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}
        iostream& operator=(iostream&& other) noexcept {
            if (this->handle == other.handle) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        void close() &&;
        ~iostream() noexcept;

    private:
        SDL_IOStream* handle;
    };
}  // namespace vgi::priv
