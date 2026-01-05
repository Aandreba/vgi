#include "vgi.hpp"

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <ranges>
#include <vector>

#include "collections/slab.hpp"
#include "defs.hpp"
#include "event.hpp"
#include "fs.hpp"
#include "log.hpp"
#include "math.hpp"

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#else
#error "This project requires dynamic Vulkan library"
#endif

// Log callback functions
extern "C" {
void SDLCALL sdl_log_callback(void* userdata, int category, SDL_LogPriority priority,
                              const char* message) {
    vgi::log_level level;
    switch (priority) {
        case SDL_LOG_PRIORITY_CRITICAL:
        case SDL_LOG_PRIORITY_ERROR:
            level = vgi::log_level::error;
            break;
        case SDL_LOG_PRIORITY_WARN:
            level = vgi::log_level::warn;
            break;
        case SDL_LOG_PRIORITY_INFO:
            level = vgi::log_level::info;
            break;
        case SDL_LOG_PRIORITY_DEBUG:
            level = vgi::log_level::debug;
            break;
        case SDL_LOG_PRIORITY_VERBOSE:
        case SDL_LOG_PRIORITY_TRACE:
        default:
            level = vgi::log_level::verbose;
            break;
    }

    vgi::log_msg(level, "{}", message);
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL vulkan_log_callback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT types,
        const vk::DebugUtilsMessengerCallbackDataEXT* data, void* userdata) {
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
    using namespace collections;

    vk::Instance instance;
    slab<std::unique_ptr<system>> systems;
    std::optional<std::chrono::steady_clock::time_point> first_frame = std::nullopt;
    std::optional<std::chrono::steady_clock::time_point> last_frame = std::nullopt;
    std::span<const bool, SDL_SCANCODE_COUNT> keyboard_state{
            reinterpret_cast<const bool*>(alignof(bool)), SDL_SCANCODE_COUNT};
    bool shutdown_requested = false;

    // Sets the C++ global locale to the user prefered one.
    static void setup_locale() {
        int count;
        SDL_Locale** locales = sdl::tri(SDL_GetPreferredLocales(&count));

        try {
            for (int i = 0; i < count; ++i) {
                const SDL_Locale* info = locales[i];
                std::string locale_name = std::format("{}{}{}.UTF-8", info->language,
                                                      info->country ? "_" : "", info->country);

                std::locale locale;
                try {
                    locale = std::locale{locale_name};
                } catch (...) {
                    continue;
                }

                std::locale::global(locale);
                break;
            }
        } catch (...) {
            SDL_free(locales);
            throw;
        }
        SDL_free(locales);
    }

    // Returns a vector with the names of all the instance extensions supported by the driver
    static std::vector<std::string> enuerate_instance_extensions() {
        std::vector<vk::ExtensionProperties> ext_props =
                vk::enumerateInstanceExtensionProperties(nullptr);
        std::vector<std::string> ext_names;
        ext_names.reserve(ext_props.size());
        for (const vk::ExtensionProperties& props: ext_props) {
            ext_names.push_back(props.extensionName);
        }
        return ext_names;
    }

    // Returns a vector with the names of all the instance extensions supported by the driver
    static std::vector<std::string> enuerate_instance_layers() {
        std::vector<vk::LayerProperties> ext_props = vk::enumerateInstanceLayerProperties();
        std::vector<std::string> ext_names;
        ext_names.reserve(ext_props.size());
        for (const vk::LayerProperties& props: ext_props) {
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

    static void create_instance(const char8_t* app_name) {
        VGI_ASSERT(!instance);

        vk::InstanceCreateFlags flags;
        std::vector<const char*> extensions;
        std::vector<const char*> layers;

        // Ask SDL which extensions it requires
        Uint32 sdl_ext_count;
        const char* const* sdl_ext_ptr = sdl::tri(SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count));
        extensions.insert(extensions.end(), sdl_ext_ptr, sdl_ext_ptr + sdl_ext_count);

        // Check if all required extensions are supported
        const std::vector<std::string> available_exts = enuerate_instance_extensions();
        for (const char* ext_name: extensions) {
            if (std::ranges::find(available_exts, ext_name) == available_exts.end()) {
                throw vgi_error{
                        std::format("Required instance extension '{}' is not present", ext_name)};
            }
        }

        // Check if all required layers are supported
        const std::vector<std::string> available_layers = enuerate_instance_layers();
        for (const char* layer_name: layers) {
            if (std::ranges::find(available_layers, layer_name) == available_layers.end()) {
                throw vgi_error{
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

// Enable the validation layer if in debug mode and available.
// The validation layer sits on top of every vulkan function to check whether the arguments passed
// are valid and correct (by default, Vulkan assumes all arguments are valid and correct).
#ifndef NDEBUG
        if (!has_env(VGI_OS("VGI_NO_VALIDATION_LAYER"))) {
            const char* validation_layer_name =
                    reinterpret_cast<const char*>(u8"VK_LAYER_KHRONOS_validation");

            if (std::ranges::find(available_layers, validation_layer_name) !=
                available_layers.end()) {
                layers.push_back(validation_layer_name);
            } else {
                log_warn("The validation layer is not present, validation disabled");
            }
        }
#endif

        // Setup creation structures
        std::optional<uint32_t> extension_count = math::check_cast<uint32_t>(extensions.size());
        if (!extension_count) throw std::overflow_error{"too many instance extensions"};
        std::optional<uint32_t> layer_count = math::check_cast<uint32_t>(layers.size());
        if (!layer_count) throw std::overflow_error{"too many instance layers"};

        vk::DebugUtilsMessengerCreateInfoEXT debug_create_info = setup_logger_info();
        vk::ApplicationInfo app_info{
                .pApplicationName = reinterpret_cast<const char*>(app_name),
                .pEngineName = reinterpret_cast<const char*>(u8"Entorn VGI"),
                .apiVersion = VK_API_VERSION_1_3,
        };

        // Create Instance
        instance = vk::createInstance(vk::InstanceCreateInfo{
                .pNext = &debug_create_info,
                .flags = flags,
                .pApplicationInfo = &app_info,
                .enabledLayerCount = layer_count.value(),
                .ppEnabledLayerNames = layers.data(),
                .enabledExtensionCount = extension_count.value(),
                .ppEnabledExtensionNames = extensions.data(),
        });
    }

    void init(const char8_t* app_name) {
        // Setup default logger
        add_logger<default_logger>();
        // Initialize SDL
        sdl::tri(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD));
        try {
            // Setup SDL log functions
            SDL_SetLogOutputFunction(sdl_log_callback, nullptr);
            // Setup the global locale
            setup_locale();
            // Obtain keyboard state array
            int keystate_count;
            keyboard_state = std::span<const bool, SDL_SCANCODE_COUNT>{
                    SDL_GetKeyboardState(&keystate_count), SDL_SCANCODE_COUNT};
            VGI_ASSERT(keystate_count == SDL_SCANCODE_COUNT);
            // Load the Vulkan driver
            sdl::tri(SDL_Vulkan_LoadLibrary(nullptr));
            try {
                // Load initial Vulkan functions
                VULKAN_HPP_DEFAULT_DISPATCHER.init(reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                        SDL_Vulkan_GetVkGetInstanceProcAddr()));

                create_instance(app_name);
                try {
                    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
                } catch (...) {
                    instance.destroy();
                    throw;
                }
            } catch (...) {
                SDL_Vulkan_UnloadLibrary();
                throw;
            }
        } catch (...) {
            SDL_Quit();
            throw;
        };
    }

    static void destroy_user_event(SDL_Event& event) {
        if (event.type != custom_event_type()) return;
        const event_info* info = reinterpret_cast<event_info*>(event.user.data2);
        if (event.user.code == 0) {
            info->destructor(event.user.data1);
        } else {
            VGI_UNREACHABLE;
        }
    }

    void run() {
        while (!shutdown_requested) {
            /// Process all events that ocurred since last frame
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                try {
                    shutdown_requested |= event.type == SDL_EVENT_QUIT;
                    for (std::unique_ptr<system>& l: systems.values()) {
                        l->on_event(event);
                    }
                } catch (...) {
                    destroy_user_event(event);
                    throw;
                }
                destroy_user_event(event);
            }

            // Handle transitions & run system updates
            timings ts;
            for (size_t i: systems.keys()) {
                if (systems[i]->transition_target.has_value()) {
                    std::unique_ptr<system> target =
                            std::move(systems[i]->transition_target.value());
                    if (target) {
                        // Swap system
                        systems[i] = std::move(target);
                    } else {
                        // Remove system
                        VGI_ASSERT(systems.try_remove(i));
                        continue;
                    }
                }
                systems[i]->on_update(ts);
            }
        }
    }

    void shutdown() noexcept { shutdown_requested = true; }

    void quit() noexcept {
        systems.clear();
        if (instance) std::exchange(instance, nullptr).destroy();
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
    }

    size_t add_system(std::unique_ptr<system>&& system) {
        return systems.emplace(std::move(system));
    }
    system& get_system(size_t key) { return *systems.at(key); }

    timings::timings() {
        this->time_point = std::chrono::steady_clock::now();
        if (!first_frame) {
            VGI_ASSERT(!last_frame);
            first_frame = last_frame = this->time_point;
        }

        this->start_time = this->time_point - first_frame.value();
        this->start =
                std::chrono::duration_cast<std::chrono::duration<float>>(this->start_time).count();

        this->delta_time = this->time_point - last_frame.value();
        this->delta =
                std::chrono::duration_cast<std::chrono::duration<float>>(this->delta_time).count();

        last_frame = this->time_point;
    }
}  // namespace vgi
