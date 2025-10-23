#include "texture.hpp"

#include <SDL3_image/SDL_image.h>
#include <vgi/io.hpp>
#include <vgi/math.hpp>

#include "window.hpp"

constexpr static vk::Format sdl_to_vk_format(SDL_PixelFormat format,
                                             vk::ColorSpaceKHR output_colorspace) noexcept {
    switch (format) {
        case SDL_PIXELFORMAT_RGBA64_FLOAT:
            return vk::Format::eR16G16B16A16Sfloat;
        case SDL_PIXELFORMAT_ABGR2101010:
            return vk::Format::eA2B10G10R10UnormPack32;
        case SDL_PIXELFORMAT_ARGB8888:
            if (output_colorspace == vk::ColorSpaceKHR::eExtendedSrgbLinearEXT) {
                return vk::Format::eB8G8R8A8Srgb;
            }
            return vk::Format::eB8G8R8A8Unorm;
        case SDL_PIXELFORMAT_ABGR8888:
            if (output_colorspace == vk::ColorSpaceKHR::eExtendedSrgbLinearEXT) {
                return vk::Format::eR8G8B8A8Srgb;
            }
            return vk::Format::eR8G8B8A8Unorm;
        case SDL_PIXELFORMAT_INDEX8:
            return vk::Format::eR8Unorm;
        case SDL_PIXELFORMAT_YUY2:
            return vk::Format::eG8B8G8R8422Unorm;
        case SDL_PIXELFORMAT_UYVY:
            return vk::Format::eB8G8R8G8422Unorm;
        default:
            return vk::Format::eUndefined;
    }
}

constexpr static SDL_PixelFormat vk_to_sdl_format(vk::Format format) noexcept {
    switch (format) {
        case vk::Format::eR16G16B16A16Sfloat:
            return SDL_PIXELFORMAT_RGBA64_FLOAT;
        case vk::Format::eA2B10G10R10UnormPack32:
            return SDL_PIXELFORMAT_ABGR2101010;
        case vk::Format::eB8G8R8A8Srgb:
        case vk::Format::eB8G8R8A8Unorm:
            return SDL_PIXELFORMAT_ARGB8888;
        case vk::Format::eR8G8B8A8Srgb:
        case vk::Format::eR8G8B8A8Unorm:
            return SDL_PIXELFORMAT_ABGR8888;
        case vk::Format::eR8Unorm:
            return SDL_PIXELFORMAT_INDEX8;
        case vk::Format::eG8B8G8R8422Unorm:
            return SDL_PIXELFORMAT_YUY2;
        case vk::Format::eB8G8R8G8422Unorm:
            return SDL_PIXELFORMAT_UYVY;
        default:
            return SDL_PIXELFORMAT_UNKNOWN;
    }
}

constexpr static size_t bytes_per_pixel(vk::Format format) noexcept {
    switch (format) {
        case vk::Format::eR8Unorm:
            return 1;
        case vk::Format::eR8G8Unorm:
            return 2;
        case vk::Format::eR16G16Unorm:
            return 4;
        case vk::Format::eB8G8R8A8Srgb:
        case vk::Format::eB8G8R8A8Unorm:
        case vk::Format::eA2R10G10B10UnormPack32:
            return 4;
        case vk::Format::eR16G16B16A16Sfloat:
            return 8;
        default:
            return 4;
    }
}

namespace vgi {
    using namespace priv;

    surface::surface(int width, int height, SDL_PixelFormat format) :
        handle(sdl::tri(SDL_CreateSurface(width, height, format))) {}

    surface::surface(const std::filesystem::path::value_type* path) :
        handle(sdl::tri(
                IMG_Load_IO(iostream{path, std::ios::binary | std::ios::in}.release(), true))) {}

    surface::surface(const std::filesystem::path& path) :
        handle(sdl::tri(
                IMG_Load_IO(iostream{path, std::ios::binary | std::ios::in}.release(), true))) {}

    surface surface::converted(SDL_PixelFormat format) const {
        return sdl::tri(SDL_ConvertSurface(this->handle, format));
    }

    surface surface::converted(SDL_PixelFormat format, SDL_Colorspace colorspace,
                               SDL_Palette* palette, SDL_PropertiesID props) const {
        return sdl::tri(
                SDL_ConvertSurfaceAndColorspace(this->handle, format, palette, colorspace, props));
    }

    void surface::convert(SDL_PixelFormat format) & {
        if (this->handle->format != format) {
            *this = this->converted(format);
        }
    }

    void surface::convert(SDL_PixelFormat format, SDL_Colorspace colorspace, SDL_Palette* palette,
                          SDL_PropertiesID props) & {
        if (this->handle->format != format ||
            SDL_GetSurfaceColorspace(this->handle) != colorspace) {
            *this = this->converted(format, colorspace, palette, props);
        }
    }

    surface::~surface() noexcept { SDL_DestroySurface(this->handle); }

