#include "window.hpp"

#include <SDL3/SDL_vulkan.h>

#include "vgi.hpp"

// Make sure we only setup the window to use Vulkan.
constexpr static inline const SDL_WindowFlags EXCLUDED_FLAGS = SDL_WINDOW_OPENGL | SDL_WINDOW_METAL;
constexpr static inline const SDL_WindowFlags REQUIRED_FLAGS = SDL_WINDOW_VULKAN;

namespace vgi {
    window::window(const char8_t* title, int width, int height, SDL_WindowFlags flags) :
        handle(sdl::tri(SDL_CreateWindow(reinterpret_cast<const char*>(title), width, height,
                                         (flags & ~EXCLUDED_FLAGS) | REQUIRED_FLAGS)))
    // ,surface(sdl::tri(SDL_Vulkan_CreateSurface(this->handle)))
    {}
}  // namespace vgi
