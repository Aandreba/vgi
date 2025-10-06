#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.hpp>

namespace vgi {
    struct window {
    private:
        SDL_Window* window;
    };
}  // namespace vgi
