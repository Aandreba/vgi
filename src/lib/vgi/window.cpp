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

    void window::create_swapchain(uint32_t& width, uint32_t& height, bool vsync, bool hdr10) {
        VGI_ASSERT(this->logical);
        vk::SurfaceCapabilitiesKHR surface_caps = physical->getSurfaceCapabilitiesKHR(surface);

        vk::Extent2D swapchain_extent;
        // If width and height equals the special value 0xFFFFFFFF, the size of the surface will
        // be set by the swapchain
        if (surface_caps.currentExtent.width == UINT32_MAX &&
            surface_caps.currentExtent.height == UINT32_MAX) {
            // If the surface size is undefined, the size is set to the size of the images requested
            swapchain_extent.width = width;
            swapchain_extent.height = height;
        } else {
            // If the surface size is defined, the swap chain size must match
            swapchain_extent = surface_caps.currentExtent;
            width = surface_caps.currentExtent.width;
            height = surface_caps.currentExtent.height;
        }

        uint32_t swapchain_image_count = math::sat_add(surface_caps.minImageCount, UINT32_C(1));
        if (surface_caps.maxImageCount != 0)
            swapchain_image_count = std::min(swapchain_image_count, surface_caps.maxImageCount);

        vk::SurfaceTransformFlagBitsKHR swapchain_transform;
        if (surface_caps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
            swapchain_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
        } else {
            swapchain_transform = surface_caps.currentTransform;
        }

        constexpr const vk::CompositeAlphaFlagBitsKHR composite_alpha_flags[] = {
                vk::CompositeAlphaFlagBitsKHR::eOpaque, vk::CompositeAlphaFlagBitsKHR::eInherit,
                vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
                vk::CompositeAlphaFlagBitsKHR::ePostMultiplied};

        vk::CompositeAlphaFlagBitsKHR swapchain_composite_alpha =
                vk::CompositeAlphaFlagBitsKHR::eOpaque;
        for (const vk::CompositeAlphaFlagBitsKHR& flag: composite_alpha_flags) {
            if (surface_caps.supportedCompositeAlpha & flag) {
                swapchain_composite_alpha = flag;
                break;
            };
        }

        vk::SurfaceFormatKHR swapchain_format = hdr10 ? HDR10_FORMAT : SRGB_FORMAT;
        vk::UniqueSwapchainKHR new_swapchain =
                logical.createSwapchainKHRUnique(vk::SwapchainCreateInfoKHR{
                        .surface = surface,
                        .minImageCount = swapchain_image_count,
                        .imageFormat = swapchain_format.format,
                        .imageColorSpace = swapchain_format.colorSpace,
                        .imageExtent = swapchain_extent,
                        .imageArrayLayers = 1,
                        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                                      (surface_caps.supportedUsageFlags &
                                       (vk::ImageUsageFlagBits::eTransferSrc |
                                        vk::ImageUsageFlagBits::eTransferDst)),
                        .imageSharingMode = vk::SharingMode::eExclusive,
                        .queueFamilyIndexCount = 0,
                        .preTransform = swapchain_transform,
                        .compositeAlpha = swapchain_composite_alpha,
                        .presentMode =
                                vsync ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eMailbox,
                        // Setting clipped to vk::True allows the implementation to discard
                        // rendering outside of the surface area
                        .clipped = vk::True,
                        // Setting oldSwapChain to the saved handle of the previous swapchain aids
                        // in resource reuse and makes sure that we can still present already
                        // acquired images
                        .oldSwapchain = this->swapchain,
                });

        unique_span<vk::Image> new_swapchain_images =
                vkn::enumerate<vk::Image>([&](uint32_t* count, vk::Image* ptr) {
                    return this->logical.getSwapchainImagesKHR(this->swapchain, count, ptr);
                });

        unique_span<vk::ImageView> new_swapchain_views =
                make_unique_span_with(new_swapchain_images.size(), [&](size_t i) { return 1; });

        // If an existing swap chain is re-created, destroy the old swap chain and the ressources
        // owned by the application (image views, images are owned by the swa
        if (this->swapchain) {
            for (vk::ImageView view: this->swapchain_views) this->logical.destroyImageView(view);
            this->logical.destroySwapchainKHR(std::exchange(this->swapchain, nullptr));
        }
    }

    window::window(const device& device, const char8_t* title, int width, int height,
                   SDL_WindowFlags flags, bool vsync, bool hdr10) :
        handle(sdl::tri(SDL_CreateWindow(reinterpret_cast<const char*>(title), width, height,
                                         (flags & ~EXCLUDED_FLAGS) | REQUIRED_FLAGS))),
        physical(device) {
        VkSurfaceKHR surface;
        sdl::tri(SDL_Vulkan_CreateSurface(this->handle, static_cast<VkInstance>(instance), nullptr,
                                          &surface));
        VGI_ASSERT(surface != VK_NULL_HANDLE);
        this->surface = surface;

        std::optional<uint32_t> queue_family = device.select_queue_family(
                this->surface, hdr10 ? HDR10_FORMAT : SRGB_FORMAT, vsync);
        if (!queue_family) throw std::runtime_error{"No valid queue family found"};

        // Properties
        this->has_mailbox = std::ranges::includes(device->getSurfacePresentModesKHR(this->surface),
                                                  std::views::single(vk::PresentModeKHR::eMailbox));
        this->has_hdr10 = std::ranges::includes(device->getSurfaceFormatsKHR(this->surface),
                                                std::views::single(HDR10_FORMAT));

        this->logical = create_logical_device(device, queue_family.value());
        this->queue = this->logical.getQueue(queue_family.value(), 0);
        this->cmdpool = this->logical.createCommandPool(vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queue_family.value(),
        });
    }

    window::~window() noexcept {
        if (this->logical) {
            // We cannot destroy stuff being used by the device, so we must first wait for it to
            // finish everything it's working on.
            this->logical.waitIdle();
            if (this->cmdpool) this->logical.destroyCommandPool(this->cmdpool);
            if (this->swapchain) this->logical.destroySwapchainKHR(this->swapchain);
            this->logical.destroy();
        }

        if (this->handle) {
            if (this->surface) SDL_Vulkan_DestroySurface(instance, this->surface, nullptr);
            SDL_DestroyWindow(this->handle);
        }
    }
}  // namespace vgi
