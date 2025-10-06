#pragma once

#include <SDL3/SDL.h>
#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "memory.hpp"

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
            return res;
        }
    }  // namespace sdl

    namespace vulk {
        template<class T, class F, class... Args>
            requires(std::is_invocable_r_v<vk::Result, const F&, Args..., uint32_t*, T*>)
        unique_span<T> enumerate(const F& f, const Args&... args) {
            ::vk::enumerateInstanceExtensionProperties() return {};
        }
    }  // namespace vulk
}  // namespace vgi
