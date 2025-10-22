#pragma once

#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>
#include <utility>

#include "defs.hpp"
#include "forward.hpp"
#include "vulkan.hpp"

namespace vgi {
    /// @brief An image that resides on host memory
    struct surface {
        surface(int width, int height, SDL_PixelFormat format = SDL_PIXELFORMAT_RGBA32);
        explicit surface(const std::filesystem::path::value_type* path);
        explicit surface(const std::filesystem::path& path) : surface(path.c_str()) {}

        surface(const surface&) = delete;
        surface& operator=(const surface&) = delete;

        surface(surface&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}
        surface& operator=(surface&& other) noexcept {
            if (this->handle == other.handle) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        constexpr operator SDL_Surface*() const noexcept { return this->handle; }
        constexpr SDL_Surface* operator->() const noexcept { return this->handle; }

        ~surface() noexcept;

    private:
        SDL_Surface* handle;
    };

    void change_layout(vk::CommandBuffer cmdbuf, vk::Image img, vk::ImageLayout old_layout,
                       vk::ImageLayout new_layout,
                       vk::PipelineStageFlags src_stage = vk::PipelineStageFlagBits::eAllCommands,
                       vk::PipelineStageFlags dst_stage = vk::PipelineStageFlagBits::eAllCommands,
                       vk::ImageAspectFlags img_aspect = vk::ImageAspectFlagBits::eColor) noexcept;
}  // namespace vgi
