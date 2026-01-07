#pragma once

#include <chrono>

#include "vulkan.hpp"
#include "window.hpp"

namespace vgi {
    /// @brief Temporary command buffer that performs.
    /// @warning This structure is designed to be short-lived, avoid storing it for long periods of
    /// time.
    struct command_buffer {
        /// @brief Creates a new temporary command buffer
        /// @param parent Window that will create the command buffer
        command_buffer(window& parent);

        /// @brief Casts to the underlying `vk::CommandBuffer`
        constexpr operator vk::CommandBuffer() const noexcept { return this->cmdbuf; }
        /// @brief Casts to the underlying `VkCommandBuffer`
        inline operator VkCommandBuffer() const noexcept { return this->cmdbuf; }
        /// @brief Dereferences the underlying `vk::CommandBuffer`
        constexpr const vk::CommandBuffer* operator->() const noexcept { return &this->cmdbuf; }
        /// @brief Dereferences the underlying `vk::CommandBuffer`
        constexpr const vk::CommandBuffer& operator*() const noexcept { return this->cmdbuf; }

        /// @brief Access to the command buffer's parent window
        /// @return Reference to the command buffer's parent window
        inline window& parent() const noexcept { return this->parent; }

        /// @brief Submits the command buffer to the device, begining execution of the commands
        /// @param signal_semaphores List of semaphores to be notified whenever the command buffer
        /// has completed
        void submit(std::span<const vk::Semaphore> signal_semaphores = {}) &&;

        /// @brief Submits the command buffer to the device, begining execution of the commands and
        /// waiting until they are completed.
        void submit_and_wait() &&;

        /// @brief Submits the command buffer to the device, begining execution of the commands and
        /// waiting until they are completed or the timeout is reached.
        /// @param d Timeout of the wait.
        /// @return `true` if the command buffer completed before the timeout, `false` otherwise
        template<class rep, class period>
        [[nodiscard]] bool submit_and_wait(const std::chrono::duration<rep, period>& d) && {
            std::chrono::duration<uint64_t, std::nano> timeout;
            if constexpr (math::integral<rep>) {
                timeout = math::chrono::sat_duration_cast<decltype(timeout)>(d);
            } else {
                timeout = std::chrono::duration_cast<decltype(timeout)>(d);
            }
            return std::move(*this).submit_and_wait(timeout);
        }

        ~command_buffer();
        command_buffer(const command_buffer&) = delete;
        command_buffer& operator=(const command_buffer&) = delete;

    private:
        vgi::window& parent;
        vk::CommandBuffer cmdbuf;
        vk::Fence fence;

        void raw_submit(std::span<const vk::Semaphore> signal_semaphores = {});
        bool submit_and_wait(const std::chrono::duration<uint64_t, std::nano>& d) &&;
    };
}  // namespace vgi
