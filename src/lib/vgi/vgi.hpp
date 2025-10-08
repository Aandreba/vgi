/*! \file */
#pragma once

#include <SDL3/SDL.h>
#include <stdexcept>

#include "memory.hpp"
#include "vulkan.hpp"

namespace vgi {
    /// @brief Initializes the environment context
    /// @param app_name Name of the application, in UTF-8
    void init(const char8_t* app_name);

    /// @brief Shuts down the environment context
    void quit() noexcept;

    /// @brief Exception class to report errors concurred by SDL
    struct sdl_error : public std::runtime_error {
        sdl_error() : std::runtime_error(SDL_GetError()) {}
    };

    namespace sdl {
        /// @brief Helper function that parses an SDL result
        /// @param res The result of an SDL function
        /// @throws `sdl_error` if `res == false`
        inline void tri(bool res) {
            if (!res) [[unlikely]]
                throw sdl_error{};
        }

        /// @brief Helper function that parses an SDL result
        /// @param res The result of an SDL function
        /// @throws `sdl_error` if `res == nullptr`
        template<class T>
        inline T* tri(T* res) {
            if (!res) [[unlikely]]
                throw sdl_error{};
            return res;
        }
    }  // namespace sdl

    namespace vkn {
        /// @brief Helper function that writes the contents of a Vulkan enumeration to a
        /// `unique_span`
        /// @tparam T Values returned by the Vulkan function
        /// @tparam F Lambda that calls the underlying Vulkan function
        /// @param f Lambda that calls the underlying Vulkan function
        /// @return The values returned by the Vulkan function
        template<class T, class F>
            requires(std::is_invocable_r_v<vk::Result, const F&, uint32_t*, T*> &&
                     std::is_trivially_copy_constructible_v<T> &&
                     std::is_trivially_destructible_v<T>)
        unique_span<T> enumerate(const F& f) {
            uint32_t count;
            vk::Result res = f(&count, nullptr);
            VGI_ASSERT(res == vk::Result::eSuccess);
            unique_span<T> buf = make_unique_span_for_overwrite<T>(count);
            res = f(&count, buf.data());
            VGI_ASSERT(res == vk::Result::eSuccess);
            return buf;
        }
    }  // namespace vkn
}  // namespace vgi
