#include "pipeline.hpp"

namespace vgi {
    pipeline_descriptor::pipeline_descriptor(const window& parent) :
        pool(parent->createDescriptorPool(vk::DescriptorPoolCreateInfo{})) {}
}  // namespace vgi
