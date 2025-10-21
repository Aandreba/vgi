#include "window.hpp"

#include <SDL3/SDL_vulkan.h>
#include <iterator>
#include <optional>
#include <ranges>

#include "cmdbuf.hpp"
#include "log.hpp"
#include "math.hpp"
#include "texture.hpp"
#include "vgi.hpp"

#define VMA_CHECK(expr) ::vk::detail::resultCheck(static_cast<::vk::Result>(expr), __FUNCTION__)

// Make sure we only setup the window to use Vulkan.
constexpr static inline const SDL_WindowFlags EXCLUDED_FLAGS = SDL_WINDOW_OPENGL | SDL_WINDOW_METAL;
constexpr static inline const SDL_WindowFlags REQUIRED_FLAGS = SDL_WINDOW_VULKAN;
// Regular, 8-bit color format.
constexpr static inline const vk::SurfaceFormatKHR SRGB_FORMAT{vk::Format::eB8G8R8A8Unorm,
                                                               vk::ColorSpaceKHR::eSrgbNonlinear};
// High definition, 10-bit color format.
constexpr static inline const vk::SurfaceFormatKHR HDR10_FORMAT{vk::Format::eA2B10G10R10UnormPack32,
                                                                vk::ColorSpaceKHR::eHdr10St2084EXT};
// Formats that can be used on a depth buffer
constexpr static inline const vk::Format DEPTH_FORMATS[] = {
        vk::Format::eD32Sfloat,      vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
        vk::Format::eD24UnormS8Uint, vk::Format::eD16Unorm,  vk::Format::eD16UnormS8Uint};

template<std::ranges::input_range R, class T, class Proj = std::identity>
    requires std::indirect_binary_predicate<
            std::ranges::equal_to, std::projected<std::ranges::iterator_t<R>, Proj>, const T*>
constexpr static bool contains(R&& r, const T& value, Proj proj = {}) {
    const auto end = std::ranges::end(r);
    return std::ranges::find(std::ranges::begin(r), end, value, std::ref(proj)) == end;
}

template<std::ranges::input_range R>
    requires(std::is_move_constructible_v<std::ranges::range_value_t<R>>)
constexpr static std::optional<std::ranges::range_value_t<R>> first(R&& r) {
    if (std::ranges::empty(r)) return std::nullopt;
    auto it = std::ranges::begin(r);
    return std::make_optional<std::ranges::range_value_t<R>>(std::move(*it));
}

namespace vgi {
    extern vk::Instance instance;

    static std::vector<std::string> enumerate_device_exts(const device& physical) {
        std::vector<vk::ExtensionProperties> props = physical->enumerateDeviceExtensionProperties();
        std::vector<std::string> result;
        result.reserve(props.size());
        for (const vk::ExtensionProperties& prop: props) {
            result.push_back(prop.extensionName);
        }
        return result;
    }

    static vk::Device create_logical_device(const device& physical, uint32_t queue_family) {
        std::vector<std::string> available_exts = enumerate_device_exts(physical);
        std::vector<const char*> extensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME});

        // Check that all enabled extensions are available
        for (const char* ext: extensions) {
            if (std::ranges::find(available_exts, ext) == available_exts.end()) {
                throw vgi_error{std::format("Required device extension '{}' is not present", ext)};
            }
        }

        // (macOS) Enable `VK_KHR_portability_subset`, if available
#if defined(__APPLE__) && defined(VK_KHR_portability_subset)
        if (std::ranges::find(available_exts, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) !=
            available_exts.end()) {
            extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }
