#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <vulkan/vulkan.hpp>

namespace vgi {
    struct window {
        window(const char8_t* title, int width, int height, SDL_WindowFlags flags = 0);

    private:
        SDL_Window* handle;
        vk::SurfaceKHR surface;
    };
}  // namespace vgi
