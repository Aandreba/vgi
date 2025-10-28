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
        /// @param type Filename extension that represent this data ("BMP", "GIF", "PNG", etc)
        explicit surface(const std::filesystem::path::value_type* path, const char* type = nullptr);

        /// @brief Loads a surface from a file
        /// @param path Path of the file
        /// @param type Filename extension that represent this data ("BMP", "GIF", "PNG", etc)
        explicit surface(const std::filesystem::path& path, const char* type = nullptr);

        /// @brief Loads a surface from a byte array
        /// @param bytes Byte data of the image
        /// @param type Filename extension that represent this data ("BMP", "GIF", "PNG", etc)
        explicit surface(std::span<const std::byte> bytes, const char* type = nullptr);

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

        /// @brief Casts to the underlying `SDL_Surface*`
        constexpr operator SDL_Surface*() const noexcept { return this->handle; }
        /// @brief Provides access to the underlying `SDL_Surface*`
        constexpr SDL_Surface* operator->() const noexcept { return this->handle; }

        ~surface() noexcept;

    private:
        SDL_Surface* handle;
        surface(SDL_Surface* handle) noexcept : handle(handle) {}
    };

    /// @brief An image that is stored and accessed by the device
    struct texture {
        /// @brief Default constructor
        texture() = default;

        /// @brief Creates a new texture
        /// @param parent Window that will create the texture
        /// @param width Width of the texture
        /// @param height Height of the texture
        /// @param format The format and type of the texel blocks that will be contained in the
        /// image
        /// @param usage Bitmask of describing the intended usage of the image
        /// @param aspect_mask Bitmask specifying which aspect(s) of the image are included in the
        /// view
        /// @param samples Value specifying the number of samples per texel
        /// @param components A remapping of color components (or of depth or stencil components
        /// after they have been converted into color components)
        texture(const window& parent, uint32_t width, uint32_t height, vk::Format format,
                vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled,
                vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor,
                vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
                const vk::ComponentMapping& components = {});

        /// @brief Creates a new texture by setting up an upload of a surface to it
        /// @param parent Window that will create the texture
        /// @param cmdbuf Command buffer used to register commands
        /// @param transfer Transfer buffer used to move the data from host to device memory
        /// @param surface Surface that will be uploaded to the device
        /// @param usage Bitmask of describing the intended usage of the image
        /// @param samples Value specifying the number of samples per texel
        /// @param initial_layout The initial layout of all image subresources of the image
        /// @param offset Offset into the transfer buffer where the surface will begin to be written
        texture(const window& parent, vk::CommandBuffer cmdbuf, transfer_buffer& transfer,
                const surface& surface,
                vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled,
                vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
                vk::ImageLayout initial_layout = vk::ImageLayout::eShaderReadOnlyOptimal,
                size_t offset = 0);

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

        /// @brief Casts to the underlying `vk::Image`
        constexpr operator vk::Image() const noexcept { return this->image; }
        /// @brief Casts to the underlying `VkImage`
        inline operator VkImage() const noexcept { return this->image; }
        /// @brief Casts to the underlying `vk::ImageView`
        constexpr operator vk::ImageView() const noexcept { return this->view; }
        /// @brief Casts to the underlying `VkImageView`
        inline operator VkImageView() const noexcept { return this->view; }
        /// @brief Casts to the underlying `VmaAllocation`
        constexpr operator VmaAllocation() const noexcept { return this->allocation; }

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        void destroy(const window& parent) && noexcept;

        /// @brief Creates a new texture, creates a transfer buffer with the required size and sets
        /// up the command for the upload
        /// @param parent Window that will create the mesh
        /// @param cmdbuf Command buffer used to register commands
        /// @param surface The surface to be uploaded
        /// @param usage Bitmask of describing the intended usage of the image
        /// @param samples Value specifying the number of samples per texel
        /// @param min_size The minimum size of the transfer buffer
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

        /// @brief Creates a texture and uploads the surface data to the device, waiting for the
        /// upload to complete.
        /// @param parent Window that will create the mesh
        /// @param surface The surface to be uploaded
        /// @param usage Bitmask of describing the intended usage of the image
        /// @param samples Value specifying the number of samples per texel
        /// @warning If possible, avoid using this method and instead upload multiple meshes
        /// simultaneously.
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

        /// @brief Computes the minimum remaining space a transfer buffer must have to transfer the
        /// data to/from the texture
        /// @param parent Window that will create the texture
        /// @param surface The surface to be uploaded
        /// @return The minimum remaining space a transfer buffer must have to transfer the data
        /// to/from the texture
        static size_t transfer_size(const window& parent, const SDL_Surface* surface);

        /// @brief Computes the minimum remaining space a transfer buffer must have to transfer the
        /// data to/from the texture
        /// @param parent Window that will create the texture
        /// @param width Width of the desired texture
        /// @param height Height of the desired texture
        /// @param format Pixel format of the desired texture
        /// @return The minimum remaining space a transfer buffer must have to transfer the data
        /// to/from the texture
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
                  vk::ImageLayout initial_layout, const vk::ComponentMapping& components);
    };
    using texture_guard = resource_guard<texture>;

    /// @brief Options used to create a sampler
    struct sampler_options {
        /// @brief Bitmask describing additional parameters of the sampler
        vk::SamplerCreateFlagBits flags = {};
        /// @brief The magnification filter to apply to lookups
        vk::Filter mag_filter = vk::Filter::eLinear;
        /// @brief The minification filter to apply to lookups
        vk::Filter min_filter = vk::Filter::eLinear;
        /// @brief The addressing mode for U coordinates outside [0,1)
        vk::SamplerAddressMode address_mode_u = vk::SamplerAddressMode::eRepeat;
        /// @brief The addressing mode for V coordinates outside [0,1)
        vk::SamplerAddressMode address_mode_v = vk::SamplerAddressMode::eRepeat;
        /// @brief The addressing mode for W coordinates outside [0,1)
        vk::SamplerAddressMode address_mode_w = vk::SamplerAddressMode::eRepeat;
        /// @brief Max level of sampler anisotropy, or `std::nullptr` to disable it.
        /// @details The value is clamped to the devices maximum anisotropy (usually 16). If the
        /// device doesn't support sampler anisotropy, this value is ignored.
        std::optional<float> max_anisotropy = std::numeric_limits<float>::infinity();
        /// @brief The comparison operator to apply to fetched data before filtering, or
        /// `std::nullopt` to disable comparisons
        std::optional<vk::CompareOp> compare_op = std::nullopt;
        /// @brief The predefined border color to use
        vk::BorderColor border_color = vk::BorderColor::eIntOpaqueBlack;
        /// @brief Controls whether to use unnormalized or normalized texel coordinates to address
        /// texels of the image
        bool unnormalized_coordinates = false;
        // TODO mipmaping

        /// @brief Creates information object used to create the sampler
        /// @param parent Window for which the sampler will be created
        /// @return Information to be passed to `VkCreateSampler`
        vk::SamplerCreateInfo create_info(const window& parent) const noexcept;
    };

    /// @brief A texture combined with a sampler for each possible frame in flight
    class texture_sampler : public texture {
        using samplers_type = std::array<vk::Sampler, window::MAX_FRAMES_IN_FLIGHT>;

    public:
        /// @brief Default constructor
        texture_sampler() = default;

        /// @brief Creates a new combined texture sampler with the provided texture
        /// @param parent Window that created the texture and will create the samplers
        /// @param texture Texture to combine with samplers
        /// @param options Options used to create the samplers
        texture_sampler(const window& parent, texture&& texture,
                        const sampler_options& options = {}) : texture(std::move(texture)) {
            const vk::SamplerCreateInfo create_info = options.create_info(parent);
            for (size_t i = 0; i < window::MAX_FRAMES_IN_FLIGHT; ++i) {
                this->samplers[i] = parent->createSampler(create_info);
            }
        }

        /// @brief Creates a new combined texture sampler with the provided arguments
        /// @tparam ...Args Argument type pack
        /// @param parent Window which will create both the texture and samplers
        /// @param ...args Arguments used to create the texture
        /// @param options Options used to create the samplers
        template<class... Args>
            requires(std::is_constructible_v<texture, const window&, Args...>)
        texture_sampler(const window& parent, Args&&... args, const sampler_options& options) :
            texture_sampler(parent, texture{parent, std::forward<Args>(args)...}, options) {}

        /// @brief Creates a new combined texture sampler with the provided arguments
        /// @tparam ...Args Argument type pack
        /// @param parent Window which will create both the texture and samplers
        /// @param ...args Arguments used to create the texture
        template<class... Args>
            requires(std::is_constructible_v<texture, const window&, Args...>)
        texture_sampler(const window& parent, Args&&... args) :
            texture_sampler(parent, texture{parent, std::forward<Args>(args)...}) {}

        /// @brief Move constructor
        /// @param other Object to be moved
        texture_sampler(texture_sampler&& other) noexcept :
            texture(static_cast<texture&&>(other)), samplers(std::move(other.samplers)) {}

        /// @brief Move assignment
        /// @param other Object to be moved
        texture_sampler& operator=(texture_sampler&& other) noexcept {
            if (this == &other) [[unlikely]]
                return *this;
            std::destroy_at(this);
            std::construct_at(this, std::move(other));
            return *this;
        }

        /// @brief Number of elements in the container
        constexpr uint32_t size() const noexcept { return window::MAX_FRAMES_IN_FLIGHT; }
        /// @brief Iterator to the first sampler
        constexpr samplers_type::const_iterator begin() const noexcept {
            return this->samplers.cbegin();
        }
        /// @brief Iterator past the last sampler
        constexpr samplers_type::const_iterator end() const noexcept {
            return this->samplers.cend();
        }

        /// @brief Access specified sampler
        /// @param n Index of the sampler
        /// @return A reference to a `vk::Sampler`
        inline const vk::Sampler& operator[](uint32_t n) const noexcept {
            VGI_ASSERT(n < window::MAX_FRAMES_IN_FLIGHT);
            return samplers[n];
        }

        /// @brief Creates the information used to write this combined texture sampler to a
        /// descriptor set
        /// @param index Index of the surface to use
        /// @param layout Layout that the subresources accessible from the texture will be in at the
        /// time this descriptor is accessed
        /// @return Structure specifying descriptor image information
        inline vk::DescriptorImageInfo descriptor_info(
                uint32_t index,
                vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal) const noexcept {
            VGI_ASSERT(index < window::MAX_FRAMES_IN_FLIGHT);
            return {.sampler = this->samplers[index], .imageView = *this, .imageLayout = layout};
        }

        /// @brief Destroys the resource
        /// @param parent Window that created the resource
        inline void destroy(const window& parent) && noexcept {
            for (vk::Sampler sampler: this->samplers) parent->destroySampler(sampler);
            static_cast<texture&&>(std::move(*this)).destroy(parent);
        }

    private:
        samplers_type samplers;
    };
    using texture_sampler_guard = resource_guard<texture_sampler>;

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
