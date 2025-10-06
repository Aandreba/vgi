#include "vgi.hpp"

#include <SDL3/SDL_vulkan.h>

#include "defs.hpp"

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#else
#error "This project requires dynamic Vulkan library"
#endif

namespace vgi {
    static vk::Instance instance;

    static void create_instance() {
        std::vector<const char *> extensions{{VK_KHR_SURFACE_EXTENSION_NAME}};
        std::vector<const char *> layers;

        // Ask SDL which extensions does it need
        Uint32 sdl_ext_count;
        const char *const *sdl_ext_ptr = sdl::tri(SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count));
        extensions.insert(extensions.end(), sdl_ext_ptr, sdl_ext_ptr + sdl_ext_count);
    }

    void init() {
        // Initialize SDL
        sdl::tri(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD));
        try {
            // Load the Vulkan driver
            sdl::tri(SDL_Vulkan_LoadLibrary(nullptr));
            try {
                // Load initial Vulkan functions
                VULKAN_HPP_DEFAULT_DISPATCHER.init(reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                        SDL_Vulkan_GetVkGetInstanceProcAddr()));
            } catch (...) {
                SDL_Vulkan_UnloadLibrary();
                throw;
            }
        } catch (...) {
            SDL_Quit();
            throw;
        };
    }

    void quit() noexcept {
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
    }
}  // namespace vgi
