#include "pipeline.hpp"

#include "math.hpp"
#include "vgi.hpp"

namespace vgi {
    pipeline::pipeline(const window& parent,
                       std::span<const vk::DescriptorSetLayoutBinding> bindings) {
        std::optional<uint32_t> binding_count = math::check_cast<uint32_t>(bindings.size());
        if (!binding_count) throw std::runtime_error{"too many bindings"};

        std::vector<vk::DescriptorPoolSize> pool_sizes;
        pool_sizes.reserve(bindings.size());
        for (const vk::DescriptorSetLayoutBinding& binding: bindings) {
            std::optional<uint32_t> count =
                    math::check_mul(binding.descriptorCount, window::max_frames_in_flight);
            if (!count) throw std::runtime_error{"too many bindings"};

            pool_sizes.push_back({
                    .type = binding.descriptorType,
                    .descriptorCount = count.value(),
            });
        }

        this->pool = parent->createDescriptorPool(vk::DescriptorPoolCreateInfo{
                .maxSets = window::max_frames_in_flight,
                .poolSizeCount = binding_count.value(),
                .pPoolSizes = pool_sizes.data(),
        });

        this->set_layout = parent->createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = binding_count.value(),
                .pBindings = bindings.data(),
        });

        this->pipeline_layout = parent->createPipelineLayout(vk::PipelineLayoutCreateInfo{
                .setLayoutCount = 1,
                .pSetLayouts = &this->set_layout,
        });

        const vk::DescriptorSetAllocateInfo alloc_info{
                .descriptorPool = this->pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &this->set_layout,
        };

        for (uint32_t i = 0; i < window::max_frames_in_flight; ++i) {
            vkn::allocateDescriptorSets(parent, alloc_info, &this->sets[i]);
        }
    }

    void pipeline::destroy(const window& parent) && {
        if (this->handle) parent->destroyPipeline(this->handle);
        if (this->set_layout) parent->destroyDescriptorSetLayout(this->set_layout);
        if (this->pool) parent->destroyDescriptorPool(this->pool);
    }

    graphics_pipeline::graphics_pipeline(const window& parent,
                                         const vk::GraphicsPipelineCreateInfo& create_info,
                                         std::span<const vk::DescriptorSetLayoutBinding> bindings) :
        pipeline(parent, bindings) {
        this->handle = parent->createGraphicsPipeline(nullptr, create_info).value;
    }
}  // namespace vgi