#endif

        // Enable all required features
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features>
                features;

        features.get<vk::PhysicalDeviceVulkan13Features>() = {
                .synchronization2 = vk::True,
                .dynamicRendering = vk::True,
        };

        constexpr const float priority = 1.0f;
        vk::DeviceQueueCreateInfo queue_info{
                .queueFamilyIndex = queue_family,
                .queueCount = 1,
                .pQueuePriorities = &priority,
        };

        std::optional<uint32_t> extension_count = math::check_cast<uint32_t>(extensions.size());
        if (!extension_count) throw vgi_error{"too many device extensions"};

        return physical->createDevice(vk::DeviceCreateInfo{
                .pNext = &features.get<vk::PhysicalDeviceFeatures2>(),
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &queue_info,
                .enabledExtensionCount = extension_count.value(),
                .ppEnabledExtensionNames = extensions.data(),
        });
    }

    static VmaAllocator create_allocator(const device& physical, vk::Device logical) {
        VmaVulkanFunctions vk_fns{
                .vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr,
        };

        VmaAllocatorCreateInfo create_info{
                .physicalDevice = physical,
                .device = logical,
                .pVulkanFunctions = &vk_fns,
                .instance = instance,
                .vulkanApiVersion = VK_API_VERSION_1_3,
        };

        VmaAllocator allocator = VK_NULL_HANDLE;
        VMA_CHECK(vmaCreateAllocator(&create_info, &allocator));
        VGI_ASSERT(allocator != VK_NULL_HANDLE);
        return allocator;
    }

    void window::create_swapchain(uint32_t width, uint32_t height, bool vsync, bool hdr10) {
        VGI_ASSERT(this->logical && this->allocator != VK_NULL_HANDLE);
        vk::SurfaceCapabilitiesKHR surface_caps = physical->getSurfaceCapabilitiesKHR(surface);

        vk::Extent2D new_swapchain_extent;
        // If width and height equals the special value 0xFFFFFFFF, the size of the surface will
        // be set by the swapchain
        if (surface_caps.currentExtent.width == UINT32_MAX &&
            surface_caps.currentExtent.height == UINT32_MAX) {
            // If the surface size is undefined, the size is set to the size of the images requested
            new_swapchain_extent.width = width;
            new_swapchain_extent.height = height;
        } else {
            // If the surface size is defined, the swap chain size must match
            new_swapchain_extent = surface_caps.currentExtent;
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
        const vk::SwapchainCreateInfoKHR new_swapchain_info{
                .surface = surface,
                .minImageCount = swapchain_image_count,
                .imageFormat = swapchain_format.format,
                .imageColorSpace = swapchain_format.colorSpace,
                .imageExtent = new_swapchain_extent,
                .imageArrayLayers = 1,
                .imageUsage =
                        vk::ImageUsageFlagBits::eColorAttachment |
                        (surface_caps.supportedUsageFlags & (vk::ImageUsageFlagBits::eTransferSrc |
                                                             vk::ImageUsageFlagBits::eTransferDst)),
                .imageSharingMode = vk::SharingMode::eExclusive,
                .queueFamilyIndexCount = 0,
                .preTransform = swapchain_transform,
                .compositeAlpha = swapchain_composite_alpha,
                .presentMode = vsync ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eMailbox,
                // Setting clipped to vk::True allows the implementation to discard
                // rendering outside of the surface area
                .clipped = vk::True,
                // Setting oldSwapChain to the saved handle of the previous swapchain aids
                // in resource reuse and makes sure that we can still present already
                // acquired images
                .oldSwapchain = this->swapchain,
        };

        vk::UniqueSwapchainKHR new_swapchain = logical.createSwapchainKHRUnique(new_swapchain_info);
        std::vector<vk::Image> new_swapchain_images =
                this->logical.getSwapchainImagesKHR(new_swapchain.get());

        std::vector<vk::UniqueImageView> new_swapchain_views;
        new_swapchain_views.reserve(new_swapchain_images.size());
        std::vector<vk::UniqueSemaphore> new_render_complete;
        new_render_complete.reserve(new_swapchain_images.size());
        std::vector<depth_texture> new_swapchain_depths;
        new_swapchain_depths.reserve(new_swapchain_images.size());

        command_buffer cmdbuf{*this};
        try {
            for (size_t i = 0; i < new_swapchain_images.size(); ++i) {
                new_swapchain_views.push_back(logical.createImageViewUnique(vk::ImageViewCreateInfo{
                        .image = new_swapchain_images[i],
                        .viewType = vk::ImageViewType::e2D,
                        .format = swapchain_format.format,
                        .components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
                                       vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA},
                        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                             .baseMipLevel = 0,
                                             .levelCount = 1,
                                             .baseArrayLayer = 0,
                                             .layerCount = 1},
                }));
                new_render_complete.push_back(logical.createSemaphoreUnique({}));

                depth_texture& depth = new_swapchain_depths.emplace_back(
                        this->logical, this->allocator, this->depth_format,
                        new_swapchain_extent.width, new_swapchain_extent.height);

                // Change the layout of the depth image to the required one
                change_layout(cmdbuf, depth.image, vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eDepthStencilAttachmentOptimal,
                              vk::PipelineStageFlagBits::eTopOfPipe,
                              vk::PipelineStageFlagBits::eEarlyFragmentTests,
                              vk::ImageAspectFlagBits::eDepth);
            }
            std::move(cmdbuf).submit_and_wait();

            // If an existing swap chain is re-created, destroy the old swap chain and the
            // ressources owned by the application (image views, images are owned by the swapchain)
            if (this->swapchain) {
                this->logical.waitIdle();

                for (depth_texture& depth: this->swapchain_depths) {
                    std::move(depth).destroy(this->logical, this->allocator);
                }
                for (vk::ImageView view: this->swapchain_views) {
                    this->logical.destroyImageView(view);
                }
                for (vk::Semaphore sem: this->render_complete) {
                    this->logical.destroySemaphore(sem);
                }
                this->logical.destroySwapchainKHR(this->swapchain);
            }
        } catch (...) {
            for (depth_texture& depth: new_swapchain_depths) {
                std::move(depth).destroy(this->logical, this->allocator);
            }
            throw;
        }

        this->swapchain = new_swapchain.release();
        this->swapchain_images = std::move(new_swapchain_images);
        this->swapchain_info = new_swapchain_info;
        this->swapchain_views = new_swapchain_views |
                                std::views::transform(std::mem_fn(&vk::UniqueImageView::release));
        this->swapchain_depths = new_swapchain_depths;
        this->render_complete = new_render_complete |
                                std::views::transform(std::mem_fn(&vk::UniqueSemaphore::release));
    }

    void window::create_swapchain(bool vsync, bool hdr10) {
        VGI_ASSERT(this->handle);
        int w, h;
        sdl::tri(SDL_GetWindowSizeInPixels(this->handle, &w, &h));
        std::optional<uint32_t> width = math::check_cast<uint32_t>(w);
        std::optional<uint32_t> height = math::check_cast<uint32_t>(h);
        if (!w || !h) throw vgi_error{"invalid window size"};
        return create_swapchain(width.value(), height.value(), vsync, hdr10);
    }

    window::depth_texture::depth_texture(vk::Device logical, VmaAllocator allocator,
                                         vk::Format format, uint32_t width, uint32_t height) {
        VkImageCreateInfo create_info = vk::ImageCreateInfo{
                .imageType = vk::ImageType::e2D,
                .format = format,
                .extent = {width, height, UINT32_C(1)},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                .sharingMode = vk::SharingMode::eExclusive,
        };

        VmaAllocationCreateInfo alloc_create_info{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
        VkImage image = VK_NULL_HANDLE;
        VMA_CHECK(vmaCreateImage(allocator, &create_info, &alloc_create_info, &image,
                                 &this->allocation, nullptr));
        this->image = image;

        vk::ImageViewCreateInfo view_create_info{
                .image = this->image,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
                               vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA},
                .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
                                     .levelCount = 1,
                                     .layerCount = 1},
        };
        this->view = logical.createImageView(view_create_info);
    }

    void window::depth_texture::destroy(vk::Device logical, VmaAllocator allocator) && noexcept {
        if (this->view) logical.destroyImageView(this->view);
        vmaDestroyImage(allocator, this->image, this->allocation);
    }

    window::window(const device& device, const char8_t* title, int width, int height,
                   SDL_WindowFlags flags, bool vsync, bool hdr10) :
        handle(sdl::tri(SDL_CreateWindow(reinterpret_cast<const char*>(title), width, height,
                                         (flags & ~EXCLUDED_FLAGS) | REQUIRED_FLAGS))),
        physical(device) {
        // Find depth format
        auto depth_formats =
                device.supported_formats(std::span<const vk::Format>{DEPTH_FORMATS},
                                         vk::FormatFeatureFlagBits::eDepthStencilAttachment);
        if (std::ranges::empty(depth_formats))
            throw vgi_error{"Device does not support depth textures"};
        this->depth_format = *std::ranges::begin(depth_formats);

        // Create surface
        VkSurfaceKHR surface;
        sdl::tri(SDL_Vulkan_CreateSurface(this->handle, static_cast<VkInstance>(instance), nullptr,
                                          &surface));
        VGI_ASSERT(surface != VK_NULL_HANDLE);
        this->surface = surface;

        std::optional<uint32_t> queue_family = device.select_queue_family(
                this->surface, hdr10 ? HDR10_FORMAT : SRGB_FORMAT, vsync);
        if (!queue_family) throw vgi_error{"No valid queue family found"};

        // Properties
        this->has_hdr10 = contains(device->getSurfaceFormatsKHR(this->surface), HDR10_FORMAT);
        this->has_mailbox = contains(device->getSurfacePresentModesKHR(this->surface),
                                     vk::PresentModeKHR::eMailbox);

        this->logical = create_logical_device(device, queue_family.value());
        this->allocator = create_allocator(device, this->logical);
        this->queue = this->logical.getQueue(queue_family.value(), 0);
        this->cmdpool = this->logical.createCommandPool(vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                .queueFamilyIndex = queue_family.value(),
        });
        this->create_swapchain(vsync, hdr10);

        // Command Buffers
        vkn::allocateCommandBuffers(this->logical,
                                    vk::CommandBufferAllocateInfo{
                                            .commandPool = this->cmdpool,
                                            .level = vk::CommandBufferLevel::ePrimary,
                                            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
                                    },
                                    this->cmdbufs);

        // Synchronization objects
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            this->in_flight[i] = this->logical.createFence(
                    vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
            this->present_complete[i] = this->logical.createSemaphore({});
        }
    }

    void window::on_event(const SDL_Event& event) {
        SDL_Window* event_window = SDL_GetWindowFromEvent(&event);
        if (event_window == nullptr || event_window == this->handle) {
            for (std::unique_ptr<layer>& s: this->layers.values()) s->on_event(*this, event);
        }
    }

    void window::on_update(const timings& ts) {
        // Use a fence to wait until the command buffer has finished execution before using it again
        while (true) {
            switch ((*this)->waitForFences(this->in_flight[this->current_frame], vk::True,
                                           UINT64_MAX)) {
                case vk::Result::eSuccess: {
                    (*this)->resetFences(this->in_flight[this->current_frame]);
                    goto acquire_image;
                }
                case vk::Result::eTimeout:
                    [[unlikely]] continue;
                default:
                    VGI_UNREACHABLE;
                    break;
            }
        }

    acquire_image:
        // Get the next swap chain image from the implementation
        // Note that the implementation is free to return the images in any order, so we must use
        // the acquire function and can't just cycle through the images/imageIndex on our own
        uint32_t current_image;
        while (true) {
            vk::ResultValue<uint32_t> result{{}, {}};
            try {
                result = (*this)->acquireNextImageKHR(this->swapchain, UINT64_MAX,
                                                      this->present_complete[this->current_frame]);
            } catch (const vk::OutOfDateKHRError&) {
                log_err("Window resizing not yet implemented");
                throw;
            } catch (...) {
                throw;
            }

            switch (result.result) {
                case vk::Result::eSuccess: {
                    current_image = result.value;
                    goto updates;
                }
                case vk::Result::eNotReady:
                case vk::Result::eTimeout: {
                    continue;
                }
                case vk::Result::eSuboptimalKHR: {
                    log_warn("Window resizing is not yet implemented");
                    break;
                }
                default:
                    VGI_UNREACHABLE;
                    break;
            }
        }

    updates:
        vk::CommandBuffer cmdbuf = this->cmdbufs[this->current_frame];
        vk::Image img = this->swapchain_images[current_image];
        vk::ImageView view = this->swapchain_views[current_image];
        depth_texture& depth = this->swapchain_depths[current_image];

        cmdbuf.reset();
        cmdbuf.begin(vk::CommandBufferBeginInfo{});
        change_layout(cmdbuf, img, vk::ImageLayout::eUndefined,
                      vk::ImageLayout::eColorAttachmentOptimal,
                      vk::PipelineStageFlagBits::eColorAttachmentOutput,
                      vk::PipelineStageFlagBits::eColorAttachmentOutput);

        // Handle transitions & updates
        for (size_t i: this->layers.keys()) {
            if (this->layers[i]->transition_target.has_value()) {
                std::unique_ptr<layer> target =
                        std::move(this->layers[i]->transition_target.value());

                this->layers[i]->on_detach(*this);
                if (target) {
                    // Swap layer
                    target->on_attach(*this);
                    this->layers[i] = std::move(target);
                } else {
                    // Remove layer
                    VGI_ASSERT(this->layers.try_remove(i));
                    continue;
                }
            }
            this->layers[i]->on_update(*this, cmdbuf, this->current_frame, ts);
        }

        // Run scene renders
        for (std::unique_ptr<layer>& s: this->layers.values()) {
            vk::RenderingAttachmentInfo color_attachment{
                    .imageView = view,
                    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eStore,
                    // TODO Per-scene clear color
                    .clearValue = {.color = {.float32 = {{0.0f, 0.0f, 0.0f, 1.0f}}}},
            };

            vk::RenderingAttachmentInfo depth_attachment{
                    .imageView = depth.view,
                    .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eDontCare,
                    // TODO Per-scene clear color
                    .clearValue = {.depthStencil = {.depth = 1.0f, .stencil = 0}},
            };

            cmdbuf.beginRendering(vk::RenderingInfo{
                    // TODO Per-scene render area
                    .renderArea = {.extent = this->swapchain_info.imageExtent},
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &color_attachment,
                    .pDepthAttachment = &depth_attachment,
                    .pStencilAttachment = nullptr,
            });

            // TODO Per-scene viewport
            cmdbuf.setViewport(0, vk::Viewport{
                                          .width = static_cast<float>(this->draw_size().width),
                                          .height = static_cast<float>(this->draw_size().height),
                                          .maxDepth = 1.0f,
                                  });

            // TODO Per-scene scissor
            cmdbuf.setScissor(0, vk::Rect2D{.extent = this->draw_size()});

            s->on_render(*this, cmdbuf, this->current_frame);
            cmdbuf.endRendering();
        }

        // Present frame to the window
        change_layout(cmdbuf, img, vk::ImageLayout::eColorAttachmentOptimal,
                      vk::ImageLayout::ePresentSrcKHR);
        cmdbuf.end();

        constexpr vk::PipelineStageFlags waitStageMask[1] = {
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
        };

        this->queue.submit(
                vk::SubmitInfo{
                        .waitSemaphoreCount = 1,
                        .pWaitSemaphores = &this->present_complete[this->current_frame],
                        .pWaitDstStageMask = waitStageMask,
                        .commandBufferCount = 1,
                        .pCommandBuffers = &cmdbuf,
                        .signalSemaphoreCount = 1,
                        .pSignalSemaphores = &this->render_complete[current_image],
                },
                this->in_flight[this->current_frame]);

        try {
            switch (this->queue.presentKHR(vk::PresentInfoKHR{
                    .waitSemaphoreCount = 1,
                    .pWaitSemaphores = &this->render_complete[current_image],
                    .swapchainCount = 1,
                    .pSwapchains = &this->swapchain,
                    .pImageIndices = &current_image,
            })) {
                case vk::Result::eSuccess:
                    break;
                case vk::Result::eSuboptimalKHR: {
                    log_warn("Window resizing is not yet implemented");
                    break;
                }
                default:
                    VGI_UNREACHABLE;
                    break;
            }
        } catch (const vk::OutOfDateKHRError&) {
            log_err("Window resizing not yet implemented");
            throw;
        } catch (...) {
            throw;
        }

        this->current_frame = math::check_add<uint32_t>(this->current_frame, 1).value_or(0) %
                              window::MAX_FRAMES_IN_FLIGHT;
    }

    std::pair<vk::Buffer, VmaAllocation VGI_RESTRICT> window::create_buffer(
            const vk::BufferCreateInfo& create_info,
            const VmaAllocationCreateInfo& alloc_create_info, VmaAllocationInfo* alloc_info) const {
        std::pair<VkBuffer, VmaAllocation> result;
        VMA_CHECK(vmaCreateBuffer(this->allocator,
                                  reinterpret_cast<const VkBufferCreateInfo*>(&create_info),
                                  &alloc_create_info, &result.first, &result.second, alloc_info));
        return result;
    }

    window::~window() noexcept {
        if (this->logical) {
            // We cannot destroy stuff being used by the device, so we must first wait for it to
            // finish everything it's working on.
            this->logical.waitIdle();

            // Destroy layers
            for (size_t i: this->layers.keys()) {
                this->layers[i]->on_detach(*this);
                VGI_ASSERT(this->layers.try_remove(i));
            }

            // Destroy internals
            for (flying_command_buffer& flying: this->flying_cmdbufs) {
                this->logical.freeCommandBuffers(this->cmdpool, flying.cmdbuf);
                this->logical.destroyFence(flying.fence);
            }
            for (depth_texture& depth: this->swapchain_depths) {
                std::move(depth).destroy(this->logical, this->allocator);
            }
            for (vk::ImageView view: this->swapchain_views) {
                this->logical.destroyImageView(view);
            }
            for (vk::Semaphore sem: this->render_complete) {
                this->logical.destroySemaphore(sem);
            }

            this->logical.freeCommandBuffers(this->cmdpool, this->cmdbufs);
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                this->logical.destroyFence(this->in_flight[i]);
                this->logical.destroySemaphore(this->present_complete[i]);
            }

            if (this->cmdpool) this->logical.destroyCommandPool(this->cmdpool);
            if (this->swapchain) this->logical.destroySwapchainKHR(this->swapchain);
            if (this->allocator) vmaDestroyAllocator(this->allocator);
            this->logical.destroy();
        }

        if (this->handle) {
            if (this->surface) SDL_Vulkan_DestroySurface(instance, this->surface, nullptr);
            SDL_DestroyWindow(this->handle);
        }
    }
}  // namespace vgi
