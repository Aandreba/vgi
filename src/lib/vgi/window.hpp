/*! \file */
#pragma once

#include <SDL3/SDL.h>
#include <cstdint>
#include <vulkan/vulkan.hpp>

namespace vgi {
    /// @brief Handle to a presentable window
    struct window {
        /// @brief Create a window with the specified properties
        /// @param title The title of the window, in UTF-8 encoding.
        /// @param width The width of the window
        /// @param height The height of the window
        /// @param flags The flags that determine the properties and behaviour of the window
        window(const char8_t* title, int width, int height, SDL_WindowFlags flags = 0);

        /// @brief Checks whether the windows has the provided identifier
        /// @details This is necessary to map window events to a specific `window` object.
        /// @param id a window identifier
        /// @return `true` if the window is identified by `id`, `false` otherwise
        inline bool operator==(SDL_WindowID id) const noexcept {
            return SDL_GetWindowID(this->handle) == id;
        }

        ~window() noexcept;

    private:
        SDL_Window* handle;
        vk::SurfaceKHR surface;
    };
}  // namespace vgi
