#pragma once

#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>
#include <utility>

#include "buffer/transfer.hpp"
#include "cmdbuf.hpp"
#include "defs.hpp"
#include "forward.hpp"
#include "resource.hpp"
#include "vulkan.hpp"

namespace vgi {
    /// @brief An image that resides on host memory
    struct surface {
        /// @brief Allocates a new surface
        /// @param width Width of the surface
        /// @param height Height of the surface
        /// @param format Pixel format of the surface
        surface(int width, int height, SDL_PixelFormat format = SDL_PIXELFORMAT_RGBA32);

        /// @brief Loads a surface from a file
        /// @param path Path of the file
        explicit surface(const std::filesystem::path::value_type* path);

        /// @brief Loads a surface from a file
        /// @param path Path of the file
        explicit surface(const std::filesystem::path& path);

        surface(const surface&) = delete;
        surface& operator=(const surface&) = delete;

        /// @brief Move constructor
        /// @param other Object to move
        surface(surface&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}

        /// @brief Move assignment
        /// @param other Object to move
        surface& operator=(surface&& other) noexcept {
            if (this->handle == other.handle) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Copy an existing surface to a new surface of the specified format
        /// @param format The new pixel format
        /// @return The new surface
        surface converted(SDL_PixelFormat format) const;

        /// @brief Copy an existing surface to a new surface of the specified format and colorspace
        /// @param format The new pixel format
        /// @param colorspace The new colorspace
        /// @param palette An optional palette to use for indexed formats
        /// @param props An SDL_PropertiesID with additional color properties
        /// @return The new surface
        surface converted(SDL_PixelFormat format, SDL_Colorspace colorspace,
                          SDL_Palette* palette = nullptr, SDL_PropertiesID props = 0) const;

        /// @brief Converts an existing surface to the new specified format
        /// @param format The new pixel format
        void convert(SDL_PixelFormat format) &;

        /// @brief Converts an existing surface to the new specified format
        /// @param format The new pixel format
        surface&& convert(SDL_PixelFormat format) && {
            this->convert(format);
            return std::move(*this);
        }

        /// @brief Converts an existing surface to the new specified format and colorspace
        /// @param format The new pixel format
        /// @param colorspace The new colorspace
        /// @param palette An optional palette to use for indexed formats
        /// @param props An SDL_PropertiesID with additional color properties
        /// @return The new surface
        void convert(SDL_PixelFormat format, SDL_Colorspace colorspace,
                     SDL_Palette* palette = nullptr, SDL_PropertiesID props = 0) &;

        /// @brief Converts an existing surface to the new specified format and colorspace
        /// @param format The new pixel format
        /// @param colorspace The new colorspace
        /// @param palette An optional palette to use for indexed formats
        /// @param props An SDL_PropertiesID with additional color properties
        /// @return The new surface
        surface&& convert(SDL_PixelFormat format, SDL_Colorspace colorspace,
                          SDL_Palette* palette = nullptr, SDL_PropertiesID props = 0) && {
            this->convert(format, colorspace, palette, props);
            return std::move(*this);
        }

        constexpr operator SDL_Surface*() const noexcept { return this->handle; }
        constexpr SDL_Surface* operator->() const noexcept { return this->handle; }

        ~surface() noexcept;

    private:
        SDL_Surface* handle;
        surface(SDL_Surface* handle) noexcept : handle(handle) {}
    };

    struct texture {
        texture() = default;

        texture(const window& parent, vk::CommandBuffer cmdbuf, transfer_buffer& transfer,
                const surface& surface,
                vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled,
                vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1, size_t offset = 0);

        texture(const window& parent, uint32_t width, uint32_t height, vk::Format format,
                vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled,
                vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor,
                vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
                const vk::ComponentMapping& components = {});

        /// @brief Move constructor
        /// @param other Object to move
        texture(texture&& other) noexcept :
            image(std::move(other.image)), view(std::move(other.view)),
            allocation(std::exchange(other.allocation, VK_NULL_HANDLE)),
            aspect_mask(std::exchange(other.aspect_mask, {})) {}

        /// @brief Move assignment
        /// @param other Object to move
        texture& operator=(texture&& other) noexcept {
            if (this == &other) return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        inline void change_layout(
                vk::CommandBuffer cmdbuf, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                vk::PipelineStageFlags src_stage = vk::PipelineStageFlagBits::eAllCommands,
                vk::PipelineStageFlags dst_stage =
                        vk::PipelineStageFlagBits::eAllCommands) noexcept;

        constexpr operator vk::Image() const noexcept { return this->image; }
        inline operator VkImage() const noexcept { return this->image; }
        constexpr operator vk::ImageView() const noexcept { return this->view; }
        inline operator VkImageView() const noexcept { return this->view; }
        constexpr operator VmaAllocation() const noexcept { return this->allocation; }

        void destroy(const window& parent) && noexcept;

        static std::pair<texture, transfer_buffer_guard> upload(
                const window& parent, vk::CommandBuffer cmdbuf, const surface& surface,
                vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled,
                vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
                size_t min_size = 0) {
            vgi::transfer_buffer_guard transfer{
                    parent, (std::max)(transfer_size(parent, surface), min_size)};
            texture result{parent, cmdbuf, transfer, surface, usage, samples};
            return std::make_pair<texture, transfer_buffer_guard>(std::move(result),
                                                                  std::move(transfer));
        }

        static texture upload_and_wait(
                window& parent, const surface& surface,
                vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled,
                vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1) {
            vgi::command_buffer cmdbuf{parent};
            auto [tex, transfer] = upload(parent, cmdbuf, surface, usage, samples);
            try {
                std::move(cmdbuf).submit_and_wait();
            } catch (...) {
                std::move(tex).destroy(parent);
                throw;
            }
            return std::move(tex);
        }

        static size_t transfer_size(const window& parent, const SDL_Surface* surface);
        static size_t transfer_size(const window& parent, uint32_t width, uint32_t height,
                                    SDL_PixelFormat format);

        texture(const texture&) = delete;
        texture& operator=(const texture&) = delete;

    private:
        vk::Image image = nullptr;
        vk::ImageView view = nullptr;
        VmaAllocation allocation = VK_NULL_HANDLE;
        vk::ImageAspectFlags aspect_mask;

        void init(const window& parent, uint32_t width, uint32_t height, vk::Format format,
                  vk::ImageUsageFlags usage, vk::SampleCountFlagBits samples,
                  const vk::ComponentMapping& components);
    };

    using texture_guard = resource_guard<texture>;

    void change_layout(vk::CommandBuffer cmdbuf, vk::Image img, vk::ImageLayout old_layout,
                       vk::ImageLayout new_layout,
                       vk::PipelineStageFlags src_stage = vk::PipelineStageFlagBits::eAllCommands,
                       vk::PipelineStageFlags dst_stage = vk::PipelineStageFlagBits::eAllCommands,
                       vk::ImageAspectFlags img_aspect = vk::ImageAspectFlagBits::eColor) noexcept;

    inline void texture::change_layout(vk::CommandBuffer cmdbuf, vk::ImageLayout old_layout,
                                       vk::ImageLayout new_layout, vk::PipelineStageFlags src_stage,
                                       vk::PipelineStageFlags dst_stage) noexcept {
        vgi::change_layout(cmdbuf, this->image, old_layout, new_layout, src_stage, dst_stage,
                           this->aspect_mask);
    }
}  // namespace vgi
