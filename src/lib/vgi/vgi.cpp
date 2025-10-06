#include "vgi.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <ranges>
#include <vector>

#include "defs.hpp"
#include "log.hpp"

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#else
#error "This project requires dynamic Vulkan library"
#endif

extern "C" {
VKAPI_ATTR vk::Bool32 VKAPI_CALL vulkan_log_callback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT types,
        const vk::DebugUtilsMessengerCallbackDataEXT *data, void *userdata) {
    // The return value of this callback controls whether the Vulkan call that caused the validation
    // message will be aborted or not We return vk::False as we DON'T want Vulkan calls that cause a
    // validation message to abort If you instead want to have calls abort, pass in vk::True and the
    // function will return `vk::Result::eErrorValidationFailedEXT`
    return vk::False;
}
}

namespace vgi {
    static vk::Instance instance;

    // Returns a vector with the names of all the instance extensions supported by the driver
    static std::vector<std::string> enuerate_instance_extensions() {
        std::vector<vk::ExtensionProperties> ext_props =
                vk::enumerateInstanceExtensionProperties(nullptr);

        std::vector<std::string> ext_names;
        ext_names.reserve(ext_props.size());
        for (const vk::ExtensionProperties &props: ext_props) {
            ext_names.push_back(props.extensionName);
        }
        return ext_names;
    }

    static vk::DebugUtilsMessengerCreateInfoEXT setup_logger_info() {
        vk::DebugUtilsMessageSeverityFlagsEXT severity;
        if constexpr (max_log_level >= log_level::verbose)
            severity |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
        if constexpr (max_log_level >= log_level::debug)
            severity |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
        if constexpr (max_log_level >= log_level::info)
            severity |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
        if constexpr (max_log_level >= log_level::warn)
            severity |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
        if constexpr (max_log_level >= log_level::error)
            severity |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

        return vk::DebugUtilsMessengerCreateInfoEXT{
                .messageSeverity = severity,
                .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                               vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                               vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
                .pfnUserCallback = vulkan_log_callback,
        };
    }

    static void create_instance(const char8_t *app_name) {
        VGI_ASSERT(!instance);

        vk::InstanceCreateFlags flags;
        std::vector<const char *> extensions;
        std::vector<const char *> layers;

        // Ask SDL which extensions does it need
        Uint32 sdl_ext_count;
        const char *const *sdl_ext_ptr = sdl::tri(SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count));
        extensions.insert(extensions.end(), sdl_ext_ptr, sdl_ext_ptr + sdl_ext_count);

        // Check if all required extensions are supported
        const std::vector<std::string> available_exts = enuerate_instance_extensions();
        for (const char *ext_ptr: extensions) {
            if (std::ranges::find(available_exts, ext_ptr) == available_exts.end()) {
                throw std::runtime_error{
                        std::format("Required instance extension '{}' is not supported", ext_ptr)};
            }
        }

        // When running on iOS/macOS with MoltenVK, enable VK_KHR_portability_subset
#if defined(__APPLE__) && defined(VK_KHR_portability_enumeration)
        if (std::ranges::find(available_exts, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) !=
            available_exts.end()) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
        }
#endif

        // Setup creation structures
        vk::ApplicationInfo app_info{
                .pApplicationName = reinterpret_cast<const char *>(app_name),
                .pEngineName = reinterpret_cast<const char *>(u8"Entorn VGI"),
                .apiVersion = VK_API_VERSION_1_3,
        };

        vk::InstanceCreateInfo create_info{
                .pApplicationInfo = &app_info,
        };

        vk::DebugUtilsMessengerCreateInfoEXT debug_create_info = setup_logger_info();

        // Create Instance
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
