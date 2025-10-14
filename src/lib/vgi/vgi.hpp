/*! \file */
#pragma once

#include <SDL3/SDL.h>
#include <optional>
#include <stdexcept>

#include "defs.hpp"
#include "memory.hpp"
#include "vulkan.hpp"

namespace vgi {
    struct window;
    struct frame;

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

    struct layer {
        virtual void on_event(const SDL_Event& event) {}
        virtual ~layer() = default;

        void transition_to(std::unique_ptr<layer>&& layer) noexcept {
            this->transition_target.emplace(std::move(layer));
        }

        template<std::derived_from<layer> T, class... Args>
            requires(std::is_constructible_v<T, Args...>)
        inline void transition_to(Args&&... args) {
            return this->transition_to(std::make_unique<T>(std::forward<Args>(args)...));
        }

        void detach() noexcept { this->transition_to(nullptr); }

    private:
        std::optional<std::unique_ptr<layer>> transition_target = std::nullopt;
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
