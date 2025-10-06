#include "vgi.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <ranges>
#include <vector>

#include "defs.hpp"
#include "log.hpp"
#include "math.hpp"

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#else
#error "This project requires dynamic Vulkan library"
#endif

extern "C" {
VKAPI_ATTR vk::Bool32 VKAPI_CALL vulkan_log_callback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT types,
        const vk::DebugUtilsMessengerCallbackDataEXT *data, void *userdata) {
    vgi::log_level level;
    switch (severity) {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
            level = vgi::log_level::debug;
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
            level = vgi::log_level::info;
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
            level = vgi::log_level::warn;
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
            level = vgi::log_level::error;
            break;
        default:
            level = vgi::log_level::verbose;
            break;
    }

    vgi::log_msg(level, "{}", data->pMessage);

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

    // Returns a vector with the names of all the instance extensions supported by the driver
    static std::vector<std::string> enuerate_instance_layers() {
        std::vector<vk::LayerProperties> ext_props = vk::enumerateInstanceLayerProperties();
        std::vector<std::string> ext_names;
        ext_names.reserve(ext_props.size());
        for (const vk::LayerProperties &props: ext_props) {
            ext_names.push_back(props.layerName);
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

        // Ask SDL which extensions it requires
        Uint32 sdl_ext_count;
        const char *const *sdl_ext_ptr = sdl::tri(SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count));
        extensions.insert(extensions.end(), sdl_ext_ptr, sdl_ext_ptr + sdl_ext_count);

        // Check if all required extensions are supported
        const std::vector<std::string> available_exts = enuerate_instance_extensions();
        for (const char *ext_name: extensions) {
            if (std::ranges::find(available_exts, ext_name) == available_exts.end()) {
                throw std::runtime_error{
                        std::format("Required instance extension '{}' is not present", ext_name)};
            }
        }

        // Check if all required layers are supported
        const std::vector<std::string> available_layers = enuerate_instance_layers();
        for (const char *layer_name: layers) {
            if (std::ranges::find(available_layers, layer_name) == available_layers.end()) {
                throw std::runtime_error{
                        std::format("Required instance layer '{}' is not present", layer_name)};
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

        // Enable the debug utils extension if available
        if (std::ranges::find(available_exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) !=
            available_exts.end()) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            log_warn("The '{}' instance extension is not present, logging disabled.",
                     VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

// Enable the validation layer if in debug mode and available
#ifndef NDEBUG
        const char *validation_layer_name =
                reinterpret_cast<const char *>(u8"VK_LAYER_KHRONOS_validation");

        if (std::ranges::find(available_layers, validation_layer_name) != available_layers.end()) {
            layers.push_back(validation_layer_name);
        } else {
            log_warn("The validation layer is not present, validation disabled");
        }
#endif

        // Setup creation structures
        std::optional<uint32_t> extension_count = math::check_cast<uint32_t>(extensions.size());
        if (!extension_count) throw std::overflow_error{"too many instance extensions"};
        std::optional<uint32_t> layer_count = math::check_cast<uint32_t>(layers.size());
        if (!layer_count) throw std::overflow_error{"too many instance layers"};

        vk::DebugUtilsMessengerCreateInfoEXT debug_create_info = setup_logger_info();

        vk::ApplicationInfo app_info{
                .pApplicationName = reinterpret_cast<const char *>(app_name),
                .pEngineName = reinterpret_cast<const char *>(u8"Entorn VGI"),
                .apiVersion = VK_API_VERSION_1_3,
        };

        // Create Instance
        instance = vk::createInstance({
                .pNext = &debug_create_info,
                .flags = flags,
                .pApplicationInfo = &app_info,
        });
    }

    void init(const char8_t *app_name) {
        // Initialize SDL
        sdl::tri(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD));
        try {
            // Load the Vulkan driver
            sdl::tri(SDL_Vulkan_LoadLibrary(nullptr));
            try {
                // Load initial Vulkan functions
                VULKAN_HPP_DEFAULT_DISPATCHER.init(reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                        SDL_Vulkan_GetVkGetInstanceProcAddr()));
                create_instance(app_name);
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
