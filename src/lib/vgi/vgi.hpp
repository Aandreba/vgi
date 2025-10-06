#pragma once

#include <SDL3/SDL.h>
#include <stdexcept>

namespace vgi {
    void init();
    void quit() noexcept;

    struct sdl_error : public std::runtime_error {
        sdl_error() : std::runtime_error(SDL_GetError()) {}
    };

    namespace sdl {
        inline void tri(bool res) {
            if (!res) [[unlikely]]
                throw sdl_error{};
        }

        template<class T>
        inline T* tri(T* res) {
            if (!res) [[unlikely]]
                throw sdl_error{};
            return *res;
        }
    }  // namespace sdl
}  // namespace vgi
