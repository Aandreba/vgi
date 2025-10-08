#include "window.hpp"

#include <SDL3/SDL_vulkan.h>

#include "vgi.hpp"

// Make sure we only setup the window to use Vulkan.
constexpr static inline const SDL_WindowFlags EXCLUDED_FLAGS = SDL_WINDOW_OPENGL | SDL_WINDOW_METAL;
constexpr static inline const SDL_WindowFlags REQUIRED_FLAGS = SDL_WINDOW_VULKAN;
// Regular, 8-bit color format used by most devices
constexpr static inline const vk::SurfaceFormatKHR SRGB_FORMAT{vk::Format::eB8G8R8A8Unorm,
                                                               vk::ColorSpaceKHR::eSrgbNonlinear};
// High definition, 10-bit color format
constexpr static inline const vk::SurfaceFormatKHR HDR10_FORMAT{vk::Format::eA2B10G10R10UnormPack32,
                                                                vk::ColorSpaceKHR::eHdr10St2084EXT};

namespace vgi {
    extern vk::Instance instance;

    window::window(const device& device, const char8_t* title, int width, int height,
                   SDL_WindowFlags flags, bool vsync) :
        handle(sdl::tri(SDL_CreateWindow(reinterpret_cast<const char*>(title), width, height,
                                         (flags & ~EXCLUDED_FLAGS) | REQUIRED_FLAGS))),
        physical(device) {
        VkSurfaceKHR surface;
        sdl::tri(SDL_Vulkan_CreateSurface(this->handle, static_cast<VkInstance>(instance), nullptr,
                                          &surface));
        VGI_ASSERT(surface != VK_NULL_HANDLE);
        this->surface = surface;

        std::optional<uint32_t> queue_family =
                device.select_queue_family(this->surface, SRGB_FORMAT, vsync);
        if (!queue_family) throw std::runtime_error{"No valid queue family found"};

        this->logical = device->createDevice(vk::DeviceCreateInfo{});
    }

    window::~window() noexcept {
        if (this->surface && this->handle)
            SDL_Vulkan_DestroySurface(instance, this->surface, nullptr);
        if (this->handle) SDL_DestroyWindow(this->handle);
    }
}  // namespace vgi
