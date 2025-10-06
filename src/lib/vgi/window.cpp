#include "window.hpp"

#include <SDL3/SDL_vulkan.h>

#include "vgi.hpp"

// Make sure we only setup the window to use Vulkan.
constexpr static inline const SDL_WindowFlags EXCLUDED_FLAGS = SDL_WINDOW_OPENGL | SDL_WINDOW_METAL;
constexpr static inline const SDL_WindowFlags REQUIRED_FLAGS = SDL_WINDOW_VULKAN;

namespace vgi {
    extern vk::Instance instance;

    window::window(const char8_t* title, int width, int height, SDL_WindowFlags flags) :
        handle(sdl::tri(SDL_CreateWindow(reinterpret_cast<const char*>(title), width, height,
                                         (flags & ~EXCLUDED_FLAGS) | REQUIRED_FLAGS))) {
        VkSurfaceKHR surface;
        sdl::tri(SDL_Vulkan_CreateSurface(this->handle, static_cast<VkInstance>(instance), nullptr,
                                          &surface));

        VGI_ASSERT(surface != VK_NULL_HANDLE);
        this->surface = surface;
    }

    window::~window() noexcept {
        if (this->surface && this->handle)
            SDL_Vulkan_DestroySurface(instance, this->surface, nullptr);
        if (this->handle) SDL_DestroyWindow(this->handle);
    }
}  // namespace vgi
