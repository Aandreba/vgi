#include "texture.hpp"

namespace vgi {
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
