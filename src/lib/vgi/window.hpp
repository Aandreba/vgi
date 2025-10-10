/*! \file */
#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <tuple>
#include <utility>

#include "device.hpp"
#include "vulkan.hpp"

namespace vgi {
    /// @brief Handle to a presentable window
    struct window {
        /// @brief Maximum number of frames that can waiting to be presented at the same time.
#ifdef VGI_MAX_FRAMES_IN_FLIGHT
#if VGI_MAX_FRAMES_IN_FLIGHT <= 0
#error "Max number of frames in flight must be a positive integer greater than zero"
#endif
        constexpr static inline const uint32_t MAX_FRAMES_IN_FLIGHT = VGI_MAX_FRAMES_IN_FLIGHT;
#else
        constexpr static inline const uint32_t MAX_FRAMES_IN_FLIGHT = 2;
#endif

        /// @brief Create a window with the specified properties
        /// @param device Device to be used for hardware acceleration
        /// @param title The title of the window, in UTF-8 encoding
        /// @param width The width of the window
        /// @param height The height of the window
        /// @param flags The flags that determine the properties and behaviour of the window
        /// @param vsync Specifies whether we want to limit the frame rate to follow the rate at
        /// which the monitor can present frames
        /// @param hdr10 Enables/Disables high-definition 10-bit display format
        window(const device& device, const char8_t* title, int width, int height,
               SDL_WindowFlags flags = 0, bool vsync = true, bool hdr10 = false);

        window(const window&) = delete;
        window& operator=(const window&) = delete;

        /// @brief Move constructor for `window`
        /// @param other Object to move
        window(window&& other) noexcept :
            handle(std::exchange(other.handle, nullptr)), surface(std::move(other.surface)),
            physical(other.physical), logical(std::move(other.logical)),
            queue(std::move(other.queue)), cmdpool(std::move(other.cmdpool)),
            swapchain(std::move(other.swapchain)) {}

        /// @brief Move assignment for `window`
        /// @param other Object to move
        window& operator=(window&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Color format used by the window to present images
        inline vk::Format format() const noexcept { return this->swapchain_info.imageFormat; }

        /// @brief Checks whether the windows has the provided identifier
        /// @details This is necessary to map window events to a specific `window` object.
        /// @param id a window identifier
        /// @return `true` if the window is identified by `id`, `false` otherwise
        inline bool operator==(SDL_WindowID id) const noexcept {
            return SDL_GetWindowID(this->handle) == id;
        }

        /// @brief Get access to the methods of `vk::Device`
        constexpr const vk::Device* operator->() const noexcept { return &this->logical; }
        /// @brief Get access to the methods of `vk::Device`
        constexpr vk::Device* operator->() noexcept { return &this->logical; }
        /// @brief Casts `window` to it's underlying `SDL_Window*`
        constexpr operator SDL_Window*() const noexcept { return this->handle; }
        /// @brief Casts `window` to it's underlying `vk::SurfaceKHR`
        constexpr operator vk::SurfaceKHR() const noexcept { return this->surface; }
        /// @brief Casts `window` to it's underlying `VkSurfaceKHR`
        inline operator VkSurfaceKHR() const noexcept { return this->surface; }
        /// @brief Casts `window` to it's underlying `vk::Device`
        constexpr operator vk::Device() const noexcept { return this->logical; }
        /// @brief Casts `window` to it's underlying `VkDevice`
        inline operator VkDevice() const noexcept { return this->logical; }
        /// @brief Casts `window` to it's underlying `VmaAllocator`
        constexpr operator VmaAllocator() const noexcept { return this->allocator; }

        /// @brief Creates a new buffer
        /// @param create_info Creation information for the `vk::Buffer`
        /// @param alloc_create_info Creation information fo the `VmaAllocation`
        /// @param alloc_info Information about allocated memory. It can be later fetched using
        /// function `vmaGetAllocationInfo`.
        /// @return The created `vk::Buffer` and it's associated `VmaAllocation`
        std::pair<vk::Buffer, VmaAllocation VGI_RESTRICT> create_buffer(
                const vk::BufferCreateInfo& create_info,
                const VmaAllocationCreateInfo& alloc_create_info,
                VmaAllocationInfo* alloc_info = nullptr) const;

        /// @brief Closes the window, releasing all it's resources
        void close() &&;
        ~window() noexcept;

    private:
        SDL_Window* VGI_RESTRICT handle;
        vk::SurfaceKHR surface;
        const device& physical;
        vk::Device logical;
        VmaAllocator VGI_RESTRICT allocator;
        vk::Queue queue;
        vk::CommandPool cmdpool;
        vk::SwapchainKHR swapchain;
        vk::SwapchainCreateInfoKHR swapchain_info;
        unique_span<vk::Image> swapchain_images;
        unique_span<vk::ImageView> swapchain_views;
        vk::CommandBuffer cmdbufs[MAX_FRAMES_IN_FLIGHT];
        vk::Fence in_flight[MAX_FRAMES_IN_FLIGHT];
        vk::Semaphore image_available[MAX_FRAMES_IN_FLIGHT];
        vk::Semaphore render_complete[MAX_FRAMES_IN_FLIGHT];
        bool has_mailbox;
        bool has_hdr10;

        void create_swapchain(uint32_t width, uint32_t height, bool vsync, bool hdr10);
        void create_swapchain(bool vsync, bool hdr10);
    };

}  // namespace vgi

/// @brief Checks whether the windows has the provided identifier
/// @details This is necessary to map window events to a specific `window` object.
/// @param id a window identifier
/// @param rhs a window
/// @return `true` if the window is identified by `id`, `false` otherwise
inline bool operator==(SDL_WindowID id, const vgi::window& rhs) noexcept {
    return SDL_GetWindowID(rhs) == id;
}
