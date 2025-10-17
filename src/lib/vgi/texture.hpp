#pragma once

#include "forward.hpp"
#include "vulkan.hpp"

namespace vgi {
    void change_layout(vk::CommandBuffer cmdbuf, vk::Image img, vk::ImageLayout old_layout,
                       vk::ImageLayout new_layout,
                       vk::PipelineStageFlags src_stage = vk::PipelineStageFlagBits::eAllCommands,
                       vk::PipelineStageFlags dst_stage = vk::PipelineStageFlagBits::eAllCommands,
                       vk::ImageAspectFlags img_aspect = vk::ImageAspectFlagBits::eColor) noexcept;
}  // namespace vgi
