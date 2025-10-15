/*! \file */
#pragma once

#include <SDL3/SDL.h>
#include <optional>
#include <stdexcept>

#include "defs.hpp"
#include "forward.hpp"
#include "memory.hpp"
#include "vulkan.hpp"

namespace vgi {
    /// @brief Initializes the environment context
    /// @param app_name Name of the application, in UTF-8
    void init(const char8_t* app_name);

    /// @brief Starts running the main loop.
    void run(const window& window);

    /// @brief Requests the main loop runner to stop execution as soon as possible
    void shutdown() noexcept;

    /// @brief Shuts down the environment context
    void quit() noexcept;

    /// @brief Exception class to report errors concurred by SDL
    struct sdl_error : public std::runtime_error {
        sdl_error() : std::runtime_error(SDL_GetError()) {}
    };

    /// @brief Represents the time intervals of the current update iteration
    struct timings {
        /// @brief Time point at which the frame started
        std::chrono::steady_clock::time_point time_point;
        /// @brief Time elapsed since the beginning of the first frame
        std::chrono::steady_clock::duration start_time;
        /// @brief Time elapsed since the beginning of the last frame
        std::chrono::steady_clock::duration delta_time;
        /// @brief Seconds elapsed since the beginning of the first frame
        float start;
        /// @brief Seconds elapsed since the beginning of the last frame
        float delta;

    private:
        timings();
    };

    struct layer_base {
        virtual ~layer_base() = default;

        /// @brief At the end of this frame, replace this layer with a new one
        /// @param layer Layer that will replace the current one
        void transition_to(std::unique_ptr<layer_base>&& layer) noexcept {
            this->transition_target.emplace(std::move(layer));
        }

        /// @brief At the end of this frame, replace this layer with a new one
        /// @tparam ...Args Argument types
        /// @tparam T Layer type
        /// @param ...args Arguments to create the new layer
        template<std::derived_from<layer_base> T, class... Args>
            requires(std::is_constructible_v<T, Args...>)
        inline void transition_to(Args&&... args) {
            return this->transition_to(std::make_unique<T>(std::forward<Args>(args)...));
        }

        /// @brief At the end of this frame, detach the current layer
        void detach() noexcept { this->transition_to(nullptr); }

    protected:
        virtual void on_event(const SDL_Event& event) = 0;
        virtual void on_update(const timings& ts) = 0;
        virtual void on_render(const timings& ts) = 0;

    private:
        std::optional<std::unique_ptr<layer_base>> transition_target = std::nullopt;
    };

    template<class T>
    concept is_layer = true;

    template<class Derived>
    struct layer : public layer_base {
        virtual void on_event(const SDL_Event& event) {}
        virtual void on_update(const timings& ts) {}
        virtual void on_render(const timings& ts) {}
        virtual ~layer() = default;
    };

    namespace sdl {
        /// @brief Helper function that parses an SDL result
        /// @param res The result of an SDL function
        /// @throws `sdl_error` if `res == false`
        constexpr VGI_FORCEINLINE void tri(bool res) {
            if (!res) [[unlikely]]
                throw sdl_error{};
        }

        /// @brief Helper function that parses an SDL result
        /// @param res The result of an SDL function
        /// @throws `sdl_error` if `res == nullptr`
        template<class T>
        constexpr VGI_FORCEINLINE T* tri(T* res) {
            if (!res) [[unlikely]]
                throw sdl_error{};
            return res;
        }
    }  // namespace sdl

    namespace vkn {
        VGI_FORCEINLINE void allocateCommandBuffers(vk::Device device,
                                                    const vk::CommandBufferAllocateInfo& alloc_info,
                                                    vk::CommandBuffer* cmdbufs) {
            vk::detail::resultCheck(device.allocateCommandBuffers(&alloc_info, cmdbufs),
                                    VULKAN_HPP_NAMESPACE_STRING "::Device::allocateCommandBuffers");
        }

        VGI_FORCEINLINE void allocateDescriptorSets(vk::Device device,
                                                    const vk::DescriptorSetAllocateInfo& alloc_info,
                                                    vk::DescriptorSet* cmdbufs) {
            vk::detail::resultCheck(device.allocateDescriptorSets(&alloc_info, cmdbufs),
                                    VULKAN_HPP_NAMESPACE_STRING "::Device::allocateDescriptorSets");
        }
    }  // namespace vkn
}  // namespace vgi
