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

        command_buffer(const command_buffer&) = delete;
        command_buffer& operator=(const command_buffer&) = delete;

        constexpr operator vk::CommandBuffer() const noexcept { return this->cmdbuf; }
        inline operator VkCommandBuffer() const noexcept { return this->cmdbuf; }
        constexpr const vk::CommandBuffer* operator->() const noexcept { return &this->cmdbuf; }
        constexpr const vk::CommandBuffer& operator*() const noexcept { return this->cmdbuf; }

        void submit(std::span<const vk::Semaphore> signal_semaphores = {}) &&;

        void submit_and_wait() &&;

        template<class rep, class period>
        [[nodiscard]] bool submit_and_wait(const std::chrono::duration<rep, period>& d) && {
            std::chrono::duration<uint64_t, std::nano> timeout;
            if constexpr (math::integral<rep>) {
                timeout = math::chrono::sat_duration_cast<decltype(timeout)>(d);
            } else {
                timeout = std::chrono::duration_cast<decltype(timeout)>(d);
            }
            return submit_and_wait<uint64_t, std::nano>(timeout);
        }

        ~command_buffer();

    private:
        window& parent;
        vk::CommandBuffer cmdbuf;
        vk::Fence fence;

        void raw_submit(std::span<const vk::Semaphore> signal_semaphores = {});
    };
}  // namespace vgi
