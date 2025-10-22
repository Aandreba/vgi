#include "io.hpp"

#include <concepts>
#include <stdexcept>
#include <vgi/log.hpp>
#include <vgi/vgi.hpp>

namespace vgi::priv {
    template<iostream_interface T>
    consteval static SDL_IOStreamInterface create_iface() noexcept {
        extern "C" {
        static Sint64 SDLCALL iface_size(void* userdata) {
            try {
                return static_cast<Sint64>(reinterpret_cast<T*>(userdata)->size());
            } catch (const std::exception& e) {
                SDL_SetError(e.what());
                return -1;
            } catch (...) {
                SDL_SetError("Unhandled exception");
                return -1;
            }
        }
        }

        return SDL_IOStreamInterface{
                .version = sizeof(SDL_IOStreamInterface),
        };
    }

    iostream::iostream(std::fstream&& file) {}

    void iostream::close() && { sdl::tri(SDL_CloseIO(std::exchange(this->handle, nullptr))); }

    iostream::~iostream() noexcept {
        if (!SDL_CloseIO(this->handle)) {
            vgi::log_err("Error closing iostream: {}", SDL_GetError());
        }
    }

}  // namespace vgi::priv
