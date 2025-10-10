#pragma once

#include "vulkan.hpp"
#include "window.hpp"

namespace vgi {
    struct pipeline_descriptor {
        pipeline_descriptor(const window& parent);

    private:
        vk::DescriptorPool pool;
        vk::DescriptorSet sets[window::max_frames_in_flight];
    };
}  // namespace vgi
