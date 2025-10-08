#include "window.hpp"

#include <SDL3/SDL_vulkan.h>
#include <ranges>

#include "log.hpp"
#include "math.hpp"
#include "vgi.hpp"

// Make sure we only setup the window to use Vulkan.
constexpr static inline const SDL_WindowFlags EXCLUDED_FLAGS = SDL_WINDOW_OPENGL | SDL_WINDOW_METAL;
constexpr static inline const SDL_WindowFlags REQUIRED_FLAGS = SDL_WINDOW_VULKAN;
// Regular, 8-bit color format.
constexpr static inline const vk::SurfaceFormatKHR SRGB_FORMAT{vk::Format::eB8G8R8A8Unorm,
                                                               vk::ColorSpaceKHR::eSrgbNonlinear};
// High definition, 10-bit color format.
constexpr static inline const vk::SurfaceFormatKHR HDR10_FORMAT{vk::Format::eA2B10G10R10UnormPack32,
                                                                vk::ColorSpaceKHR::eHdr10St2084EXT};

namespace vgi {
    extern vk::Instance instance;

    static std::vector<std::string> enumerate_device_exts(const device& physical) {
        std::vector<vk::ExtensionProperties> props = physical->enumerateDeviceExtensionProperties();
        std::vector<std::string> result;
        result.reserve(props.size());
        for (const vk::ExtensionProperties& prop: props) {
            result.emplace_back(prop.extensionName);
        }
        return result;
    }

    static vk::Device create_logical_device(const device& physical, uint32_t queue_family) {
        std::vector<std::string> available_exts = enumerate_device_exts(physical);
        std::vector<const char*> extensions{{VK_KHR_SWAPCHAIN_EXTENSION_NAME}};

        // Check that all enabled extensions are available
        for (const char* ext: extensions) {
            if (std::ranges::find(available_exts, ext) == available_exts.end()) {
                throw std::runtime_error{
                        std::format("Required device extension '{}' is not present", ext)};
            }
        }

        // (macOS) Enable `VK_KHR_portability_subset`, if available
#if defined(__APPLE__) && defined(VK_KHR_portability_subset)
        if (std::ranges::find(available_exts, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) !=
            available_exts.end()) {
            extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }
#endif

        constexpr const float priority = 1.0f;
        vk::DeviceQueueCreateInfo queue_info{
                .queueFamilyIndex = queue_family,
                .queueCount = 1,
                .pQueuePriorities = &priority,
        };

        std::optional<uint32_t> extension_count = math::check_cast<uint32_t>(extensions.size());
        if (!extension_count) throw std::runtime_error{"too many device extensions"};

        return physical->createDevice(vk::DeviceCreateInfo{
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &queue_info,
                .enabledExtensionCount = extension_count.value(),
                .ppEnabledExtensionNames = extensions.data(),
        });
    }

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

        this->logical = create_logical_device(device, queue_family.value());
        this->queue = this->logical.getQueue(queue_family.value(), 0);
        this->cmdpool = this->logical.createCommandPool(vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queue_family.value(),
        });
    }

    window::~window() noexcept {
        if (this->logical) {
            // We cannot destroy stuff being used by the device, so we must first wait for the
            // device to finish everything it's working on.
            this->logical.waitIdle();
            if (this->cmdpool) this->logical.destroyCommandPool(this->cmdpool);
            this->logical.destroy();
        }

        if (this->handle) {
            if (this->surface) SDL_Vulkan_DestroySurface(instance, this->surface, nullptr);
            SDL_DestroyWindow(this->handle);
        }
    }
}  // namespace vgi
