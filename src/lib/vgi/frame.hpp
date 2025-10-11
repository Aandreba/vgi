#pragma once

#include <chrono>

#include "window.hpp"

namespace vgi {
    struct frame {
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

        /// @brief Creates a new frame
        /// @param parent The window which will start the frame
        /// @warning Do not create multiple frames at the same time, only a single frame per window
        /// should exist simultaneously.
        frame(window& parent);

        /// @brief Casts to the `vk::CommandBuffer` being used
        constexpr operator vk::CommandBuffer() const noexcept {
            return parent.cmdbufs[parent.current_frame];
        }
        /// @brief Casts to the `VkCommandBuffer` being used
        inline operator VkCommandBuffer() const noexcept {
            return parent.cmdbufs[parent.current_frame];
        }
        /// @brief Casts to the `vk::Image` being used
        constexpr operator vk::Image() const noexcept {
            return parent.swapchain_images[this->current_image];
        }
        /// @brief Casts to the `vk::Image` being used
        inline operator VkImage() const noexcept {
            return parent.swapchain_images[this->current_image];
        }
        /// @brief Casts to the `vk::ImageView` being used
        constexpr operator vk::ImageView() const noexcept {
            return parent.swapchain_views[this->current_image];
        }
        /// @brief Casts to the `vk::ImageView` being used
        inline operator VkImageView() const noexcept {
            return parent.swapchain_views[this->current_image];
        }
        /// @brief Derefrences the `vk::CommandBuffer` being currently used
        constexpr const vk::CommandBuffer* operator->() const noexcept {
            return &parent.cmdbufs[parent.current_frame];
        }
        /// @brief Derefrences the `vk::CommandBuffer` being currently used
        constexpr const vk::CommandBuffer& operator*() const noexcept {
            return parent.cmdbufs[parent.current_frame];
        }

        ~frame() noexcept(false);

    private:
        window& parent;
        uint32_t current_image;
    };
}  // namespace vgi