    texture::texture(const window& parent, uint32_t width, uint32_t height, vk::Format format,
                     vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect_mask,
                     vk::SampleCountFlagBits samples, const vk::ComponentMapping& components) :
        aspect_mask(aspect_mask) {
        this->init(parent, width, height, format, usage, samples, vk::ImageLayout::eUndefined,
                   components);
    }

    texture::texture(const window& parent, vk::CommandBuffer cmdbuf, transfer_buffer& transfer,
                     const surface& surface, vk::ImageUsageFlags usage,
                     vk::SampleCountFlagBits samples, vk::ImageLayout initial_layout,
                     size_t offset) : aspect_mask(vk::ImageAspectFlagBits::eColor) {
        vk::Format format = sdl_to_vk_format(surface->format, parent.colorspace());
        if (format == vk::Format::eUndefined) throw vgi_error{"pixel format not supported"};
        vk::ComponentMapping swizzle = {
                vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity};

        std::optional<uint32_t> width = math::check_cast<uint32_t>(surface->w);
        std::optional<uint32_t> height = math::check_cast<uint32_t>(surface->h);
        std::optional<uint32_t> pitch = math::check_cast<uint32_t>(surface->pitch);
        if (!width || !height || !pitch) throw vgi_error{"invalid size"};
        this->init(parent, *width, *height, format, usage, samples, vk::ImageLayout::eUndefined,
                   swizzle);

        try {
            size_t pixel_size = bytes_per_pixel(format);
            std::optional<size_t> length = math::check_mul<size_t>(*width, pixel_size);
            if (!length) throw vgi_error{"too many pixels"};
            std::optional<size_t> size = math::check_mul<size_t>(*length, *height);
            if (!size) throw vgi_error{"too many pixels"};

            // Copy data to the transfer buffer
            const size_t start_offset = offset;
            if (*length == *pitch) {
                transfer.template write_at<std::byte>(
                        {static_cast<const std::byte*>(surface->pixels), *size}, offset);
            } else {
                const std::byte* src = static_cast<const std::byte*>(surface->pixels);
                length = (std::min<size_t>) (*length, *pitch);
                for (size_t row = *height; row--;) {
                    offset = transfer.template write_at<std::byte>({src, *length}, offset);
                    src += *pitch;
                }
            }

            // Record commands
            this->change_layout(cmdbuf, vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eTransferDstOptimal,
                                vk::PipelineStageFlagBits::eTransfer |
                                        vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                        vk::PipelineStageFlagBits::eFragmentShader,
                                vk::PipelineStageFlagBits::eTransfer);

            cmdbuf.copyBufferToImage(
                    transfer, this->image, vk::ImageLayout::eTransferDstOptimal,
                    vk::BufferImageCopy{
                            .bufferOffset = start_offset,
                            .bufferRowLength = 0,
                            .bufferImageHeight = 0,
                            .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                 .layerCount = 1},
                            .imageOffset = {0, 0, 0},
                            .imageExtent = {*width, *height, 1},
                    });

            if (initial_layout != vk::ImageLayout::eUndefined) {
                this->change_layout(cmdbuf, vk::ImageLayout::eTransferDstOptimal, initial_layout,
                                    vk::PipelineStageFlagBits::eTransfer,
                                    vk::PipelineStageFlagBits::eAllCommands);
            }
        } catch (...) {
            std::move(*this).destroy(parent);
            throw;
        }
    }

    void texture::init(const window& parent, uint32_t width, uint32_t height, vk::Format format,
                       vk::ImageUsageFlags usage, vk::SampleCountFlagBits samples,
                       vk::ImageLayout initial_layout, const vk::ComponentMapping& components) {
        const VmaAllocationCreateInfo alloc_create_info{.usage = VMA_MEMORY_USAGE_AUTO};
        const VkImageCreateInfo create_info = vk::ImageCreateInfo{
                .imageType = vk::ImageType::e2D,
                .format = format,
                .extent = {width, height, 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = samples,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = usage | vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eTransferDst,
                .sharingMode = vk::SharingMode::eExclusive,
                .initialLayout = initial_layout,
        };

        VkImage image;
        VGI_VMA_CHECK(vmaCreateImage(parent, &create_info, &alloc_create_info, &image,
                                     &this->allocation, nullptr));
        this->image = image;
        this->view = parent->createImageView(vk::ImageViewCreateInfo{
                .image = this->image,
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
                               vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA},
                .subresourceRange = {.aspectMask = this->aspect_mask,
                                     .levelCount = 1,
                                     .layerCount = 1},
        });
    }

    size_t texture::transfer_size(const window& parent, uint32_t width, uint32_t height,
                                  SDL_PixelFormat pix_format) {
        vk::Format format = sdl_to_vk_format(pix_format, parent.colorspace());
        if (format == vk::Format::eUndefined) throw vgi_error{"format is not supported"};
        vk::DeviceSize pixel_size = bytes_per_pixel(format);
        std::optional<size_t> result = math::check_mul<size_t>(width, pixel_size);
        if (result) result = math::check_mul<size_t>(*result, height);
        if (!result) throw vgi_error{"too many pixels"};
        return *result;
    }

    size_t texture::transfer_size(const window& parent, const SDL_Surface* surface) {
        VGI_ASSERT(surface != nullptr);
        std::optional<uint32_t> width = math::check_cast<uint32_t>(surface->w);
        std::optional<uint32_t> height = math::check_cast<uint32_t>(surface->h);
        if (!width || !height) throw vgi_error{"invalid size"};
        return transfer_size(parent, *width, *height, surface->format);
    }

    void texture::destroy(const window& parent) && noexcept {
        if (this->view) parent->destroyImageView(this->view);
        vmaDestroyImage(parent, this->image, this->allocation);
    }

    vk::SamplerCreateInfo sampler_options::create_info(const window& parent) const noexcept {
        return vk::SamplerCreateInfo{
                .flags = this->flags,
                .magFilter = this->mag_filter,
                .minFilter = this->min_filter,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = this->address_mode_u,
                .addressModeV = this->address_mode_v,
                .addressModeW = this->address_mode_w,
                .mipLodBias = 0.0f,
                .anisotropyEnable = parent.device().feats().samplerAnisotropy &&
                                    this->max_anisotropy.has_value(),
                .maxAnisotropy = (std::min)(this->max_anisotropy.value_or(0.0f),
                                            parent.device().props().limits.maxSamplerAnisotropy),
                .compareEnable = this->compare_op.has_value(),
                .compareOp = this->compare_op.value_or(vk::CompareOp::eNever),
                .minLod = 0.0f,
                .maxLod = 0.0f,
                .borderColor = this->border_color,
                .unnormalizedCoordinates = this->unnormalized_coordinates,
        };
    }

    void change_layout(vk::CommandBuffer cmdbuf, vk::Image img, vk::ImageLayout old_layout,
                       vk::ImageLayout new_layout, vk::PipelineStageFlags src_stage,
                       vk::PipelineStageFlags dst_stage, vk::ImageAspectFlags img_aspect) noexcept {
        vk::ImageMemoryBarrier img_memory_barrier{.oldLayout = old_layout,
                                                  .newLayout = new_layout,
                                                  .image = img,
                                                  .subresourceRange = {
                                                          .aspectMask = img_aspect,
                                                          .levelCount = 1,
                                                          .layerCount = 1,
                                                  }};

        // Source layouts (old)
        // Source access mask controls actions that have to be finished on the old layout
        // before it will be transitioned to the new layout
        switch (old_layout) {
            case vk::ImageLayout::eUndefined:
                // Image layout is undefined (or does not matter)
                // Only valid as initial layout
                // No flags required, listed only for completeness
                img_memory_barrier.srcAccessMask = {};
                break;

            case vk::ImageLayout::ePreinitialized:
                // Image is preinitialized
                // Only valid as initial layout for linear images, preserves memory contents
                // Make sure host writes have been finished
                img_memory_barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
                break;

            case vk::ImageLayout::eColorAttachmentOptimal:
                // Image is a color attachment
                // Make sure any writes to the color buffer have been finished
                img_memory_barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
                break;

            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                // Image is a depth/stencil attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                img_memory_barrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                break;

            case vk::ImageLayout::eTransferSrcOptimal:
                // Image is a transfer source
                // Make sure any reads from the image have been finished
                img_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
                break;

            case vk::ImageLayout::eTransferDstOptimal:
                // Image is a transfer destination
                // Make sure any writes to the image have been finished
                img_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                break;

            case vk::ImageLayout::eShaderReadOnlyOptimal:
                // Image is read by a shader
                // Make sure any shader reads from the image have been finished
                img_memory_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch (new_layout) {
            case vk::ImageLayout::eTransferDstOptimal:
                // Image will be used as a transfer destination
                // Make sure any writes to the image have been finished
                img_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
                break;

            case vk::ImageLayout::eTransferSrcOptimal:
                // Image will be used as a transfer source
                // Make sure any reads from the image have been finished
                img_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
                break;

            case vk::ImageLayout::eColorAttachmentOptimal:
                // Image will be used as a color attachment
                // Make sure any writes to the color buffer have been finished
                img_memory_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
                break;

            case vk::ImageLayout::eDepthStencilAttachmentOptimal:
                // Image layout will be used as a depth/stencil attachment
                // Make sure any writes to depth/stencil buffer have been finished
                img_memory_barrier.dstAccessMask = img_memory_barrier.dstAccessMask |
                                                   vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                break;

            case vk::ImageLayout::eShaderReadOnlyOptimal:
                // Image will be read in a shader (sampler, input attachment)
                // Make sure any writes to the image have been finished
                if (!img_memory_barrier.srcAccessMask) {
                    img_memory_barrier.srcAccessMask =
                            vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
                }
                img_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        cmdbuf.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, img_memory_barrier);
    }
}  // namespace vgi
