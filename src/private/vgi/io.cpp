#include "io.hpp"

#include <concepts>
#include <fstream>
#include <stdexcept>
#include <vgi/log.hpp>
#include <vgi/vgi.hpp>

namespace vgi::priv {
    template<iostream_interface T>
    static Sint64 SDLCALL iface_size(void* userdata) {
        try {
            return static_cast<Sint64>(reinterpret_cast<T*>(userdata)->size());
        } catch (const std::exception& e) {
            SDL_SetError("%s", e.what());
            return -1;
        } catch (...) {
            SDL_SetError("Unhandled exception");
            return -1;
        }
    }

    template<iostream_interface T>
    static Sint64 SDLCALL iface_seek(void* userdata, Sint64 offset, SDL_IOWhence whence) {
        try {
            return static_cast<Sint64>(reinterpret_cast<T*>(userdata)->seek(offset, whence));
        } catch (const std::exception& e) {
            SDL_SetError("%s", e.what());
            return -1;
        } catch (...) {
            SDL_SetError("Unhandled exception");
            return -1;
        }
    }

    template<iostream_interface T>
    static size_t SDLCALL iface_read(void* userdata, void* ptr, size_t size, SDL_IOStatus* status) {
        try {
            return static_cast<size_t>(reinterpret_cast<T*>(userdata)->read(
                    std::span<std::byte>{static_cast<std::byte*>(ptr), size}, status));
        } catch (const std::exception& e) {
            if (status) *status = SDL_IO_STATUS_ERROR;
            SDL_SetError("%s", e.what());
            return 0;
        } catch (...) {
            if (status) *status = SDL_IO_STATUS_ERROR;
            SDL_SetError("Unhandled exception");
            return 0;
        }
    }

    template<iostream_interface T>
    static size_t SDLCALL iface_write(void* userdata, const void* ptr, size_t size,
                                      SDL_IOStatus* status) {
        try {
            return static_cast<size_t>(reinterpret_cast<T*>(userdata)->write(
                    std::span<const std::byte>{static_cast<const std::byte*>(ptr), size}, status));
        } catch (const std::exception& e) {
            if (status) *status = SDL_IO_STATUS_ERROR;
            SDL_SetError("%s", e.what());
            return 0;
        } catch (...) {
            if (status) *status = SDL_IO_STATUS_ERROR;
            SDL_SetError("Unhandled exception");
            return 0;
        }
    }

    template<iostream_interface T>
    static bool SDLCALL iface_flush(void* userdata, SDL_IOStatus* status) {
        try {
            reinterpret_cast<T*>(userdata)->flush(status);
            return true;
        } catch (const std::exception& e) {
            if (status) *status = SDL_IO_STATUS_ERROR;
            SDL_SetError("%s", e.what());
            return false;
        } catch (...) {
            if (status) *status = SDL_IO_STATUS_ERROR;
            SDL_SetError("Unhandled exception");
            return false;
        }
    }

    template<iostream_interface T>
    static bool SDLCALL iface_close(void* userdata) {
        T* iface = reinterpret_cast<T*>(userdata);
        try {
            try {
                iface->close();
            } catch (...) {
                std::destroy_at(iface);
                throw;
            }
            std::destroy_at(iface);
            return true;
        } catch (const std::exception& e) {
            SDL_SetError("%s", e.what());
            return false;
        } catch (...) {
            SDL_SetError("Unhandled exception");
            return false;
        }
    }

    template<std::derived_from<std::iostream> T>
    struct iostream_iface final : public T {
        template<class... Args>
            requires(std::is_constructible_v<T, Args...>)
        iostream_iface(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) :
            T(std::forward<Args>(args)...) {
            std::ios::exceptions(std::ios::badbit | std::ios::failbit);
        }

        std::streamoff size() {
            std::streamoff prev = std::istream::tellg();
            std::istream::seekg(0, std::ios::end);
            std::streamoff pos;
            try {
                pos = std::istream::tellg();
            } catch (...) {
                std::istream::seekg(prev, std::ios::beg);
                throw;
            }
            std::istream::seekg(prev, std::ios::beg);
            return pos;
        }

        std::streamoff seek(int64_t offset, SDL_IOWhence whence) {
            if (offset == 0 && whence == SDL_IO_SEEK_CUR) {
                return std::istream::tellg();
            }

            std::ios::seekdir dir;
            switch (whence) {
                case SDL_IO_SEEK_SET:
                    dir = std::ios::beg;
                    break;
                case SDL_IO_SEEK_CUR:
                    dir = std::ios::cur;
                    break;
                case SDL_IO_SEEK_END:
                    dir = std::ios::end;
                    break;
                default:
                    throw vgi_error{"Unknown seek whence"};
            }

            std::istream::seekg(offset, dir);
            std::ostream::seekp(offset, dir);
            return std::istream::tellg();
        }

        std::streamsize read(std::span<std::byte> dst, SDL_IOStatus* status) {
            std::istream::read(reinterpret_cast<char*>(dst.data()), dst.size());
            if (status && std::ios::eof()) *status = SDL_IO_STATUS_EOF;
            std::streamsize count = std::istream::gcount();
            std::ostream::seekp(count, std::ios::cur);
            return count;
        }

        size_t write(std::span<const std::byte> dst, SDL_IOStatus* status) {
            std::ostream::write(reinterpret_cast<const char*>(dst.data()), dst.size());
            if (status && std::ios::eof()) *status = SDL_IO_STATUS_EOF;
            std::istream::seekg(dst.size());
            return dst.size();
        }

        void flush(SDL_IOStatus* status) {
            std::istream::sync();
            std::ostream::flush();
        }

        void close() {
            if constexpr (std::derived_from<T, std::fstream>) {
                std::fstream::close();
            } else if constexpr (std::derived_from<T, std::ifstream>) {
                std::ifstream::close();
            } else if constexpr (std::derived_from<T, std::ofstream>) {
                std::ofstream::close();
            }
        }
    };

    using fstream_iface = iostream_iface<std::fstream>;
    static_assert(iostream_interface<fstream_iface>);

    template<iostream_interface T>
    constexpr static inline const SDL_IOStreamInterface stream_iface{
            .version = sizeof(SDL_IOStreamInterface),
            .size = iface_size<T>,
            .seek = iface_seek<T>,
            .read = iface_read<T>,
            .write = iface_write<T>,
            .flush = iface_flush<T>,
            .close = iface_close<T>,
    };

    iostream::iostream(std::span<std::byte> bytes) :
        handle(sdl::tri(SDL_IOFromMem(bytes.data(), bytes.size()))) {}

    iostream::iostream(std::span<const std::byte> bytes) :
        handle(sdl::tri(SDL_IOFromConstMem(bytes.data(), bytes.size()))) {}

    iostream::iostream(std::fstream&& file) {
        fstream_iface* iface = new fstream_iface{std::move(file)};
        try {
            this->handle = sdl::tri(SDL_OpenIO(&stream_iface<fstream_iface>, iface));
        } catch (...) {
            delete iface;
            throw;
        }
    }

    iostream::iostream(const std::filesystem::path::value_type* path,
                       std::ios_base::openmode mode) : iostream(std::fstream{path, mode}) {}

    void iostream::close() && { sdl::tri(SDL_CloseIO(std::exchange(this->handle, nullptr))); }

    iostream::~iostream() noexcept {
        if (!SDL_CloseIO(this->handle)) {
            vgi::log_err("Error closing iostream: {}", SDL_GetError());
        }
    }

}  // namespace vgi::priv
