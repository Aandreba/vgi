/*! \file */
#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <utility>

#include "device.hpp"
#include "vulkan.hpp"

namespace vgi {
    /// @brief Handle to a presentable window
    struct window {
        /// @brief Create a window with the specified properties
        /// @param device Device to be used for hardware acceleration
        /// @param title The title of the window, in UTF-8 encoding
        /// @param width The width of the window
        /// @param height The height of the window
        /// @param vsync Specifies whether we want to limit the frame rate to follow the rate at
        /// which the monitor can present frames
        /// @param flags The flags that determine the properties and behaviour of the window
        window(const device& device, const char8_t* title, int width, int height,
               SDL_WindowFlags flags = 0, bool vsync = true);

        window(const window&) = delete;
        window& operator=(const window&) = delete;

        /// @brief Move constructor for `window`
        /// @param other Object to move
        window(window&& other) noexcept :
            handle(std::exchange(other.handle, nullptr)), surface(std::move(other.surface)),
            physical(other.physical), logical(std::move(other.logical)),
            queue(std::move(other.queue)), cmdpool(std::move(other.cmdpool)) {}

        /// @brief Move assignment for `window`
        /// @param other Object to move
        window& operator=(window&& other) noexcept {
            VGI_IF_UNLIKELY(this == &other) { return *this; }
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

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
        constexpr operator VkSurfaceKHR() const noexcept { return this->surface; }
        /// @brief Casts `window` to it's underlying `vk::Device`
        constexpr operator vk::Device() const noexcept { return this->logical; }
        /// @brief Casts `window` to it's underlying `VkDevice`
        constexpr operator VkDevice() const noexcept { return this->logical; }

        ~window() noexcept;

    private:
        SDL_Window* VGI_RESTRICT handle;
        vk::SurfaceKHR surface;
        const device& physical;
        vk::Device logical;
        vk::Queue queue;
        vk::CommandPool cmdpool;
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
